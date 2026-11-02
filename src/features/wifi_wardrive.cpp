/*
 * wifi_wardrive — channel-hopping beacon logger → WiGLE v1.6 CSV.
 *
 * Requires:
 *   - GPS fix from the M5Stack LoRa-GNSS HAT (NMEA on UART1)
 *   - SD card mounted (M5Cardputer.Display.getSDCard() or sd_mount())
 *
 * Output: /poseidon/captures/wardrive/wigle-YYYYMMDD-HHMMSS.csv with the standard
 * WiGLE CSV v1.6 header. Rows are deduped by BSSID — stronger RSSI
 * + latest GPS fix win.
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "menu.h"
#include "gps.h"
#include "../wifi_wardrive.h"
#include "../c5_cmd.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <NimBLEDevice.h>
#include <SD.h>
#include "../sd_helper.h"
#include "../argus.h"
#include "wdr_mood.h"
#include "wdr_matrix.h"
#include "../sfx.h"
#include "../sigdb_surveillance.h"
#include "../heap_budget.h"

static portMUX_TYPE s_wdr_mux = portMUX_INITIALIZER_UNLOCKED;

/* Public AP table — persists across feature exits so Triton + others can
 * seed themselves from what we've already catalogued in this session. */
wdr_ap_t *g_wdr_aps = nullptr;

/* Allocate the AP buffer on first wardrive use. Kept resident afterward
 * because triton/pmkid read it later in the session; freeing on exit would
 * corrupt those. Sessions that never wardrive keep the 8.5 KB free. */
bool wdr_aps_ensure(void)
{
    if (g_wdr_aps) return true;
    g_wdr_aps = (wdr_ap_t *)heap_caps_calloc(WARDRIVE_MAX_APS, sizeof(wdr_ap_t),
                                             MALLOC_CAP_INTERNAL);
    if (!g_wdr_aps) { ui_toast("Low memory for wardrive", T_BAD, 1500); return false; }
    return true;
}
int      g_wdr_ap_count = 0;

/* File-scope aliases for the existing internal code — keeps the diff
 * minimal. Both names refer to the same storage. */
#define s_aps      g_wdr_aps
#define s_ap_count g_wdr_ap_count
static volatile bool s_running = false;
static volatile bool s_hop_alive = false;
static volatile uint32_t s_beacons = 0;
static volatile uint8_t  s_current_ch = 1;
static volatile bool     s_c5_hold    = false;  /* true = hop task parks on ch1 for a C5 5 GHz harvest window */
static volatile int      s_5g_count = 0;   /* distinct 5 GHz APs from the C5 */
static File       s_csv;
static char       s_csv_path[80] = {0};

enum wdr_view_t { WDR_VIEW_ARGUS = 0, WDR_VIEW_PLAIN = 1, WDR_VIEW_MATRIX = 2, WDR_VIEW__COUNT = 3 };
#define MX_SEED 5   /* recent APs to preload into the matrix roster on entry */
static wdr_view_t s_view = WDR_VIEW_ARGUS;
static volatile int s_new_this_run = 0;   /* distinct new APs this session */
static volatile int s_surv_count = 0;     /* surveillance hits (Flock/Raven) */
static volatile bool s_surv_pending = false;
static char s_last_surv_name[33] = {0};
static surv_class_t s_last_surv_cls = SURV_UNKNOWN;
static uint32_t s_last_surv_ms = 0;
static uint32_t s_entry_ms = 0;           /* millis() at feature start */
static volatile uint32_t s_last_new_ms = 0;      /* millis() of most recent new AP */
static volatile bool     s_gps_ever_locked = false;
static volatile bool     s_juicy_pending = false; /* set in RX cb, consumed in UI loop */
static volatile uint32_t s_cache_rollovers = 0;
static volatile int      s_last_new_idx = -1;
static volatile bool     s_force_flush = false;
/* Experimental hybrid path is intentionally not menu-exposed until the
 * pinned SDK's stale-netif lifecycle is fixed; keep its code isolated. */
static bool              s_hybrid_enabled = false;
static bool              s_snapshot_mode = false;
static uint32_t          s_hybrid_ble_total = 0;
#define HYBRID_BLE_INTERVAL_MS 30000UL
#define HYBRID_BLE_SCAN_MS      5000UL
#define HYBRID_BLE_MIN_BLOCK    60000UL
#define HYBRID_BLE_SEEN_MAX     128
static uint8_t s_hybrid_ble_seen[HYBRID_BLE_SEEN_MAX][6];
static int     s_hybrid_ble_seen_n = 0;
struct hybrid_ble_row_t {
    uint8_t addr[6];
    char name[33];
    char stamp[16];
    int8_t rssi;
    double lat;
    double lon;
    float alt;
};
static hybrid_ble_row_t s_hybrid_ble_rows[32];
static int s_hybrid_ble_row_n = 0;
static uint8_t s_snapshot_wifi_seen[256][6];
static int s_snapshot_wifi_seen_n = 0;

static inline const char *surv_tag_str(surv_class_t cls, const char *ssid)
{
    if (cls == SURV_FLOCK_T1) return "FLOCK T1";
    if (cls == SURV_FLOCK_T2) return "FLOCK T2";
    if (cls == SURV_FLOCK_SSID) {
        if (istrstr_local(ssid, "raven")) return "RAVEN SENSOR";
        return "FLOCK CAMERA";
    }
    if (cls == SURV_FLOCK_PROBE) return "FLOCK PROBE";
    return "SURVEILLANCE";
}

static int find_ap(const uint8_t *bssid)
{
    for (int i = 0; i < s_ap_count; ++i)
        if (memcmp(s_aps[i].bssid, bssid, 6) == 0) return i;
    return -1;
}

static void format_wigle_time(const gps_fix_t &g, char out[16]);

