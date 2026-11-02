/*
 * wifi_scan — reference feature. Shows the POSEIDON pattern:
 *
 *   1. Enter feature: radio_switch(RADIO_WIFI), start async scan task.
 *   2. While running: UI shows live status, keyboard is responsive.
 *   3. Results list: letter hotkeys for actions (D=deauth, C=clone,
 *      I=info, P=portal, O=open-only filter, / = typed filter).
 *   4. ESC: drop out, caller decides radio teardown.
 *
 * This is the template. Every feature should follow this shape —
 * never block the UI, always let the keyboard drive actions.
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "menu.h"
#include "wifi_types.h"
#include "c5_cmd.h"
#include "sd_helper.h"
#include "ble_db.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <math.h>
#include <esp_event.h>
#include <SD.h>

#define MAX_APS 64

static ap_t     s_aps[MAX_APS];
static int      s_ap_count = 0;
static volatile bool s_scan_running = false;
static volatile bool s_scan_done = false;
static char     s_filter[24] = "";
static bool     s_filter_open_only = false;

/* Shared with portal, deauth, ap-clone. Declared in wifi_types.h. */
ap_t g_last_selected_ap = {};
bool g_last_selected_valid = false;

extern void feat_wifi_deauth(void);
extern void feat_wifi_deauth_broadcast_ap(const ap_t &target);
extern void feat_wifi_pmkid(void);
extern void feat_wifi_clients(void);
extern void feat_wifi_apclone(void);
extern void feat_wifi_portal(void);
extern void c5_deauth_dashboard(const ap_t &target, bool broadcast);

static const char *auth_str(uint8_t a)
{
    switch (a) {
    case WIFI_AUTH_OPEN:          return "OPEN";
    case WIFI_AUTH_WEP:           return "WEP";
    case WIFI_AUTH_WPA_PSK:       return "WPA";
    case WIFI_AUTH_WPA2_PSK:      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "EAP";
    case WIFI_AUTH_WPA3_PSK:      return "WPA3";
    default:                      return "?";
    }
}

/* (scan_task removed — R=rescan now re-runs the inline animated scan on the
 * caller task; the old background task hung on Arduino WiFi.scanNetworks over
 * a raw-inited driver and raced the feature exit.) */

static bool ap_matches_filter(const ap_t &a)
{
    if (s_filter_open_only && a.auth != WIFI_AUTH_OPEN) return false;
    if (s_filter[0] == '\0') return true;
    /* Case-insensitive substring on SSID. */
    const char *s = a.ssid;
    size_t fl = strlen(s_filter);
    for (; *s; ++s) {
        if (strncasecmp(s, s_filter, fl) == 0) return true;
    }
    return false;
}

#define SCAN_ROWS (ui_big_text() ? 3 : 8)

/* Build the filtered index list (visible items). Returns count. */
static int build_filtered(int *idx)
{
    int n = 0;
    for (int i = 0; i < s_ap_count; ++i) {
        if (ap_matches_filter(s_aps[i])) idx[n++] = i;
    }
    return n;
}

/* Header row with AP count + filter status. Overwritten in place. */
static void draw_list_header(void)
{
    char buf[64];
    if (s_filter[0] || s_filter_open_only) {
        snprintf(buf, sizeof(buf), "APs %d  filter:%s%s", s_ap_count,
                 s_filter, s_filter_open_only ? "+open" : "");
    } else {
        snprintf(buf, sizeof(buf), "APs %d", s_ap_count);
    }
    ui_header(buf);
}

