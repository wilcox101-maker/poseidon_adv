/*
 * c5_tier1.cpp — S3 menu screens for the Phase 1 Tier 1 C5 passive features:
 * probe sniff, deauth detect, client hunt, and spectrum (frame-activity)
 * sweep. The ESP-NOW senders and result collectors already live in c5_cmd;
 * each screen just fires the sender once and polls the matching collector,
 * mirroring feat_c5_scan_zb's dashboard loop. ESC stops (sends CMD_STOP).
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "c5_cmd.h"
#include <Arduino.h>

static void mac6(char *out, const uint8_t m[6])
{
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

/* Shared entry guard: switch to WiFi, bring the C5 link up, bail with a toast
 * if no node answers. Returns false if the caller should abort. */
static bool c5_tier1_begin(void)
{
    radio_switch(RADIO_WIFI);
    c5_begin();
    if (!c5_any_online()) { ui_toast("no C5 online", T_BAD, 1500); return false; }
    c5_clear_results();
    return true;
}

/* --- T1.1: probe-request sniffer ------------------------------------- */
void feat_c5_probe_sniff(void)
{
    if (!c5_tier1_begin()) return;
    c5_cmd_probe_sniff(0, 20000);   /* channel 0 = hop 1..13, 20 s */

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=back");
    int last_n = -1;
    uint32_t last = 0;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (millis() - last > 300) {
            last = millis();
            c5_probe_t pr[32];
            int n = c5_probes(pr, 32);
            ui_dashboard_chrome("C5 PROBE SNIFF", n != last_n);
            last_n = n;
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);
            if (n == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 26);
                d.print("hopping 1-13, listening...");
                d.setCursor(4, BODY_Y + 40);
                d.print("(walk a phone w/ WiFi on)");
            } else {
                int rows = 8;
                int first = n > rows ? n - rows : 0;
                for (int r = 0; r < rows && first + r < n; ++r) {
                    const c5_probe_t &e = pr[first + r];
                    int y = BODY_Y + 16 + r * 12;
                    char mac[18]; mac6(mac, e.src);
                    d.setTextColor(T_ACCENT, T_BG);
                    d.setCursor(2, y);   d.print(mac);
                    d.setTextColor(T_FG, T_BG);
                    d.setCursor(118, y); d.printf("%4d", e.rssi);
                    d.setTextColor(T_WARN, T_BG);
                    d.setCursor(150, y);
                    if (e.ssid_len == 0) d.print("<bcast>");
                    else { char s[34]; int sl = e.ssid_len > 11 ? 11 : e.ssid_len;
                           memcpy(s, e.ssid, sl); s[sl] = 0; d.print(s); }
                }
            }
        }
        delay(20);
    }
    c5_cmd_stop();
}

/* --- T1.2: passive deauth/disassoc detector -------------------------- */
void feat_c5_deauth_detect(void)
{
    if (!c5_tier1_begin()) return;
    c5_cmd_deauth_detect(0, 20000);   /* hop 1..13, 20 s */

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=back");
    int last_n = -1;
    uint32_t last = 0;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (millis() - last > 300) {
            last = millis();
            c5_deauth_hit_t hit[32];
            int n = c5_deauth_hits(hit, 32);
            ui_dashboard_chrome("C5 DEAUTH DETECT", n != last_n);
            last_n = n;
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);
            if (n == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 26);
                d.print("no deauth/disassoc seen");
                d.setCursor(4, BODY_Y + 40);
                d.print("(hopping 1-13)");
            } else {
                int rows = 8;
                int first = n > rows ? n - rows : 0;
                for (int r = 0; r < rows && first + r < n; ++r) {
                    const c5_deauth_hit_t &e = hit[first + r];
                    int y = BODY_Y + 16 + r * 12;
                    char src[18], dst[18]; mac6(src, e.src); mac6(dst, e.dst);
                    d.setTextColor(T_BAD, T_BG);
                    d.setCursor(2, y);   d.print(src);
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(112, y); d.print(">");
                    d.setTextColor(T_FG, T_BG);
                    d.setCursor(120, y);
                    /* broadcast dst is the common case — compress it */
                    bool bcast = (e.dst[0] & e.dst[1] & e.dst[2] & e.dst[3] & e.dst[4] & e.dst[5]) == 0xFF;
                    d.print(bcast ? "bcast" : dst + 12);
                    d.setTextColor(T_WARN, T_BG);
                    d.setCursor(170, y); d.printf("ch%u r%u", e.channel, e.reason);
                }
            }
        }
        delay(20);
    }
    c5_cmd_stop();
}