/* WiGLE v1.6 header + metadata line */
static bool wdr_open_csv(void)
{
    gps_fix_t g;
    if (!gps_snapshot(&g)) {
        /* No GPS fix yet — write with placeholder timestamp. */
        g.utc[0] = '\0';
        g.date[0] = '\0';
    }
    if (!sd_ensure_layout()) return false;
    char stamp[16];
    format_wigle_time(g, stamp);
    if (stamp[0]) {
        snprintf(s_csv_path, sizeof(s_csv_path), SD_WARDRIVE_DIR "/wigle-%s.csv", stamp);
    } else {
        snprintf(s_csv_path, sizeof(s_csv_path), SD_WARDRIVE_DIR "/wigle-boot-%lu.csv",
                 (unsigned long)(millis() / 1000));
    }
    if (SD.exists(s_csv_path)) {
        char base[64];
        /* Leave room for "-99.csv\0" (8 bytes) so the suffix loop below
         * can never truncate/overflow s_csv_path regardless of how long
         * the timestamp-derived base name is. */
        strncpy(base, s_csv_path, sizeof(base) - 8);
        base[sizeof(base) - 8] = '\0';
        char *ext = strrchr(base, '.');
        if (ext) *ext = '\0';
        for (unsigned suffix = 2; suffix < 100; ++suffix) {
            snprintf(s_csv_path, sizeof(s_csv_path), "%s-%u.csv", base, suffix);
            if (!SD.exists(s_csv_path)) break;
        }
    }
    s_csv = SD.open(s_csv_path, FILE_WRITE);
    if (!s_csv) return false;

    /* WiGLE requires a pre-header meta line. */
    s_csv.println("WigleWifi-1.6,appRelease=POSEIDON," POSEIDON_VERSION ",model=M5Cardputer,release=1,device=POSEIDON,display=ST7789,board=ESP32S3,brand=M5Stack");
    s_csv.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
    s_csv.flush();
    return true;
}

static const char *auth_to_wigle(uint8_t a)
{
    switch (a) {
    case WIFI_AUTH_OPEN:          return "[ESS]";
    case WIFI_AUTH_WEP:           return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:       return "[WPA-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA2_PSK:      return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "[WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA3_PSK:      return "[WPA3-SAE-CCMP][ESS]";
    default:                      return "[ESS]";
    }
}

static void format_wigle_time(const gps_fix_t &g, char out[16])
{
    out[0] = '\0';
    if (!g.valid || strlen(g.date) < 6 || strlen(g.utc) < 6) return;
    snprintf(out, 16, "20%c%c%c%c%c%c-%c%c%c%c%c%c",
             g.date[4], g.date[5], g.date[2], g.date[3], g.date[0], g.date[1],
             g.utc[0], g.utc[1], g.utc[2], g.utc[3], g.utc[4], g.utc[5]);
}

static void csv_escape(const char *value, char *out, size_t out_size)
{
    size_t pos = 0;
    if (out_size == 0) return;
    for (const char *p = value ? value : ""; *p && pos + 2 < out_size; ++p) {
        uint8_t c = (uint8_t)*p;
        if (c < 0x20 || c >= 0x7F) c = '?';
        if (c == '"') out[pos++] = '"';
        out[pos++] = (char)c;
    }
    out[pos] = '\0';
    if (strchr(out, ',') || strchr(out, '"')) {
        size_t len = strlen(out);
        if (len + 2 < out_size) {
            memmove(out + 1, out, len + 1);
            out[0] = '"';
            out[len + 1] = '"';
            out[len + 2] = '\0';
        }
    }
}

static void flush_dirty_rows(void)
{
    if (!s_csv) return;
    /* Data-race fix: this runs on the UI task while the promisc RX callback
     * writes s_aps[]/s_ap_count under s_wdr_mux. Snapshot the count, then
     * per row briefly enter the mux to resolve the dirty/null-island
     * bookkeeping and copy the row into a local — the SD I/O (printf) is
     * done OUTSIDE the lock so the critical section stays short. */
    int n;
    portENTER_CRITICAL(&s_wdr_mux);
    n = s_ap_count;
    portEXIT_CRITICAL(&s_wdr_mux);
    for (int i = 0; i < n; ++i) {
        wdr_ap_t snap;
        bool write_row = false;
        portENTER_CRITICAL(&s_wdr_mux);
        wdr_ap_t &a = s_aps[i];
        if (a.dirty) {
            /* POS-AUDIT-208 / wifi-016: skip rows that never had a GPS fix.
             * The previous code would write lat=0.0,lon=0.0 placeholders
             * which WiGLE silently accepts but which corrupt aggregate
             * maps — Gulf of Guinea null-island clusters from missed
             * fixes. Better to drop the row entirely; the AP stays in the
             * in-RAM table for a later flush when GPS catches up. Leave
             * dirty=true so it retries on the next flush after a fix. */
            if (a.has_gps && a.first_seen_utc[0] != '\0') {
                a.dirty = false;
                snap = a;           /* copy fields to write under the lock */
                write_row = true;
            }
        }
        portEXIT_CRITICAL(&s_wdr_mux);
        if (!write_row) continue;
        char escaped_ssid[70];
        csv_escape(snap.ssid, escaped_ssid, sizeof(escaped_ssid));
        s_csv.printf("%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%s,%u,%d,%.6f,%.6f,%.1f,5,WIFI\n",
                     snap.bssid[0], snap.bssid[1], snap.bssid[2],
                     snap.bssid[3], snap.bssid[4], snap.bssid[5],
                     escaped_ssid, auth_to_wigle(snap.auth), snap.first_seen_utc,
                     snap.channel, snap.rssi, snap.lat, snap.lon, snap.alt);
    }
    s_csv.flush();
}

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *p = pkt->payload;
    if (pkt->rx_ctrl.sig_len < 40) return;
    uint8_t fc = p[0];
    uint8_t subtype = (fc >> 4) & 0xF;
    if (subtype != 0x8 && subtype != 0x5) return;

    portENTER_CRITICAL_ISR(&s_wdr_mux);
    const uint8_t *bssid = p + 16;
    int idx = find_ap(bssid);
    bool is_new_ap = false;
    if (idx < 0) {
        is_new_ap = true;
        if (s_ap_count >= WARDRIVE_MAX_APS) {
            /* The CSV is the durable capture. Reuse the oldest entry that
             * has no *valid* pending row: a dirty entry with no GPS fix
             * never gets written anyway (flush_dirty_rows drops rows with
             * !has_gps to avoid null-island), so it is safe — and
             * necessary — to evict those too. Without this, a full cache
             * indoors / before first GPS fix made every entry "dirty"
             * forever and no new AP could ever be added again: the AP
             * counter froze at WARDRIVE_MAX_APS and wardrive looked hung. */
            uint32_t oldest = UINT32_MAX;
            int victim = -1;
            for (int i = 0; i < s_ap_count; ++i) {
                if ((!s_aps[i].dirty || !s_aps[i].has_gps) && s_aps[i].last_seen < oldest) {
                    oldest = s_aps[i].last_seen;
                    victim = i;
                }
            }
            if (victim < 0) {
                /* Every entry is dirty with a real GPS-tagged row pending —
                 * ask the UI loop to flush ASAP instead of waiting up to
                 * 3s, so the table unblocks on the next tick rather than
                 * stalling captures in dense areas. */
                s_force_flush = true;
                portEXIT_CRITICAL_ISR(&s_wdr_mux);
                return;
            }
            idx = victim;
            s_cache_rollovers = s_cache_rollovers + 1;
        } else {
            idx = s_ap_count++;
        }
        memset(&s_aps[idx], 0, sizeof(wdr_ap_t));
        memcpy(s_aps[idx].bssid, bssid, 6);
        s_aps[idx].first_seen_ms = millis();
    }
    wdr_ap_t &a = s_aps[idx];
    a.last_seen = millis();
    s_beacons = s_beacons + 1;

    /* Parse tagged parameters after the fixed beacon body. The capture may
     * contain no tags or a truncated final tag, so never read a tag length
     * until both header bytes are inside the frame. */
    const uint8_t *tags = p + 36;
    int tag_len = pkt->rx_ctrl.sig_len - 36 - 4;  /* minus FCS */

    /* Channel is current hop. */
    a.channel = s_current_ch;

    /* Capability bits: WEP is bit 4, plus RSN/WPA info elements for WPA/2. */
    /* Quick hack: check for RSN (48) or WPA (221) in tag list. */
    int off = 0;
    uint8_t auth = WIFI_AUTH_OPEN;
    uint16_t cap = p[34] | (p[35] << 8);
    if (cap & (1 << 4)) auth = WIFI_AUTH_WEP;
    while (off + 1 < tag_len) {
        uint8_t tag = tags[off];
        uint8_t tlen = tags[off + 1];
        if (off + 2 + tlen > tag_len) break;
        if (tag == 0 && tlen <= 32) {
            memcpy(a.ssid, tags + off + 2, tlen);
            a.ssid[tlen] = '\0';
        } else if (tag == 48) {
            auth = WIFI_AUTH_WPA2_PSK;
            /* Scan the RSN element for the SAE AKM suite (00-0F-AC-08) ->
             * WPA3-Personal. Transition APs list both PSK and SAE; SAE
             * present is enough to call it WPA3 for the catch banner. */
            for (int k = 0; k + 3 < tlen; ++k) {
                if (tags[off+2+k]==0x00 && tags[off+2+k+1]==0x0F &&
                    tags[off+2+k+2]==0xAC && tags[off+2+k+3]==0x08) {
                    auth = WIFI_AUTH_WPA3_PSK; break;
                }
            }
        }
        else if (tag == 221 && tlen >= 4 && tags[off+2]==0x00 && tags[off+3]==0x50) {
            if (auth != WIFI_AUTH_WPA2_PSK) auth = WIFI_AUTH_WPA_PSK;
        }
        off += 2 + tlen;
    }
    a.auth = auth;

    if (is_new_ap) {
        s_last_new_idx = idx;
        s_new_this_run = s_new_this_run + 1;
        s_last_new_ms = millis();
        surv_class_t scls = flock_classify_oui(bssid);
        if (scls == SURV_UNKNOWN && a.ssid[0]) scls = flock_classify_ssid(a.ssid);
        if (scls != SURV_UNKNOWN) {
            s_surv_count = s_surv_count + 1;
            s_surv_pending = true;
            s_last_surv_cls = scls;
            s_last_surv_ms = millis();
            strncpy((char*)s_last_surv_name, a.ssid[0] ? a.ssid : surv_tag_str(scls, ""), sizeof(s_last_surv_name) - 1);
            s_last_surv_name[sizeof(s_last_surv_name) - 1] = 0;
        }
        if (auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA3_PSK) s_juicy_pending = true;
    }

    if (pkt->rx_ctrl.rssi > a.rssi || a.rssi == 0) {
        a.rssi = pkt->rx_ctrl.rssi;
        a.dirty = true;
    }
    /* Keep a sighting useful when the first beacon arrived before GPS lock.
     * A later packet can attach the first valid position without requiring a
     * stronger RSSI than the original observation. */
    gps_fix_t g;
    if (gps_snapshot(&g) && g.valid) {
        if (!a.has_gps) {
            a.lat = g.lat_deg; a.lon = g.lon_deg; a.alt = g.alt_m;
            a.has_gps = true;
            a.dirty = true;
        }
        if (a.first_seen_utc[0] == '\0') {
            format_wigle_time(g, a.first_seen_utc);
            a.dirty = true;
        }
    }
    portEXIT_CRITICAL_ISR(&s_wdr_mux);
}

