/*
 * drone_remoteid — passive ASTM F3411-22a Drone Remote ID detector.
 *
 * Most US drones over 250 g must broadcast Remote ID per FAA Part 89.
 * The standard mandates a Bluetooth 4 Legacy Advertisement on the
 * advertising channels (37/38/39) with Service Data UUID 0xFFFA
 * containing one of six ASTM message types:
 *
 *   0x0  Basic ID         UAS ID + UA type
 *   0x1  Location/Vector  lat/lon/alt + speed + heading
 *   0x2  Authentication
 *   0x3  Self-ID          operator note (e.g. flight purpose)
 *   0x4  System           operator location + classification
 *   0x5  Operator ID      operator identifier
 *
 * This scanner passively listens for those frames, decodes the
 * Basic ID + Location, and shows live target list with operator
 * location when available. SD log to /poseidon/captures/ble/drone-<ts>.jsonl
 * with timestamps + observed RSSI.
 *
 * Source: ASTM F3411-22a spec + GhostBLE decoder pattern (SmonSE).
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "gps.h"
#include "sfx.h"
#include "../sd_helper.h"
#include "../sigdb_bt.h"
#include "../odid_decode.h"
#include <NimBLEDevice.h>
#include <SD.h>
#include <math.h>

#define ASTM_SVC_UUID16   0xFFFA


struct drone_t {
    char     uas_id[21];     /* CAA / ANSI/CTA-2063 */
    uint8_t  src[6];
    double   lat_deg;
    double   lon_deg;
    float    alt_m;
    float    speed_ms;
    float    track_deg;
    double   op_lat_deg;
    double   op_lon_deg;
    int8_t   rssi;
    uint32_t last_ms;
    uint8_t  ua_type;        /* aeroplane / helicopter / etc. */
    bool     have_loc;
    bool     have_op;
};

#define DRONE_N 8
static drone_t s_drones[DRONE_N];
static int     s_drone_count = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static File    s_log;
static char    s_log_path[64] = {0};

static int find_or_add_drone(const uint8_t src[6], bool *is_new)
{
    if (is_new) *is_new = false;
    for (int i = 0; i < s_drone_count; i++)
        if (memcmp(s_drones[i].src, src, 6) == 0) return i;
    if (is_new) *is_new = true;
    if (s_drone_count >= DRONE_N) {
        /* Evict oldest. */
        int oldest = 0;
        for (int i = 1; i < DRONE_N; i++)
            if (s_drones[i].last_ms < s_drones[oldest].last_ms) oldest = i;
        memset(&s_drones[oldest], 0, sizeof(drone_t));
        memcpy(s_drones[oldest].src, src, 6);
        return oldest;
    }
    int idx = s_drone_count++;
    memset(&s_drones[idx], 0, sizeof(drone_t));
    memcpy(s_drones[idx].src, src, 6);
    return idx;
}

static void log_event(const drone_t &d, uint8_t msg_type)
{
    if (!s_log) return;
    gps_fix_t g;
    bool have_obs = gps_snapshot(&g);
    s_log.printf("{\"ts\":%lu,\"msg_type\":%u,\"src\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                 "\"uas_id\":\"%s\",\"rssi\":%d,"
                 "\"drone_lat\":%.6f,\"drone_lon\":%.6f,\"drone_alt\":%.1f,"
                 "\"op_lat\":%.6f,\"op_lon\":%.6f,"
                 "\"obs_lat\":%.6f,\"obs_lon\":%.6f}\n",
                 (unsigned long)d.last_ms, (unsigned)msg_type,
                 d.src[0], d.src[1], d.src[2], d.src[3], d.src[4], d.src[5],
                 d.uas_id,
                 (int)d.rssi,
                 d.lat_deg, d.lon_deg, (double)d.alt_m,
                 d.op_lat_deg, d.op_lon_deg,
                 have_obs ? g.lat_deg : 0.0,
                 have_obs ? g.lon_deg : 0.0);
    s_log.flush();
}

/* `sd` is the 0xFFFA service-data body after the UUID: app code 0x0D, counter,
 * then one message or a pack. Decoding lives in odid_decode.h (native-tested);
 * this only merges the result into the drone table and logs it. */