/* Paint one AP row in full over its own background (no body clear). */
static void draw_ap_row(int r, const int *idx, int first, int cursor)
{
    auto &d = M5Cardputer.Display;
    int ai = idx[first + r];
    const ap_t &a = s_aps[ai];
    const bool large = ui_big_text();
    const int row_height = large ? 29 : 11;
    int y = BODY_Y + 20 + r * row_height;
    bool sel = (first + r == cursor);
    uint16_t bg = sel ? T_SEL_BG : T_BG;
    uint16_t fg = sel ? T_ACCENT : T_FG;
    d.fillRect(0, y - 1, SCR_W, row_height, bg);
    if (large) {
        ui_label(6, y, 228, fg, bg, a.ssid[0] ? a.ssid : "(hidden)", 2);
        char metadata[40];
        snprintf(metadata, sizeof(metadata), "%s  CH %u  %d dBm  %s",
                 a.is_5g ? "5G" : "2G", a.channel, a.rssi, auth_str(a.auth));
        ui_label(6, y + 18, 228, T_DIM, bg, metadata);
        if (sel) d.fillRect(0, y, 2, row_height - 3, T_ACCENT);
        return;
    }

    /* band tag | ch | rssi | auth | ssid */
    d.setTextColor(a.is_5g ? T_ACCENT : T_DIM, bg);
    d.setCursor(2, y);
    d.print(a.is_5g ? "5G" : "2G");
    d.setTextColor(T_DIM, bg);
    d.setCursor(16, y);
    d.printf("%3u", a.channel);
    d.setTextColor(fg, bg);
    d.setCursor(32, y);
    d.printf("%4d", a.rssi);
    d.setTextColor(a.auth == WIFI_AUTH_OPEN ? T_BAD : T_GOOD, bg);
    d.setCursor(58, y);
    d.printf("%-5s", auth_str(a.auth));
    /* WPS marker — "W" in warn color tells operator the AP exposes
     * WPS IE in its beacon and is a PIN-attack candidate (Pixie Dust
     * / Reaver). Slot it between auth and SSID at col 88. */
    if (a.wps) {
        d.setTextColor(T_WARN, bg);
        d.setCursor(88, y);
        d.print("W");
    }
    d.setTextColor(fg, bg);
    ui_label(94, y, 138, fg, bg, a.ssid[0] ? a.ssid : "(hidden)");
    if (sel) d.fillRect(0, y, 2, 8, T_ACCENT);
}

/* Full window repaint over a one-time body clear. Used at entry and after
 * any full-screen event (detail view, help modal) overwrites the body. */
static void draw_list(int cursor)
{
    auto &d = M5Cardputer.Display;
    d.fillRect(0, BODY_Y + 15, SCR_W, 4, T_BG);
    d.fillRect(0, BODY_Y + 107, SCR_W, BODY_H - 107, T_BG);
    draw_list_header();

    int idx[MAX_APS];
    int n = build_filtered(idx);
    if (n == 0) {
        M5Cardputer.Display.fillRect(0, BODY_Y + 16, SCR_W, BODY_H - 16, T_BG);
        auto &d = M5Cardputer.Display;
        d.setTextColor(T_DIM, T_BG);
        d.setCursor(4, BODY_Y + 18);
        d.print(s_scan_running ? "Scanning for networks..." : "No networks match this filter.");
        return;
    }
    if (cursor >= n) cursor = n - 1;
    if (cursor < 0) cursor = 0;

    int first = cursor - SCAN_ROWS / 2;
    if (first < 0) first = 0;
    if (first + SCAN_ROWS > n) first = max(0, n - SCAN_ROWS);

    for (int r = 0; r < SCAN_ROWS; ++r) {
        if (first + r < n) draw_ap_row(r, idx, first, cursor);
        else d.fillRect(0, BODY_Y + 19 + r * (ui_big_text() ? 29 : 11),
                        SCR_W, ui_big_text() ? 29 : 11, T_BG);
    }
    ui_scrollbar(237, BODY_Y + 19, 88, first, SCAN_ROWS, n);
}

/* Other features use g_last_selected_ap. We set it here so the user
 * can jump directly from the AP detail view into an attack. */