static void hop_task(void *)
{
    s_hop_alive = true;
    while (s_running) {
        if (s_c5_hold) {                 /* C5 5 GHz window: hold ch1 so the */
            if (s_current_ch != 1) {     /* satellite's ESP-NOW batch reaches us */
                s_current_ch = 1;
                esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
            }
            delay(50);
            continue;
        }
        s_current_ch = (s_current_ch % 13) + 1;
        esp_wifi_set_channel(s_current_ch, WIFI_SECOND_CHAN_NONE);
        delay(400);
    }
    s_hop_alive = false;
    vTaskDelete(nullptr);
}

/* Fold the C5 satellite's collected 5 GHz APs into the shared wardrive
 * table so they land in the same WiGLE CSV — deduped by BSSID, tagged
 * with the current GPS fix (same null-island guard as 2.4 GHz rows).
 * Best-effort: the C5 talks ESP-NOW on ch1 but wardrive channel-hops
 * 1-13, so 5 GHz sightings are harvested on the hop's ch1 passes. Runs
 * from the UI task — shares s_wdr_mux with the promisc ISR. */
static void merge_c5_5g(void)
{
    c5_ap_t buf[32];
    int n = c5_aps(buf, 32);
    if (n <= 0) return;

    gps_fix_t g;
    bool have_gps = gps_snapshot(&g);

    for (int i = 0; i < n; ++i) {
        if (!buf[i].is_5g) continue;
        bool is_new = false;
        portENTER_CRITICAL(&s_wdr_mux);
        int idx = find_ap(buf[i].bssid);
        if (idx < 0) {
            if (s_ap_count >= WARDRIVE_MAX_APS) {
                uint32_t oldest = UINT32_MAX;
                int victim = -1;
                for (int j = 0; j < s_ap_count; ++j) {
                    if ((!s_aps[j].dirty || !s_aps[j].has_gps) && s_aps[j].last_seen < oldest) {
                        oldest = s_aps[j].last_seen;
                        victim = j;
                    }
                }
                if (victim < 0) {
                    s_force_flush = true;
                    portEXIT_CRITICAL(&s_wdr_mux);
                    continue;
                }
                idx = victim;
                s_cache_rollovers = s_cache_rollovers + 1;
            } else {
                idx = s_ap_count++;
            }
            is_new = true;
            memset(&s_aps[idx], 0, sizeof(wdr_ap_t));
            memcpy(s_aps[idx].bssid, buf[i].bssid, 6);
            s_aps[idx].first_seen_ms = millis();
        }
        wdr_ap_t &a = s_aps[idx];
        a.last_seen = millis();
        a.channel   = buf[i].channel;
        a.auth      = buf[i].auth;
        strncpy(a.ssid, buf[i].ssid, sizeof(a.ssid) - 1);
        a.ssid[sizeof(a.ssid) - 1] = '\0';
        if (buf[i].rssi > a.rssi || a.rssi == 0) {
            a.rssi = buf[i].rssi;
            if (have_gps) {
                a.lat = g.lat_deg; a.lon = g.lon_deg; a.alt = g.alt_m;
                a.has_gps = true;
            }
            a.dirty = true;
        }
        if (have_gps && a.first_seen_utc[0] == '\0') {
            format_wigle_time(g, a.first_seen_utc);
            a.dirty = true;
        }
        portEXIT_CRITICAL(&s_wdr_mux);
        if (is_new) {
            s_5g_count = s_5g_count + 1;
            s_new_this_run = s_new_this_run + 1;
            s_last_new_ms = millis();
            surv_class_t scls = flock_classify_oui(buf[i].bssid);
            if (scls == SURV_UNKNOWN && buf[i].ssid[0]) scls = flock_classify_ssid(buf[i].ssid);
            if (scls != SURV_UNKNOWN) {
                s_surv_count = s_surv_count + 1;
                s_surv_pending = true;
                s_last_surv_cls = scls;
                s_last_surv_ms = millis();
                strncpy((char*)s_last_surv_name, buf[i].ssid[0] ? buf[i].ssid : surv_tag_str(scls, ""), sizeof(s_last_surv_name) - 1);
                s_last_surv_name[sizeof(s_last_surv_name) - 1] = 0;
            }
            if (buf[i].auth == WIFI_AUTH_OPEN || buf[i].auth == WIFI_AUTH_WPA3_PSK)
                s_juicy_pending = true;
        }
    }
}

