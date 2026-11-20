/*
 * c5_godzilla.cpp — GODZILLA detections bench view (C5 menu -> x).
 *
 * Passive and read-only: sends no command and transmits nothing beyond the
 * ESP-NOW link c5_begin() already brings up. It shows what GODZILLA nodes
 * have published (types 60/61, merged by godzilla_det.h via c5_cmd):
 *   - accepted / rejected detection frames and the row count,
 *   - the newest rows: family, RSSI, address tail, node id, age.
 * Keys: c = clear results, ` (ESC) = back.
 *
 * Meant for Dale's first bench run of the 1.85B node: if "frames" rises the
 * wire works; if "rej" rises instead, the node is off-contract (see
 * docs/godzilla-detection-publish-contract.md).
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "c5_cmd.h"
#include "../det_classify.h"
#include <Arduino.h>

/* Four-character family tags that fit the 240 px row. */
static const char *fam_tag(uint8_t f)
{
    switch (f) {
    case DET_FAM_WIFI_AP:     return "AP  ";
    case DET_FAM_WIFI_STA:    return "STA ";
    case DET_FAM_BLE_GENERIC: return "BLE ";
    case DET_FAM_FINDMY:      return "FMY ";
    case DET_FAM_AIRTAG:      return "ATAG";
    case DET_FAM_IBEACON:     return "IBCN";
    case DET_FAM_REMOTEID:    return "RID ";
    case DET_FAM_FLOCK:       return "FLCK";
    case DET_FAM_RAVEN:       return "RAVN";
    case DET_FAM_TILE:        return "TILE";
    case DET_FAM_SMARTTAG:    return "STAG";
    case DET_FAM_SKIMMER:     return "SKIM";
    case DET_FAM_CAMERA:      return "CAM ";
    case DET_FAM_LE_EQUIP:    return "LE  ";
    case DET_FAM_ALPR:        return "ALPR";
    case DET_FAM_SMARTGLASS:  return "GLAS";
    default:                  return "??? ";
    }
}

static uint16_t fam_colour(uint8_t f)
{
    switch (f) {
    case DET_FAM_FLOCK: case DET_FAM_RAVEN: case DET_FAM_REMOTEID:
    case DET_FAM_SKIMMER: case DET_FAM_LE_EQUIP: case DET_FAM_ALPR: return T_BAD;
    case DET_FAM_FINDMY: case DET_FAM_AIRTAG: case DET_FAM_TILE:
    case DET_FAM_SMARTTAG: case DET_FAM_CAMERA: case DET_FAM_SMARTGLASS: return T_WARN;
    default:                                                       return T_FG;
    }
}

/* Snapshot buffer: static, not on loopTask's stack (64 x 24 B). */
static gz_det_t s_rows[GZ_DET_TABLE_MAX];

void feat_c5_godzilla(void)
{
    radio_switch(RADIO_WIFI);
    c5_begin();

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=back  c=clear");
    uint32_t last = 0, last_frames = 0;
    const int ROWS = 6;

    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (k == 'c' || k == 'C') { c5_clear_results(); ui_toast("cleared", T_ACCENT, 400); }

        if (millis() - last > 300) {
            last = millis();
            uint32_t frames = c5_dbg_det_frames();
            uint32_t rej    = c5_dbg_det_rejects();
            int n = c5_dets(s_rows, GZ_DET_TABLE_MAX);
            gz_det_sort_recent(s_rows, n, millis());

            ui_dashboard_chrome("GODZILLA DETS", frames != last_frames);
            last_frames = frames;
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);

            d.setTextColor(T_FG, T_BG);
            d.setCursor(4, BODY_Y + 16);
            d.printf("frames %lu", (unsigned long)frames);
            d.setTextColor(rej ? T_BAD : T_DIM, T_BG);
            d.setCursor(96, BODY_Y + 16);
            d.printf("rej %lu", (unsigned long)rej);
            d.setTextColor(T_DIM, T_BG);
            d.setCursor(160, BODY_Y + 16);
            d.printf("rows %d", n);

            if (n == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 34); d.print("no detections yet");
                d.setCursor(4, BODY_Y + 46); d.print("power a GODZILLA node nearby");
            } else {
                int shown = n < ROWS ? n : ROWS;
                for (int i = 0; i < shown; ++i) {
                    const gz_det_t &e = s_rows[i];
                    int y = BODY_Y + 30 + i * 11;
                    d.setTextColor(fam_colour(e.family), T_BG);
                    d.setCursor(2, y);  d.print(fam_tag(e.family));
                    d.setTextColor(T_FG, T_BG);
                    d.setCursor(32, y); d.printf("%4d", (int)e.rssi);
                    d.setCursor(62, y); d.printf("..%02X:%02X:%02X", e.addr[3], e.addr[4], e.addr[5]);
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(140, y); d.printf("n%u", (unsigned)e.node_id);
                    d.setCursor(170, y);
                    d.printf("%lus", (unsigned long)((millis() - e.last_seen_ms) / 1000));
                    d.setCursor(206, y); d.printf("x%u", (unsigned)e.hits);
                }
                if (n > shown) {
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(4, BODY_Y + BODY_H - 10);
                    d.printf("+%d more", n - shown);
                }
            }
            ui_dashboard_radar();
        }
        delay(20);
    }
}