/* --- T1.3: client (STA) hunt ----------------------------------------- */
void feat_c5_clients(void)
{
    if (!c5_tier1_begin()) return;
    c5_cmd_clients_hunt(20000);   /* all APs, 5 GHz hop, 20 s */

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=back");
    int last_n = -1;
    uint32_t last = 0;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (millis() - last > 300) {
            last = millis();
            c5_sta_t st[48];
            int n = c5_stas(st, 48);
            ui_dashboard_chrome("C5 CLIENT HUNT", n != last_n);
            last_n = n;
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);
            if (n == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 26);
                d.print("sweeping 5 GHz 36-165...");
                d.setCursor(4, BODY_Y + 40);
                d.print("STA<->BSSID pairs appear here");
            } else {
                int rows = 8;
                int first = n > rows ? n - rows : 0;
                for (int r = 0; r < rows && first + r < n; ++r) {
                    const c5_sta_t &e = st[first + r];
                    int y = BODY_Y + 16 + r * 12;
                    char sta[18], bss[18]; mac6(sta, e.sta); mac6(bss, e.bssid);
                    d.setTextColor(T_ACCENT, T_BG);
                    d.setCursor(2, y);   d.print(sta);
                    d.setTextColor(T_DIM, T_BG);
                    d.setCursor(112, y); d.print("@");
                    d.setTextColor(T_FG, T_BG);
                    d.setCursor(120, y); d.print(bss + 9);   /* last 3 octets */
                    d.setTextColor(T_WARN, T_BG);
                    d.setCursor(186, y); d.printf("ch%u", e.channel);
                }
            }
        }
        delay(20);
    }
    c5_cmd_stop();
}

/* --- T1.4: spectrum (frame-activity) sweep --------------------------- */
void feat_c5_spectrum(void)
{
    if (!c5_tier1_begin()) return;
    c5_cmd_spectrum(0);   /* run until STOP */

    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=back");
    uint32_t last = 0;
    bool have = false;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) break;
        if (millis() - last > 300) {
            last = millis();
            c5_spectrum_t b;
            bool ok = c5_spectrum_get(&b);
            ui_dashboard_chrome("C5 ACTIVITY SWEEP", ok && !have);
            if (ok) have = true;
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 20, T_BG);
            if (!ok || b.ch_count == 0) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 26);
                d.print("sweeping... (peak RSSI/ch,");
                d.setCursor(4, BODY_Y + 40);
                d.print(" frame activity not true RF)");
            } else {
                int nb = b.ch_count;
                int x0 = 4, base = BODY_Y + BODY_H - 14;
                int maxh = BODY_H - 26;
                int step = (SCR_W - 8) / (nb > 0 ? nb : 1);
                if (step < 4) step = 4;
                int bw = step - 2 > 2 ? step - 2 : 2;
                for (int i = 0; i < nb; ++i) {
                    int8_t pr = b.ch[i].peak_rssi;
                    /* map -100..-30 dBm → 0..maxh; -128 = nothing heard */
                    int h = 0;
                    if (pr > -128) {
                        int v = pr + 100; if (v < 0) v = 0; if (v > 70) v = 70;
                        h = v * maxh / 70;
                    }
                    int x = x0 + i * step;
                    uint16_t col = (pr > -60) ? T_BAD : (pr > -80 ? T_WARN : T_ACCENT);
                    if (h > 0) d.fillRect(x, base - h, bw, h, col);
                    d.drawPixel(x, base + 1, T_DIM);
                }
                /* a couple of channel labels for orientation */
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(x0, BODY_Y + 2); d.printf("ch%u", b.ch[0].ch);
                char tail[8]; sprintf(tail, "ch%u", b.ch[nb - 1].ch);
                d.setCursor(SCR_W - 34, BODY_Y + 2); d.print(tail);
            }
        }
        delay(20);
    }
    c5_cmd_stop();
}