static void draw_plain_view(bool &dirty)
{
    auto &d = M5Cardputer.Display;
    if (dirty) {
        ui_clear_body();
        d.setTextColor(T_ACCENT, T_BG);
        d.setCursor(4, BODY_Y + 2);  d.print("WARDRIVE");
        dirty = false;
    }
    d.setTextColor(T_FG, T_BG);
    /* Cache occupancy hidden here on purpose — it caps at WARDRIVE_MAX_APS
     * by design once the rolling table fills, which read as a false stall.
     * "wifi" (uncapped, unique networks this session) is the number to
     * watch; see wdr_milestone_crossed / s_new_this_run. */
    d.setCursor(4, BODY_Y + 18); d.printf("5G:%-3d Surv:%-3d", s_5g_count, s_surv_count);
    d.setCursor(4, BODY_Y + 30); d.printf("Beacons:%-7lu wifi:%-5d", (unsigned long)s_beacons, s_new_this_run);
    d.setCursor(4, BODY_Y + 42); d.printf("Channel: %-2u  C5:%-3s",
                                          s_current_ch, c5_any_online() ? "on" : "off");
    const gps_fix_t &g = gps_get();
    d.setTextColor(g.valid ? T_GOOD : T_DIM, T_BG);
    d.setCursor(4, BODY_Y + 54);
    if (g.valid) d.printf("GPS: %.4f, %.4f (%d sats)   ", g.lat_deg, g.lon_deg, g.sats);
    else         d.printf("GPS: waiting for fix...      ");

    uint32_t now = millis();
    if (now - s_last_surv_ms < 4000 && s_last_surv_name[0]) {
        d.setTextColor(T_BAD, T_BG);
        d.setCursor(4, BODY_Y + 70);
        d.printf("[!] SURV: %-23.23s", s_last_surv_name);
    } else {
        d.setTextColor(T_DIM, T_BG);
        d.setCursor(4, BODY_Y + 70);
        d.printf("%-30s", s_csv_path);
    }
}

static void draw_argus_view(argus_mood_t base, bool &dirty)
{
    auto &d = M5Cardputer.Display;
    /* Clear ONCE on entry / view switch. argus_draw caches and will not
     * re-push an unchanged mood, so a per-frame clear would leave a gap.
     * Invalidate the cache here so the face repaints after the wipe (fixes
     * the "Argus vanishes after exit + re-enter" case, where mood/x/y are
     * unchanged from the previous session and the cache would skip). */
    if (dirty) { ui_clear_body(); argus_invalidate(); dirty = false; }

    argus_draw(base, 8, BODY_Y);   /* 96x96, matches Triton placement */

    const int rx = 110;            /* right stat column */
    d.setTextColor(T_FG, T_BG);
    /* "wifi" is the uncapped running total of unique networks found this
     * session -- the number to watch. Cache occupancy (caps at
     * WARDRIVE_MAX_APS by design) is intentionally not shown here. */
    d.setCursor(rx, BODY_Y + 2);  d.printf("wifi %-5d", s_new_this_run);
    d.setCursor(rx, BODY_Y + 14); d.printf("bcn %-6lu", (unsigned long)s_beacons);
    /* ch + 5G count (magenta when a C5 satellite is feeding us) + C5 pip */
    bool c5on = c5_any_online();
    d.setCursor(rx, BODY_Y + 38);
    d.setTextColor(T_FG, T_BG);              d.printf("ch%-3u", s_current_ch);
    d.setTextColor(c5on ? T_ACCENT2 : T_DIM, T_BG); d.printf("5G%-3d", s_5g_count);
    d.setTextColor(c5on ? T_GOOD : T_DIM, T_BG);    d.printf("C5%c", c5on ? '*' : '.');

    const gps_fix_t &g = gps_get();
    d.setCursor(rx, BODY_Y + 50);
    d.setTextColor(g.valid ? T_GOOD : T_DIM, T_BG);
    d.printf("GPS%c%-2d", g.valid ? '*' : '.', g.sats);
    if (s_surv_count > 0) {
        d.setTextColor(T_BAD, T_BG);
        d.printf(" !%-2d", s_surv_count);
    } else {
        d.setTextColor(T_DIM, T_BG);
        d.print("    ");
    }

    uint32_t now = millis();
    d.setCursor(rx, BODY_Y + 62);
    if (now - s_last_surv_ms < 4000 && s_last_surv_name[0]) {
        d.setTextColor(T_BAD, T_BG);
        d.printf("!%.13s", s_last_surv_name);
    } else {
        d.setTextColor(T_DIM, T_BG);
        if (g.valid) d.printf("%-14s", s_csv_path + 10);  /* skip "/poseidon/" prefix */
        else         d.printf("holding rows ");
    }
}