extern void feat_wifi_deauth(void);
extern void feat_wifi_deauth_broadcast(void);
extern void feat_wifi_deauth_broadcast_ap(const ap_t &a);
extern void feat_wifi_apclone(void);
extern void feat_wifi_portal(void);
extern void feat_wifi_clients(void);

/* Returns a ui_state hint — but we just call the feature directly and
 * return once it finishes. */
/* Exposed (non-static) so feat_c5_scan_5g can reuse the same detail
 * screen — same hotkeys, same C5/local dispatch, no duplicate code. */
void wifi_show_ap_details(const ap_t &a)
{
    ui_clear_body();
    auto &d = M5Cardputer.Display;
    d.setTextColor(T_ACCENT, T_BG);
    d.setCursor(4, BODY_Y + 2);  d.print("AP DETAILS");
    if (a.is_5g) {
        d.setTextColor(T_GOOD, T_BG);
        d.setCursor(SCR_W - 28, BODY_Y + 2); d.print("[5G]");
    }
    d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT);

    uint32_t oui = ((uint32_t)a.bssid[0] << 16) | ((uint32_t)a.bssid[1] << 8) | a.bssid[2];
    const char *vendor = ble_db_oui(oui);

    d.setTextColor(T_FG, T_BG);
    ui_text(4, BODY_Y + 16, T_FG, "SSID %s", a.ssid);
    d.setCursor(4, BODY_Y + 28); d.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
        a.bssid[0], a.bssid[1], a.bssid[2], a.bssid[3], a.bssid[4], a.bssid[5]);
    d.setTextColor(T_DIM, T_BG);
    ui_text(4, BODY_Y + 40, T_DIM, "MFR  : %s", vendor ? vendor : "Unknown / Generic");
    d.setTextColor(T_FG, T_BG);
    d.setCursor(4, BODY_Y + 52); d.printf("CH   : %-3u   AUTH: %s%s", a.channel, auth_str(a.auth), a.wps ? " (WPS!)" : "");

    /* Signal Bar */
    int bar_w = 110;
    int pct = (a.rssi + 100) * 100 / 70;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    d.setCursor(4, BODY_Y + 64); d.printf("RSSI : %d dBm", a.rssi);
    d.drawRect(95, BODY_Y + 64, bar_w, 7, T_DIM);
    uint16_t col = (a.rssi > -60) ? T_GOOD : (a.rssi > -80) ? T_WARN : T_BAD;
    d.fillRect(96, BODY_Y + 65, (bar_w - 2) * pct / 100, 5, col);

    /* Actions */
    d.setTextColor(T_ACCENT2, T_BG);
    d.setCursor(4, BODY_Y + 76);
    if (a.is_5g) {
        d.print("D=deauth  X=broadcast (via C5)");
        d.setCursor(4, BODY_Y + 87);
        d.print("`=back to scan list");
    } else {
        d.print("D=deauth X=bcast M=pmkid C=clone");
        d.setCursor(4, BODY_Y + 87);
        d.print("L=clients P=portal   `=back");
    }
    ui_draw_footer("pick an action");

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(10); continue; }
        if (k == PK_ESC) return;

        switch ((char)tolower((int)k)) {
        case 'd':
            if (a.is_5g) {
                if (!c5_any_online()) { ui_toast("no C5 paired", T_BAD, 1200); break; }
                c5_deauth_dashboard(a, false);
            } else {
                feat_wifi_deauth();
            }
            return;
        case 'x':
            if (a.is_5g) {
                if (!c5_any_online()) { ui_toast("no C5 paired", T_BAD, 1200); break; }
                c5_deauth_dashboard(a, true);   /* broadcast on the AP's ch */
            } else {
                feat_wifi_deauth_broadcast_ap(a);
            }
            return;
        case 'm':
            if (a.is_5g) { ui_toast("pmkid: 2.4G only", T_WARN, 1000); break; }
            feat_wifi_pmkid(); return;
        case 'l':
            if (a.is_5g) { ui_toast("clients: 2.4G only", T_WARN, 1000); break; }
            feat_wifi_clients(); return;
        case 'c':
            if (a.is_5g) { ui_toast("clone: 2.4G only", T_WARN, 1000); break; }
            feat_wifi_apclone(); return;
        case 'p':
            if (a.is_5g) { ui_toast("portal: 2.4G only", T_WARN, 1000); break; }
            feat_wifi_portal();  return;
        }
    }
}