static void decode_astm(const uint8_t *sd, size_t len, const uint8_t src[6], int8_t rssi)
{
    odid_info_t info = {};
    if (odid_decode_ble_svc(sd, len, &info) <= 0) return;
    uint8_t msg_type = (uint8_t)(sd[2] >> 4);       /* 0xF for a pack */

    bool newly_added = false;
    portENTER_CRITICAL(&s_mux);
    int idx = find_or_add_drone(src, &newly_added);
    drone_t &dr = s_drones[idx];
    dr.rssi    = rssi;
    dr.last_ms = millis();
    if (info.have_id) {
        memcpy(dr.uas_id, info.uas_id, sizeof(dr.uas_id));
        dr.ua_type = info.ua_type;
    }
    if (info.have_loc) {
        dr.track_deg = info.track_deg;
        dr.speed_ms  = info.speed_ms;
        dr.lat_deg   = info.lat_deg;
        dr.lon_deg   = info.lon_deg;
        dr.alt_m     = info.alt_geo_m;
        dr.have_loc  = true;
    }
    if (info.have_op) {
        dr.op_lat_deg = info.op_lat_deg;
        dr.op_lon_deg = info.op_lon_deg;
        dr.have_op    = true;
    }
    drone_t snap = dr;
    portEXIT_CRITICAL(&s_mux);

    if (newly_added) sfx_scan_hit();
    log_event(snap, msg_type);
}

class DroneScanCb : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override {
        /* Find ASTM service data (UUID 0xFFFA). NimBLE doesn't have a
         * direct "service data by UUID16" accessor in 2.x so we walk
         * the raw payload. Service Data 16-bit type = 0x16. */
        const std::vector<uint8_t> &payload = dev->getPayload();
        const uint8_t *p = payload.data();
        size_t plen = payload.size();
        size_t i = 0;
        while (i + 1 < plen) {
            uint8_t l = p[i];
            if (l == 0 || i + 1 + l > plen) break;
            uint8_t t = p[i + 1];
            if (t == 0x16 && l >= 3) {
                uint16_t svc = (uint16_t)p[i + 2] | ((uint16_t)p[i + 3] << 8);
                if (svc == ASTM_SVC_UUID16 && l > 3) {
                    uint8_t addr[6];
                    memcpy(addr, dev->getAddress().getBase()->val, 6);
                    decode_astm(p + i + 4, l - 3, addr, dev->getRSSI());
                }
            }
            i += 1 + l;
        }
    }
};
static DroneScanCb s_cb;

static void show_drone_detail(const drone_t &dr)
{
    auto &d = M5Cardputer.Display;
    ui_clear_body();
    d.setTextColor(T_ACCENT, T_BG);
    d.setCursor(4, BODY_Y + 2); d.print("DRONE TELEMETRY HUD");
    d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT);

    d.setTextColor(T_GOOD, T_BG);
    d.setCursor(4, BODY_Y + 16);
    d.printf("UAS ID: %.20s", dr.uas_id[0] ? dr.uas_id : "<unregistered>");

    d.setTextColor(T_FG, T_BG);
    d.setCursor(4, BODY_Y + 28);
    d.printf("BLE MAC: %02X:%02X:%02X:%02X:%02X:%02X (%d dBm)",
             dr.src[0], dr.src[1], dr.src[2], dr.src[3], dr.src[4], dr.src[5], (int)dr.rssi);

    d.setTextColor(T_ACCENT2, T_BG);
    d.setCursor(4, BODY_Y + 40);
    if (dr.have_loc) {
        d.printf("DRONE: %.5f, %.5f", dr.lat_deg, dr.lon_deg);
    } else {
        d.print("DRONE: (location pending)");
    }

    d.setTextColor(T_WARN, T_BG);
    d.setCursor(4, BODY_Y + 52);
    d.printf("ALT: %-4.0fm  SPD: %-4.1fm/s  HDG: %-3.0f°", (double)dr.alt_m, (double)dr.speed_ms, (double)dr.track_deg);

    d.setTextColor(T_BAD, T_BG);
    d.setCursor(4, BODY_Y + 64);
    if (dr.have_op) {
        d.printf("PILOT: %.5f, %.5f", dr.op_lat_deg, dr.op_lon_deg);
    } else {
        d.print("PILOT: (operator GPS not broadcast)");
    }

    uint32_t age = (millis() - dr.last_ms) / 1000;
    d.setTextColor(T_DIM, T_BG);
    d.setCursor(4, BODY_Y + 76);
    d.printf("Last beacon: %lu sec ago", (unsigned long)age);

    ui_draw_footer("`=back to list");
    sfx_select();

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) break;
    }
}