static bool hybrid_restore_wifi(void)
{
    radio_switch(RADIO_WIFI);
    if (!wifi_lean_sta_init()) return false;
    static const wifi_promiscuous_filter_t all_filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
    };
    esp_wifi_set_promiscuous_filter(&all_filter);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_channel(s_current_ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    if (s_snapshot_mode) return true;
    s_running = true;
    if (xTaskCreate(hop_task, "wdr_hop", 2048, nullptr, 4, nullptr) != pdPASS) {
        s_running = false;
        esp_wifi_set_promiscuous(false);
        return false;
    }
    return true;
}

static bool hybrid_restore_storage(void)
{
    if (!sd_remount()) return false;
    if (s_snapshot_mode)
        return (bool)(s_csv = SD.open(s_csv_path, FILE_APPEND));
    if (!wdr_aps_ensure()) return false;
    return wdr_open_csv();
}

static bool hybrid_restore_all(void)
{
    return hybrid_restore_storage() && hybrid_restore_wifi();
}

static bool hybrid_ble_seen(const uint8_t *addr)
{
    for (int i = 0; i < s_hybrid_ble_seen_n; ++i)
        if (memcmp(s_hybrid_ble_seen[i], addr, 6) == 0) return true;
    return false;
}

static bool run_hybrid_ble_burst(void)
{
    Serial.printf("[wdr-hybrid] burst begin free=%u largest=%u\n",
                  (unsigned)heap_free_internal(),
                  (unsigned)heap_largest_internal());
    if (c5_any_online()) {
        Serial.println("[wdr-hybrid] skipped: C5 active");
        ui_toast("BLE burst skipped: C5 active", T_WARN, 1200);
        return false;
    }

    s_running = false;
    uint32_t deadline = millis() + 800;
    while (s_hop_alive && millis() < deadline) delay(5);
    esp_wifi_set_promiscuous(false);
    flush_dirty_rows();
    if (s_csv) s_csv.flush();

    /* The AP cache and SD bus are Wardrive-only state. The durable CSV is
     * already flushed, so evacuate both before asking BLE for a large block. */
    if (s_csv) s_csv.close();
    if (g_wdr_aps) {
        heap_caps_free(g_wdr_aps);
        g_wdr_aps = nullptr;
        s_ap_count = 0;
    }
    SD.end();
    sd_get_spi().end();

    /* Release the WiFi driver and reclaim registered display caches before
     * asking the BT controller for its large contiguous startup block. */
    heap_reclaim_all();
    wifi_release_driver();
    Serial.printf("[wdr-hybrid] after WiFi release free=%u largest=%u\n",
                  (unsigned)heap_free_internal(),
                  (unsigned)heap_largest_internal());
    if (heap_largest_internal() < HYBRID_BLE_MIN_BLOCK) {
        Serial.printf("[wdr-hybrid] skipped: need=%lu largest=%u\n",
                      (unsigned long)HYBRID_BLE_MIN_BLOCK,
                      (unsigned)heap_largest_internal());
        ui_toast("BLE burst skipped: low DMA RAM", T_WARN, 1400);
        return hybrid_restore_all();
    }
    if (!radio_switch(RADIO_BLE)) {
        Serial.println("[wdr-hybrid] skipped: radio switch failed");
        ui_toast("BLE burst unavailable", T_WARN, 1400);
        return hybrid_restore_all();
    }

    NimBLEScan *scan = NimBLEDevice::getScan();
    if (!scan) {
        radio_switch(RADIO_WIFI);
        hybrid_restore_all();
        return false;
    }
    scan->setInterval(97);
    scan->setWindow(67);
    scan->setDuplicateFilter(false);
    scan->setMaxResults(64);
    scan->setActiveScan(true);
    Serial.printf("[wdr-hybrid] scan start free=%u largest=%u\n",
                  (unsigned)heap_free_internal(),
                  (unsigned)heap_largest_internal());
    scan->start(HYBRID_BLE_SCAN_MS, false);
    while (scan->isScanning()) {
        gps_poll();
        delay(20);
    }

    NimBLEScanResults results = scan->getResults();
    s_hybrid_ble_row_n = 0;
    for (int i = 0; i < (int)results.getCount(); ++i) {
        const NimBLEAdvertisedDevice *device = results.getDevice(i);
        if (!device) continue;
        NimBLEAddress address = device->getAddress();
        const uint8_t *raw = address.getBase()->val;
        if (hybrid_ble_seen(raw)) continue;

        gps_fix_t g;
        char stamp[16];
        if (!gps_snapshot(&g) || !g.valid) continue;
        format_wigle_time(g, stamp);
        if (!stamp[0]) continue;

        if (s_hybrid_ble_row_n >= (int)(sizeof(s_hybrid_ble_rows) / sizeof(s_hybrid_ble_rows[0])))
            continue;
        hybrid_ble_row_t &row = s_hybrid_ble_rows[s_hybrid_ble_row_n++];
        memcpy(row.addr, raw, 6);
        strncpy(row.name, device->getName().c_str(), sizeof(row.name) - 1);
        row.name[sizeof(row.name) - 1] = '\0';
        strncpy(row.stamp, stamp, sizeof(row.stamp) - 1);
        row.stamp[sizeof(row.stamp) - 1] = '\0';
        row.rssi = device->getRSSI();
        row.lat = g.lat_deg; row.lon = g.lon_deg; row.alt = g.alt_m;
    }
    scan->clearResults();
    radio_switch(RADIO_WIFI);
    if (!hybrid_restore_storage()) return false;
    int logged = 0;
    for (int i = 0; i < s_hybrid_ble_row_n; ++i) {
        hybrid_ble_row_t &row = s_hybrid_ble_rows[i];
        if (hybrid_ble_seen(row.addr)) continue;
        char escaped_name[70];
        csv_escape(row.name, escaped_name, sizeof(escaped_name));
        s_csv.printf("%02X:%02X:%02X:%02X:%02X:%02X,%s,[BLE],%s,0,%d,%.6f,%.6f,%.1f,5,BLE\n",
                     row.addr[0], row.addr[1], row.addr[2], row.addr[3],
                     row.addr[4], row.addr[5], escaped_name, row.stamp,
                     row.rssi, row.lat, row.lon, row.alt);
        if (s_hybrid_ble_seen_n < HYBRID_BLE_SEEN_MAX)
            memcpy(s_hybrid_ble_seen[s_hybrid_ble_seen_n++], row.addr, 6);
        s_hybrid_ble_total++;
        logged++;
    }
    s_csv.flush();
    bool restored = hybrid_restore_wifi();
    Serial.printf("[wdr-hybrid] burst end logged=%d restored=%d free=%u largest=%u\n",
                  logged, (int)restored, (unsigned)heap_free_internal(),
                  (unsigned)heap_largest_internal());
    if (logged > 0) {
        char msg[32];
        snprintf(msg, sizeof(msg), "BLE +%d", logged);
        ui_toast(msg, T_GOOD, 900);
    }
    return restored;
}