/* ---- Animated scanning screen -------------------------------------------
 * The raw esp_wifi scan runs non-blocking (blocking=false) so esp_wifi's own
 * task does the channel hopping while THIS task stays free to animate a radar
 * sweep. A SCAN_DONE event flips the flag. We never spawn our own scan task —
 * that overflowed the stack even at 8 KB (see the note in feat_wifi_scan). */
static volatile bool s_scan_evt_done = false;
static void wifi_scan_evt_cb(void *, esp_event_base_t base, int32_t id, void *) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) s_scan_evt_done = true;
}
static void ensure_scan_evt(void) {
    static bool reg = false;
    if (!reg) {
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, wifi_scan_evt_cb, nullptr);
        reg = true;
    }
}

static int s_painted_pass = -1;
static int s_painted_found = -1;

/* Static chrome, drawn once before the sweep starts. */
static void scan_screen_static(void) {
    ui_clear_body();
    ui_header("WIFI SCAN", "LIVE");
    ui_label(8, BODY_Y + 27, 224, T_FG, T_BG, "Finding networks", 2);
    ui_label(8, BODY_Y + 51, 224, T_DIM, T_BG, "2.4 GHz / all channels");
    s_painted_pass = -1;
    s_painted_found = -1;
    ui_draw_footer("` cancel");
}

/* Small busy indicator; counters repaint only when actual data changes. */
static void scan_screen_frame(int pass, int found, uint32_t elapsed) {
    auto &d = M5Cardputer.Display;
    (void)elapsed;
    if (pass != s_painted_pass) {
        s_painted_pass = pass;
        if (pass <= 2) ui_text_w(8, BODY_Y + 72, 174, T_DIM, "Local pass %d / 2", pass);
        else ui_label(8, BODY_Y + 72, 174, T_DIM, T_BG, "Satellite merge");
    }
    if (found != s_painted_found) {
        s_painted_found = found;
        ui_text_w(8, BODY_Y + 91, 174, T_GOOD, "%d networks found", found);
    }
    d.fillRect(198, BODY_Y + 68, 26, 26, T_BG);
    ui_radar(211, BODY_Y + 81, 10, T_ACCENT);
}

/* Run one scan pass non-blocking, animating until SCAN_DONE. Returns false if
 * the user cancelled with ESC. */
static bool scan_pass_animated(wifi_scan_config_t *scfg, int pass) {
    ensure_scan_evt();
    s_scan_evt_done = false;
    if (esp_wifi_scan_start(scfg, false) != ESP_OK) return true;  /* let record read handle it */
    uint32_t t0 = millis();
    while (!s_scan_evt_done) {
        scan_screen_frame(pass, s_ap_count, millis() - t0);
        uint16_t key = input_poll();
        if (key == PK_ESC) { esp_wifi_scan_stop(); return false; }
        delay(35);
        if (millis() - t0 > 9000) break;      /* safety: never hang forever */
    }
    return true;
}