void feat_drone_remoteid(void)
{
    if (!sd_mount() && !sd_remount()) {
        ui_toast("SD mount failed", T_BAD, 1200);
        return;
    }
    uint32_t ts = millis() / 1000;
    if (!sd_ensure_layout()) { ui_toast("cant create log dir", T_BAD, 1200); return; }
    snprintf(s_log_path, sizeof(s_log_path),
             SD_BLE_CAPTURE_DIR "/drone-%lu.jsonl", (unsigned long)ts);
    s_log = SD.open(s_log_path, FILE_WRITE);
    if (!s_log) { ui_toast("cant open log", T_BAD, 1200); return; }

    radio_switch(RADIO_BLE);
    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("");
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    }
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&s_cb, false);
    scan->setMaxResults(0);
    scan->setActiveScan(false);
    scan->setInterval(97);
    scan->setWindow(67);
    scan->setDuplicateFilter(false);

    /* Reset state. */
    s_drone_count = 0;
    memset(s_drones, 0, sizeof(s_drones));

    scan->start(0, false, false);   /* run until stopped */

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer(";/.=move  ENTER=details  `=stop");

    int cursor = 0;
    uint32_t last_redraw = 0;
    int last_count = -1;
    int last_cursor = -1;

    while (true) {
        uint32_t now = millis();
        if (now - last_redraw > 300) {
            last_redraw = now;
            ui_draw_status(radio_name(), "drone RID");

            if (s_drone_count != last_count || cursor != last_cursor) {
                last_count = s_drone_count;
                last_cursor = cursor;

                ui_clear_body();
                d.setTextColor(T_ACCENT, T_BG);
                d.setCursor(4, BODY_Y + 2); d.print("DRONE REMOTE ID");
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(SCR_W - 74, BODY_Y + 2);
                d.printf("%d targets", s_drone_count);
                d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT);

                if (s_drone_count == 0) {
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(4, BODY_Y + 26);
                    d.print("Listening on 2.4GHz for FAA Part 89");
                    d.setCursor(4, BODY_Y + 38);
                    d.print("ASTM F3411-22a broadcasts...");
                } else {
                    int rows = 4;
                    if (cursor >= s_drone_count) cursor = s_drone_count - 1;
                    if (cursor < 0) cursor = 0;

                    for (int i = 0; i < rows && i < s_drone_count; i++) {
                        const drone_t &dr = s_drones[i];
                        int y = BODY_Y + 16 + i * 19;
                        bool sel = (i == cursor);
                        if (sel) d.fillRect(0, y - 1, SCR_W, 18, T_SEL_BG);
                        uint16_t bg = sel ? T_SEL_BG : T_BG;

                        d.setTextColor(sel ? T_ACCENT : T_FG, bg);
                        d.setCursor(4, y);
                        d.printf("%-18.18s", dr.uas_id[0] ? dr.uas_id : "<unregistered>");

                        d.setTextColor(sel ? T_FG : T_GOOD, bg);
                        d.setCursor(154, y);
                        d.printf("%4ddBm", (int)dr.rssi);

                        d.setTextColor(T_DIM, bg);
                        d.setCursor(4, y + 9);
                        if (dr.have_loc) {
                            d.printf("%.4f,%.4f %-3.0fm %-2.0fm/s",
                                     dr.lat_deg, dr.lon_deg, (double)dr.alt_m, (double)dr.speed_ms);
                        } else {
                            d.print("(position pending)");
                        }
                    }
                }
            }
        }
        if (s_drone_count == 0) ui_scanning_indicator("waiting for drones", s_drone_count);

        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(25); continue; }
        if (k == PK_ESC) break;
        if (k == ';' || k == PK_UP)   { if (cursor > 0) cursor--; }
        if (k == '.' || k == PK_DOWN) { if (cursor + 1 < s_drone_count) cursor++; }
        if (k == PK_ENTER && s_drone_count > 0 && cursor < s_drone_count) {
            show_drone_detail(s_drones[cursor]);
            last_count = last_cursor = -1;
            ui_draw_footer(";/.=move  ENTER=details  `=stop");
        }
    }

    scan->stop();
    if (s_log) s_log.close();
    delay(100);
}