static bool snapshot_wifi_seen(const uint8_t *addr)
{
    for (int i = 0; i < s_snapshot_wifi_seen_n; ++i)
        if (memcmp(s_snapshot_wifi_seen[i], addr, 6) == 0) return true;
    return false;
}

static void snapshot_log_wifi(void)
{
    int count = WiFi.scanNetworks(false, true);
    gps_fix_t g;
    char stamp[16];
    bool have_gps = gps_snapshot(&g) && g.valid;
    if (have_gps) format_wigle_time(g, stamp);
    for (int i = 0; i < count; ++i) {
        const uint8_t *raw = WiFi.BSSID(i);
        if (!raw || snapshot_wifi_seen(raw) || !have_gps || !stamp[0]) continue;
        char ssid[33];
        strncpy(ssid, WiFi.SSID(i).c_str(), sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        char escaped[70];
        csv_escape(ssid, escaped, sizeof(escaped));
        s_csv.printf("%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%s,%ld,%ld,%.6f,%.6f,%.1f,5,WIFI\n",
                     raw[0], raw[1], raw[2], raw[3], raw[4], raw[5],
                     escaped, auth_to_wigle(WiFi.encryptionType(i)), stamp,
                     (long)WiFi.channel(i), (long)WiFi.RSSI(i), g.lat_deg, g.lon_deg,
                     g.alt_m);
        if (s_snapshot_wifi_seen_n < (int)(sizeof(s_snapshot_wifi_seen) / sizeof(s_snapshot_wifi_seen[0])))
            memcpy(s_snapshot_wifi_seen[s_snapshot_wifi_seen_n++], raw, 6);
    }
    WiFi.scanDelete();
    if (s_csv) s_csv.flush();
}

static void feat_wifi_wardrive_snapshot(void)
{
    s_hybrid_enabled = true;
    s_snapshot_mode = true;
    s_hybrid_ble_total = 0;
    s_hybrid_ble_seen_n = 0;
    s_snapshot_wifi_seen_n = 0;
    memset(s_hybrid_ble_seen, 0, sizeof(s_hybrid_ble_seen));
    memset(s_snapshot_wifi_seen, 0, sizeof(s_snapshot_wifi_seen));

    if (!sd_mount() && !sd_remount()) {
        ui_toast("SD mount failed - reseat card?", T_BAD, 1800);
        s_snapshot_mode = false;
        s_hybrid_enabled = false;
        return;
    }
    radio_switch(RADIO_WIFI);
    wifi_release_driver();
    heap_reclaim_all();
    if (!wifi_lean_sta_init() || !gps_ensure_running() || !wdr_open_csv()) {
        ui_toast("snapshot setup failed", T_BAD, 1600);
        s_snapshot_mode = false;
        s_hybrid_enabled = false;
        return;
    }

    ui_clear_body();
    ui_draw_footer("ESC=stop  WiFi snapshot -> BLE  ?=help");
    bool failed = false;
    while (true) {
        gps_poll();
        snapshot_log_wifi();
        if (!run_hybrid_ble_burst()) {
            failed = true;
            break;
        }
        ui_draw_status("hybrid", "WiFi+BLE");
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (k == '?') ui_show_current_help();
    }
    s_running = false;
    if (s_csv) s_csv.close();
    esp_wifi_set_promiscuous(false);
    radio_switch(RADIO_NONE);
    if (failed) ui_toast("hybrid stopped safely", T_WARN, 1400);
    s_snapshot_mode = false;
    s_hybrid_enabled = false;
}

void feat_wifi_wardrive(void)
{
    /* SD mount BEFORE radio_switch — WiFi init grabs ~30 KB of heap and
     * fragments what's left, and FATFS's mount allocation can then fail
     * even on a healthy card. Mount first while heap is clean. */
    if (!sd_mount() && !sd_remount()) {
        ui_toast("SD mount failed - reseat card?", T_BAD, 1800);
        return;
    }

    /* Leave BLE or another radio domain before allocating the persistent AP
     * table. BLE controller memory is otherwise still resident and can make
     * this allocation fail after visiting several screens. */
    radio_switch(RADIO_WIFI);

    /* radio_switch() is a no-op when WiFi was already the active domain
     * (e.g. coming straight from WiFi Scan) — the driver is left resident
     * per teardown_current()'s intentional stopped-but-inited policy, which
     * fragments the heap and can shrink the largest free block below the
     * AP table's requirement. First-time allocation needs the whole block
     * contiguous, so fully release and let wifi_lean_sta_init() below
     * bring the driver back up clean. */
    if (!g_wdr_aps) wifi_release_driver();

    /* Reclaim display/radio caches while the heap is still clean, before WiFi
     * init grabs its buffers. The preflight checks the largest contiguous
     * block, which is the constraint that controls this allocation. */
    if (!g_wdr_aps && !rf_preflight("wardrive", sizeof(wdr_ap_t) * WARDRIVE_MAX_APS))
        return;
    if (!wdr_aps_ensure()) return;

    wifi_lean_sta_init();
    /* Wardrive is the canonical GPS-using feature; treat entry as the
     * opt-in event (persists user_enabled to NVS so cold-boots also
     * spawn the poller). gps_ensure_running internally calls gps_begin
     * + spawns the polling task if not already running. */
    gps_ensure_running();
    if (!wdr_open_csv()) {
        /* CSV open failed despite mount — try a remount and re-open once
         * more. Covers the case where mount thinks it's good but the
         * underlying FAT state is wonky after a format. */
        if (!sd_remount() || !wdr_open_csv()) {
            ui_toast("cant open csv", T_BAD, 1500);
            return;
        }
    }

    /* Keep accumulated AP table across sessions so Triton + wifi_scan can
     * seed themselves from prior wardrive runs. Each session still writes
     * a fresh CSV; only new sightings flip dirty=true for the new file. */
    s_beacons  = 0;
    s_current_ch = 1;
    s_5g_count = 0;
    s_new_this_run = 0;
    s_surv_count = 0;
    s_surv_pending = false;
    s_last_surv_name[0] = 0;
    s_last_surv_cls = SURV_UNKNOWN;
    s_last_surv_ms = 0;
    s_last_new_idx = -1;
    s_force_flush = false;
    if (s_hybrid_enabled) {
        s_hybrid_ble_total = 0;
        s_hybrid_ble_seen_n = 0;
        memset(s_hybrid_ble_seen, 0, sizeof(s_hybrid_ble_seen));
    }
    s_entry_ms = millis();

    /* Explicit MASK_ALL filter. On IDF 5.5, NOT setting a filter (or
     * passing nullptr) silently disables capture for some frame types
     * — Triton hit this bug. Without this we wouldn't see beacons. */
    static const wifi_promiscuous_filter_t s_all_filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
    };
    esp_wifi_set_promiscuous_filter(&s_all_filter);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_channel(s_current_ch, WIFI_SECOND_CHAN_NONE);

    s_running = true;
    xTaskCreate(hop_task, "wdr_hop", 2048, nullptr, 4, nullptr);

    /* Bring up the C5 ESP-NOW link AFTER the hop task has its stack — ESP-NOW
     * init eats internal SRAM, and doing it first starved xTaskCreate(hop_task)
     * (silent fail → wardrive froze on channel 1). Idempotent; if a satellite
     * is present it feeds us 5 GHz APs. Note: c5_begin re-pins ch1 once, but
     * the hop task immediately resumes sweeping. */
    bool c5_discovery_active = c5_begin();
    bool c5_detected = false;
    uint32_t c5_discovery_deadline = millis() + 6500;

    ui_clear_body();
    ui_draw_footer(s_hybrid_enabled
        ? "ESC=stop  A=view  F=flush  BLE=5s/30s  ?=help"
        : "ESC=stop  A=view  F=flush  ?=help");

    uint32_t last_redraw = 0;
    uint32_t last_flush  = 0;
    uint32_t last_c5_scan = 0;
    uint32_t last_c5_merge = 0;
    uint32_t c5_window_end = 0;
    uint32_t next_hybrid_ble = millis() + HYBRID_BLE_INTERVAL_MS;
    bool dirty = true;
    bool     prev_gps_valid = false;
    int      prev_new       = 0;
    int      new_5s_ref     = 0;
    uint32_t new_5s_ref_ms  = s_entry_ms;
    int      mx_prev_apc    = s_ap_count;   /* only feed the matrix view APs seen from here on */
    int      mx_prev_new    = 0;
    bool     prev_c5        = false;
    while (true) {
        gps_poll();
        uint32_t now = millis();

        if (s_hybrid_enabled && now >= next_hybrid_ble) {
            next_hybrid_ble = now + HYBRID_BLE_INTERVAL_MS;
            if (!run_hybrid_ble_burst() && !s_running) break;
            dirty = true;
            last_redraw = 0;
            continue;
        }

        /* C5 5 GHz augmentation — hop-SYNCHRONOUS. The old "opportunistic over
         * the hop" approach logged zero 5 GHz APs: the C5 ships its result
         * batch on ch1 ~2 s after the command, by which point the hop task has
         * moved us off ch1, so the batch was never received. Fix: every ~6 s
         * open a short window where the hop task parks on ch1 (s_c5_hold) so the
         * scan command AND the streamed batch both land, then resume hopping. */
        bool c5_online = c5_any_online();
        if (c5_online) c5_detected = true;
        if (c5_discovery_active && !c5_detected && now >= c5_discovery_deadline) {
            /* A C5 broadcasts HELLO every 5 s. If a full interval plus margin
             * passes without one, release ESP-NOW's heap for local capture. */
            c5_stop();
            c5_discovery_active = false;
            heap_report("wardrive c5 absent");
        }
        if (c5_online) {
            if (!s_c5_hold && now - last_c5_scan > 6000) {
                last_c5_scan   = now;
                s_c5_hold      = true;                 /* hop task parks on ch1 */
                esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
                s_current_ch   = 1;
                c5_clear_results();
                c5_cmd_scan_5g(2000);
                c5_window_end  = now + 2700;           /* scan dur + result margin */
            }
            if (now - last_c5_merge > 500) {           /* harvest streamed batches */
                last_c5_merge = now;
                merge_c5_5g();
            }
            if (s_c5_hold && now > c5_window_end) {     /* close window, resume hop */
                merge_c5_5g();
                s_c5_hold = false;
            }
        }

        if (now - last_flush > 3000 || s_force_flush) {
            last_flush = now;
            s_force_flush = false;
            flush_dirty_rows();
        }
        /* Matrix view animates fast; the partial-redraw views stay at 250ms. */
        uint32_t redraw_iv = (s_view == WDR_VIEW_MATRIX) ? 40 : 250;
        if (now - last_redraw > redraw_iv) {
            last_redraw = now;
            const gps_fix_t &g = gps_get();

            /* ---- process newly discovered APs (runs in every view) ----
             * Feed the matrix decode with real SSIDs, and fire the OPEN/WPA3
             * sound cue. AP data snapshotted under the promisc mutex. */
            int apc = s_ap_count;
            if (apc > mx_prev_apc) {
                for (int i = mx_prev_apc; i < apc && i < WARDRIVE_MAX_APS; ++i) {
                    char ss[40]; uint8_t au; int8_t rs; uint8_t ch; uint8_t bb[6];
                    portENTER_CRITICAL(&s_wdr_mux);
                    strncpy(ss, s_aps[i].ssid, 33); ss[33] = 0;
                    au = s_aps[i].auth; rs = s_aps[i].rssi; ch = s_aps[i].channel;
                    memcpy(bb, s_aps[i].bssid, 6);
                    portEXIT_CRITICAL(&s_wdr_mux);
                    if (ss[0] == 0) snprintf(ss, sizeof(ss), "<hidden %02X:%02X>", bb[4], bb[5]);
                    bool juicy = (au == WIFI_AUTH_OPEN || au == WIFI_AUTH_WPA3_PSK);
                    surv_class_t scls = flock_classify_oui(bb);
                    if (scls == SURV_UNKNOWN && ss[0]) scls = flock_classify_ssid(ss);
                    const char *stag = (scls != SURV_UNKNOWN) ? surv_tag_str(scls, ss) : nullptr;
                    if (s_view == WDR_VIEW_MATRIX) wdr_matrix_feed(ss, au, rs, ch, stag);
                    if (stag) sfx_alert();
                    else if (juicy) sfx_glitch();
                }
                mx_prev_apc = apc;
            } else if (s_new_this_run > mx_prev_new && s_last_new_idx >= 0) {
                /* At capacity, new APs replace old slots so s_ap_count no
                 * longer changes. Feed the most recent replacement instead
                 * of leaving the live SSID roster frozen forever. */
                int i = s_last_new_idx;
                char ss[40]; uint8_t au; int8_t rs; uint8_t ch; uint8_t bb[6];
                portENTER_CRITICAL(&s_wdr_mux);
                strncpy(ss, s_aps[i].ssid, 33); ss[33] = 0;
                au = s_aps[i].auth; rs = s_aps[i].rssi; ch = s_aps[i].channel;
                memcpy(bb, s_aps[i].bssid, 6);
                portEXIT_CRITICAL(&s_wdr_mux);
                if (ss[0] == 0) snprintf(ss, sizeof(ss), "<hidden %02X:%02X>", bb[4], bb[5]);
                bool juicy = (au == WIFI_AUTH_OPEN || au == WIFI_AUTH_WPA3_PSK);
                surv_class_t scls = flock_classify_oui(bb);
                if (scls == SURV_UNKNOWN && ss[0]) scls = flock_classify_ssid(ss);
                const char *stag = (scls != SURV_UNKNOWN) ? surv_tag_str(scls, ss) : nullptr;
                if (s_view == WDR_VIEW_MATRIX) wdr_matrix_feed(ss, au, rs, ch, stag);
                if (stag) sfx_alert();
                else if (juicy) sfx_glitch();
            }
            mx_prev_new = s_new_this_run;

            /* Consume the ISR juicy flag and surveillance flag. */
            bool juicy_flash = s_juicy_pending; s_juicy_pending = false;
            bool surv_flash  = s_surv_pending;  s_surv_pending  = false;
            if (surv_flash) {
                sfx_alert();
                argus_flash(ARGUS_OLD_FURY, 1600);
            }

            /* C5 satellite came online -> one celebration flash + sound cue. */
            bool c5_now = c5_any_online();
            if (c5_now && !prev_c5) { argus_flash(ARGUS_PLEASED, 800); sfx_scan_hit(); }
            prev_c5 = c5_now;

            if (s_view == WDR_VIEW_MATRIX) {
                wdr_matrix_render(s_current_ch, s_ap_count, g.valid, g.sats);
            } else {
                ui_draw_status(radio_name(), "wardrive");

                /* GPS lock edges -> celebration / annoyance flashes. */
                if (g.valid && !prev_gps_valid) {
                    s_gps_ever_locked = true;
                    argus_flash(ARGUS_PLEASED, 900);
                } else if (!g.valid && prev_gps_valid) {
                    argus_flash(ARGUS_ANNOYED, 900);
                }
                prev_gps_valid = g.valid;

                /* Milestone every 50 new APs + juicy instant flash. */
                int new_now = s_new_this_run;
                if (wdr_milestone_crossed(prev_new, new_now, 50)) argus_flash(ARGUS_PLEASED, 700);
                prev_new = new_now;
                if (juicy_flash) argus_flash(ARGUS_PLEASED, 500);

                /* Sliding ~5 s window of new APs for the dense-zone mood. */
                if (now - new_5s_ref_ms >= 5000) { new_5s_ref = new_now; new_5s_ref_ms = now; }

                if (s_view == WDR_VIEW_ARGUS) {
                    wdr_mood_ctx mc;
                    mc.gps_valid       = g.valid;
                    mc.gps_ever_locked = s_gps_ever_locked;
                    mc.gps_speed_kts   = g.speed_kts;
                    mc.now_ms          = now;
                    mc.entry_ms        = s_entry_ms;
                    mc.last_new_ms     = s_last_new_ms;
                    mc.new_in_5s       = new_now - new_5s_ref;
                    mc.ap_count        = s_ap_count;
                    mc.ap_cap          = WARDRIVE_MAX_APS;
                    draw_argus_view(wdr_pick_mood(mc), dirty);
                } else {
                    draw_plain_view(dirty);
                }
            }
        }

        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) break;
        if (k == '?') { ui_show_current_help(); dirty = true; }
        if (k == 'f' || k == 'F') {
            flush_dirty_rows();
            ui_toast("flushed", T_GOOD, 400);
            dirty = true;
        }
        if (k == 'a' || k == 'A') {
            s_view = (wdr_view_t)((s_view + 1) % WDR_VIEW__COUNT);
            if (s_view == WDR_VIEW_MATRIX) {
                /* Seed the roster from the most recent APs, oldest->newest so
                 * the newest lands on top; new arrivals decode in live. */
                wdr_matrix_begin();
                int start = s_ap_count > MX_SEED ? s_ap_count - MX_SEED : 0;
                for (int j = start; j < s_ap_count; ++j) {
                    char ss[40]; uint8_t au; int8_t rs; uint8_t ch; uint8_t bb[6];
                    portENTER_CRITICAL(&s_wdr_mux);
                    strncpy(ss, s_aps[j].ssid, 33); ss[33] = 0;
                    au = s_aps[j].auth; rs = s_aps[j].rssi; ch = s_aps[j].channel;
                    memcpy(bb, s_aps[j].bssid, 6);
                    portEXIT_CRITICAL(&s_wdr_mux);
                    if (ss[0] == 0) snprintf(ss, sizeof(ss), "<hidden %02X:%02X>", bb[4], bb[5]);
                    wdr_matrix_seed(ss, au, rs, ch);
                }
            } else {
                /* Leaving the full-screen matrix: restore chrome the next
                 * frame (status cache was clobbered, footer overwritten). */
                ui_status_invalidate();
                ui_draw_footer("ESC=stop  A=view  F=flush  ?=help");
            }
            dirty = true;
        }
    }

    s_running = false;
    /* Join the hop task before returning — its loop period (400ms) is
     * longer than the old delay(150), so it could still be issuing
     * esp_wifi_set_channel when the next feature inits the radio. */
    uint32_t deadline = millis() + 800;
    while (s_hop_alive && millis() < deadline) delay(5);
    esp_wifi_set_promiscuous(false);
    flush_dirty_rows();
    if (s_csv) { s_csv.close(); }
    delay(150);
}

void feat_wifi_wardrive_hybrid(void)
{
    feat_wifi_wardrive_snapshot();
}