void feat_wifi_scan(void)
{
    static int s_saved_cursor = 0;     /* remembered across re-entries */
    static bool s_have_results = false; /* skip re-scan if last scan still fresh */
    radio_switch(RADIO_WIFI);
    s_filter[0] = '\0';
    s_filter_open_only = false;

    /* Outer loop so R=rescan re-runs the inline animated scan on THIS task.
     * The old background scan_task hung on Arduino WiFi.scanNetworks over a
     * raw-inited driver and raced the feature exit; the inline non-blocking
     * scan (esp_wifi's own task hops channels while we animate) is safe. */
    for (;;) {
    ui_draw_status(radio_name(), s_have_results ? "cached" : "scanning...");

    if (!s_have_results) {
        s_ap_count = 0;
        s_scan_running = true;
        s_scan_done = false;
        if (!c5_any_online()) {
            /* Only switch radio if we don't need to keep ESP-NOW alive
             * for a follow-up 5 GHz C5 merge. Switching toggles WiFi
             * init which fragments heap. */
        }
        Serial.printf("[wifi_scan] pre-scan heap=%u dma=%u\n",
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
        wifi_lean_sta_init();
        /* Raw IDF scan — Arduino's WiFi.scanNetworks needs WiFi inited
         * via Arduino's wifiLowLevelInit path, which uses default
         * buffer counts and fails NO_MEM on Cardputer-Adv. We inited
         * via raw IDF above with shrunk buffers; that bypassed
         * Arduino's init so Arduino's event handlers aren't registered
         * and WiFi.scanNetworks blocks forever waiting for SCAN_DONE.
         * Solution: use raw esp_wifi_scan_start(blocking=true) which
         * doesn't depend on Arduino event dispatch. */
        wifi_scan_config_t scfg = {};
        scfg.show_hidden       = true;
        scfg.scan_type         = WIFI_SCAN_TYPE_ACTIVE;
        scfg.scan_time.active.min = 80;
        scfg.scan_time.active.max = 250;
        scan_screen_static();
        bool ok1 = scan_pass_animated(&scfg, 1);
        esp_err_t scan_rc = ok1 ? ESP_OK : ESP_FAIL;
        uint16_t n_u = 0;
        esp_wifi_scan_get_ap_num(&n_u);
        int n = (scan_rc == ESP_OK) ? (int)n_u : -2;
        Serial.printf("[wifi_scan] raw pass1 rc=%d count=%d\n", (int)scan_rc, n);
        if (n > 0) {
            /* Pull raw wifi_ap_record_t for the WPS bit — Arduino's
             * WiFi.* helpers don't expose it. Same trick as the async
             * scan path above. */
            uint16_t want = (uint16_t)((n < MAX_APS) ? n : MAX_APS);
            wifi_ap_record_t *recs = (wifi_ap_record_t *)heap_caps_malloc(
                want * sizeof(wifi_ap_record_t), MALLOC_CAP_8BIT);
            bool got_records = false;
            if (recs) {
                uint16_t got = want;
                if (esp_wifi_scan_get_ap_records(&got, recs) == ESP_OK && got > 0) {
                    got_records = true;
                    for (int i = 0; i < (int)got && s_ap_count < MAX_APS; ++i) {
                        ap_t &a = s_aps[s_ap_count++];
                        strncpy(a.ssid, (const char *)recs[i].ssid, sizeof(a.ssid) - 1);
                        a.ssid[sizeof(a.ssid) - 1] = '\0';
                        memcpy(a.bssid, recs[i].bssid, 6);
                        a.rssi    = recs[i].rssi;
                        a.channel = recs[i].primary;
                        a.auth    = (uint8_t)recs[i].authmode;
                        a.is_5g   = false;
                        a.wps     = recs[i].wps ? true : false;
                        if (a.wps) {
                            Serial.printf("[wifi_scan] WPS ap '%s' ch%u %ddBm\n",
                                          a.ssid, a.channel, a.rssi);
                        }
                    }
                }
                free(recs);
            }
            if (!got_records) {
                for (int i = 0; i < n && s_ap_count < MAX_APS; ++i) {
                    ap_t &a = s_aps[s_ap_count++];
                    strncpy(a.ssid, WiFi.SSID(i).c_str(), sizeof(a.ssid) - 1);
                    a.ssid[sizeof(a.ssid) - 1] = '\0';
                    memcpy(a.bssid, WiFi.BSSID(i), 6);
                    a.rssi    = WiFi.RSSI(i);
                    a.channel = WiFi.channel(i);
                    a.auth    = (uint8_t)WiFi.encryptionType(i);
                    a.is_5g   = false;
                    a.wps     = false;
                }
            }
        }
        /* No WiFi.scanDelete needed — raw IDF auto-frees. */

        /* Second pass via raw IDF too. */
        esp_err_t scan_rc2 = (ok1 && scan_pass_animated(&scfg, 2)) ? ESP_OK : ESP_FAIL;
        uint16_t n2_u = 0;
        esp_wifi_scan_get_ap_num(&n2_u);
        int n2 = (scan_rc2 == ESP_OK) ? (int)n2_u : -2;
        Serial.printf("[wifi_scan] raw pass2 rc=%d count=%d\n", (int)scan_rc2, n2);
        if (n2 > 0) {
            uint16_t want2 = (uint16_t)((n2 < MAX_APS) ? n2 : MAX_APS);
            wifi_ap_record_t *recs2 = (wifi_ap_record_t *)heap_caps_malloc(
                want2 * sizeof(wifi_ap_record_t), MALLOC_CAP_8BIT);
            if (recs2) {
                uint16_t got2 = want2;
                if (esp_wifi_scan_get_ap_records(&got2, recs2) == ESP_OK) {
                    for (int i = 0; i < (int)got2 && s_ap_count < MAX_APS; ++i) {
                        /* Skip if BSSID already in list. */
                        bool dup = false;
                        for (int j = 0; j < s_ap_count; ++j) {
                            if (memcmp(s_aps[j].bssid, recs2[i].bssid, 6) == 0) {
                                /* Refresh RSSI to the stronger reading. */
                                if (recs2[i].rssi > s_aps[j].rssi)
                                    s_aps[j].rssi = recs2[i].rssi;
                                dup = true;
                                break;
                            }
                        }
                        if (dup) continue;
                        ap_t &a = s_aps[s_ap_count++];
                        strncpy(a.ssid, (const char *)recs2[i].ssid, sizeof(a.ssid) - 1);
                        a.ssid[sizeof(a.ssid) - 1] = '\0';
                        memcpy(a.bssid, recs2[i].bssid, 6);
                        a.rssi    = recs2[i].rssi;
                        a.channel = recs2[i].primary;
                        a.auth    = (uint8_t)recs2[i].authmode;
                        a.is_5g   = false;
                        a.wps     = recs2[i].wps ? true : false;
                    }
                }
                free(recs2);
            }
            WiFi.scanDelete();
        }
        Serial.printf("[wifi_scan] inline merged total=%d\n", s_ap_count);

        /* Merge 5 GHz APs from a paired C5/TRIDENT satellite over ESP-NOW so
         * the list isn't 2.4 GHz only. This used to live only on the async
         * rescan task; folding it inline means EVERY scan gets the 5 GHz band. */
        if (c5_any_online()) {
            c5_clear_results();
            c5_cmd_scan_5g(600);
            uint32_t deadline = millis() + 2000;
            int      last_n   = 0;
            uint32_t last_chk = 0;
            while (millis() < deadline) {
                /* Keep the scan screen alive — animate the same radar so the
                 * 5 GHz merge wait doesn't read as a freeze. */
                scan_screen_frame(3, s_ap_count, millis());
                if (millis() - last_chk > 150) {
                    last_chk = millis();
                    c5_ap_t tmp[4];
                    int cur = c5_aps(tmp, 4);
                    if (cur == last_n && cur > 0) break;   /* quiesced */
                    last_n = cur;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            c5_ap_t c5aps[64];
            int c5n = c5_aps(c5aps, 64);
            for (int i = 0; i < c5n && s_ap_count < MAX_APS; ++i) {
                if (!c5aps[i].is_5g) continue;
                ap_t &a = s_aps[s_ap_count++];
                strncpy(a.ssid, c5aps[i].ssid, sizeof(a.ssid) - 1);
                a.ssid[sizeof(a.ssid) - 1] = '\0';
                memcpy(a.bssid, c5aps[i].bssid, 6);
                a.rssi    = c5aps[i].rssi;
                a.channel = c5aps[i].channel;
                a.auth    = c5aps[i].auth;
                a.is_5g   = true;
                a.wps     = false;   /* C5 satellite doesn't surface WPS IE yet */
            }
            Serial.printf("[wifi_scan] merged %d 5G APs from C5\n", c5n);
        }

        s_scan_running = false;
        s_scan_done = true;
    }

    ui_draw_footer("/=flt O=open S=save R=rescan ENTER=info `=back");
    ui_force_clear_body();
    draw_list(s_saved_cursor);

    int cursor = s_saved_cursor;
    bool rescan = false;   /* R sets this to break out and re-run the scan inline */
    /* Track the last-painted state so we only redraw incrementally. The
     * async scan task bumps s_ap_count as new APs land. A full draw_list
     * (body clear) runs only at entry / after a modal; steady-state moves
     * and new-AP arrivals repaint just the affected rows + header. */
    int  last_count   = -1;
    int  last_cursor  = -1;
    int  last_first   = -1;
    bool last_running = !s_scan_running;      /* force first paint */
    while (true) {
        int idx[MAX_APS];
        int n = build_filtered(idx);
        int first = 0;
        if (n > 0) {
            if (cursor >= n) cursor = n - 1;
            if (cursor < 0) cursor = 0;
            first = cursor - SCAN_ROWS / 2;
            if (first < 0) first = 0;
            if (first + SCAN_ROWS > n) first = max(0, n - SCAN_ROWS);
        } else {
            cursor = 0;
        }

        bool count_changed   = (s_ap_count != last_count);
        bool cursor_changed  = (cursor != last_cursor);
        bool first_changed   = (first != last_first);
        bool running_changed = (s_scan_running != last_running);
        bool state_changed = count_changed || cursor_changed ||
                             first_changed || running_changed;

        if (state_changed) {
            ui_draw_status(radio_name(), s_scan_running ? "..." : "done");
            auto &d = M5Cardputer.Display;
            /* last_count == -1 is the "force full rebuild" sentinel set by
             * the filter/help key handlers — body changed wholesale. */
            if (last_count < 0) {
                draw_list(cursor);
            } else if (n == 0) {
                if (count_changed || running_changed) {
                    d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 14, T_BG);
                    draw_list_header();
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(4, BODY_Y + 18);
                    d.print(s_scan_running ? "scanning..." : "no matches");
                }
            } else {
                if (count_changed || running_changed) draw_list_header();
                if (count_changed || first_changed) {
                    for (int r = 0; r < SCAN_ROWS && first + r < n; ++r)
                        draw_ap_row(r, idx, first, cursor);
                } else {
                    int old_r = last_cursor - first;
                    int new_r = cursor - first;
                    if (old_r >= 0 && old_r < SCAN_ROWS && first + old_r < n)
                        draw_ap_row(old_r, idx, first, cursor);
                    if (new_r >= 0 && new_r < SCAN_ROWS && first + new_r < n)
                        draw_ap_row(new_r, idx, first, cursor);
                }
            }
            last_count   = s_ap_count;
            last_cursor  = cursor;
            last_first   = first;
            last_running = s_scan_running;
            ui_scrollbar(237, BODY_Y + 19, 88, first, SCAN_ROWS, n);
        }
        /* Radar sweep in top-right while scanning. */
        if (s_scan_running) ui_radar(SCR_W - 16, BODY_Y + 8, 7, 0x07FF);

        if (s_scan_done) {
            /* Only cache a scan result if it actually found something —
             * otherwise re-entries would be stuck on cached zeros forever
             * without the user knowing to press R. */
            if (s_ap_count > 0) s_have_results = true;
            s_scan_done = false;
        }
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) { s_saved_cursor = cursor; return; }

        switch (k) {
        /* Key handlers only update state — the state-change gate at the
         * top of the loop draws exactly once per change. Direct draw_list
         * calls here would double-paint. */
        case ';': case PK_UP:
            cursor--; if (cursor < 0) cursor = 0; break;
        case '.': case PK_DOWN:
            cursor++; break;
        case 'r': case 'R':
            if (!s_scan_running) {
                s_ap_count = 0;
                s_have_results = false;
                rescan = true;   /* break to the outer loop → inline animated rescan */
            }
            break;
        case '?':
            ui_show_current_help();
            last_count = -1;    /* force redraw of list after help modal */
            ui_draw_footer("/=flt O=open S=save R=rescan ENTER=info `=back");
            break;
        case 'o': case 'O':
            s_filter_open_only = !s_filter_open_only;
            last_count = -1;    /* filter toggle — force redraw */
            break;
        case '/':
            if (!input_line("Filter SSID contains:", s_filter, sizeof(s_filter))) {
                s_filter[0] = '\0';
            }
            last_count = -1;    /* filter changed — force redraw */
            break;
        case 's': case 'S': {
            if (s_ap_count == 0) { ui_toast("no results", T_WARN, 800); last_count = -1; break; }
            /* Keep the highlighted AP as the next Connect target. The CSV
             * export remains a report; credentials are still collected only
             * by WiFi Connect. */
            int selected_idx[MAX_APS];
            int selected_n = 0;
            for (int i = 0; i < s_ap_count; ++i)
                if (ap_matches_filter(s_aps[i])) selected_idx[selected_n++] = i;
            if (selected_n > 0 && cursor < selected_n) {
                g_last_selected_ap = s_aps[selected_idx[cursor]];
                g_last_selected_valid = true;
            }
            char path[64];
            File f = sdlog_open_in(SD_WIFI_CAPTURE_DIR, "wifiscan",
                                   "ssid,bssid,channel,rssi,auth",
                                   path, sizeof(path));
            if (!f) { ui_toast("SD open failed", T_BAD, 1000); last_count = -1; break; }
            int wrote = 0;
            for (int i = 0; i < s_ap_count; ++i) {
                if (!ap_matches_filter(s_aps[i])) continue;
                const ap_t &a = s_aps[i];
                f.printf("\"%s\",%02X:%02X:%02X:%02X:%02X:%02X,%u,%d,%s\n",
                         a.ssid,
                         a.bssid[0], a.bssid[1], a.bssid[2],
                         a.bssid[3], a.bssid[4], a.bssid[5],
                         (unsigned)a.channel, (int)a.rssi, auth_str(a.auth));
                wrote++;
            }
            f.close();
            char msg[32];
            snprintf(msg, sizeof(msg), "saved %d APs", wrote);
            ui_toast(msg, T_GOOD, 1000);
            last_count = -1;    /* toast overwrote body — full rebuild */
            Serial.printf("[wifi_scan] saved %s (%d rows)\n", path, wrote);
            break;
        }
        case PK_ENTER: {
            /* Show details of selected item. */
            int idx[MAX_APS];
            int n = 0;
            for (int i = 0; i < s_ap_count; ++i)
                if (ap_matches_filter(s_aps[i])) idx[n++] = i;
            if (n > 0 && cursor < n) {
                g_last_selected_ap    = s_aps[idx[cursor]];
                g_last_selected_valid = true;
                wifi_show_ap_details(g_last_selected_ap);
                last_count = -1;    /* detail view overwrote body — full rebuild */
                ui_draw_footer("/=flt O=open S=save R=rescan ENTER=info `=back");
            }
            break;
        }
        default: break;
        }
        if (rescan) break;   /* re-run the scan via the outer loop */
    }
    }   /* outer rescan loop */
}
