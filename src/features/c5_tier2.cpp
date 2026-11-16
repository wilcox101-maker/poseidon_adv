/*
 * UI OWNERSHIP: these C5 menu screens are UI code and are owned by the UI
 * Engineer (see claude/agents/ui-engineer.md). They follow the repo's existing
 * feat_c5_*() function pattern, NOT the screen-object/widget architecture the
 * UIE doc drives toward — keep, refactor, or rebuild as UIE sees fit. The
 * firmware/protocol boundary and the stable command/collector API to build
 * against are written up in claude/c5-ui-handoff.md. The ESP-NOW senders,
 * node handlers, and wire protocol are firmware-side and change separately.
 *
 * c5_tier2.cpp — S3 menu screens for the Phase 1 Tier 2 C5 TX/SoftAP features:
 * beacon spam, CIW (SSID-injection beacon spam), AP clone, and Karma. The
 * ESP-NOW senders already live in c5_cmd; each screen gathers the few params
 * (channel preset, mode/category, or a target AP), fires the sender once, and
 * then polls the RESP_STATUS stream — c5_status_frames()/c5_status_channel() —
 * exactly like feat_c5_deauth_5g's live dashboard. ESC stops (sends CMD_STOP).
 *
 * Channel discipline note (see c5_node/main/softap.h): APCLONE/KARMA host a
 * real SoftAP, and while it sits on a non-ch1 channel the C5's ESP-NOW link to
 * us (ch 1) is unreachable, so a mid-run ESC only lands reliably when the AP is
 * on ch 1. Both screens therefore host on ch 1 by default; the firmware's
 * 5-minute safety cap is the backstop regardless.
 */
#include "app.h"
#include "../theme.h"
#include "ui.h"
#include "input.h"
#include "radio.h"
#include "c5_cmd.h"
#include <Arduino.h>

/* Channel presets the operator cycles with ;/. — a 2.4 GHz spread plus two
 * common 5 GHz channels (the C5's headline: it can beacon-spam on 5 GHz). */
static const uint8_t CH_PRESET[] = { 1, 6, 11, 36, 149 };
#define CH_PRESET_N (sizeof(CH_PRESET) / sizeof(CH_PRESET[0]))
static const char *band_of(uint8_t ch) { return ch <= 14 ? "2.4GHz" : "5GHz"; }

/* Shared entry guard: WiFi radio, bring the C5 link up, bail if nobody home. */
static bool c5_tier2_begin(void)
{
    radio_switch(RADIO_WIFI);
    c5_begin();
    if (!c5_any_online()) { ui_toast("no C5 online", T_BAD, 1500); return false; }
    return true;
}

/* Shared live TX dashboard: polls RESP_STATUS until ESC, then stops. `subtitle`
 * is drawn under the frame count (e.g. the SSID set or category in play). */
static void c5_tx_dashboard(const char *title, const char *subtitle)
{
    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=stop");
    uint32_t last = 0, last_frames = 0;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) { c5_cmd_stop(); break; }
        if (millis() - last > 200) {
            last = millis();
            uint32_t now_frames = c5_status_frames();
            bool tick = (now_frames != last_frames);
            last_frames = now_frames;
            ui_dashboard_chrome(title, tick);
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);
            d.setTextColor(T_ACCENT, T_BG);
            d.setCursor(4, BODY_Y + 16);
            if (now_frames > 0) d.printf("frames: %lu", (unsigned long)now_frames);
            else                d.print("LIVE (count lags)");
            d.setTextColor(T_FG, T_BG);
            d.setCursor(4, BODY_Y + 30);
            d.printf("ch%u via C5", c5_status_channel());
            if (subtitle && subtitle[0]) {
                d.setTextColor(T_DIM, T_BG);
                d.setCursor(4, BODY_Y + 44);
                d.printf("%.30s", subtitle);
            }
            ui_dashboard_radar();
        }
        delay(20);
    }
}

/* Two-field picker (channel preset + a labelled option set). Returns the chosen
 * channel in *ch_out and the option index in *opt_out; false if ESC. */
static bool c5_tx_picker(const char *title, const char *const *opts, int opt_n,
                         uint8_t *ch_out, int *opt_out)
{
    auto &d = M5Cardputer.Display;
    int ch_i = 1;   /* default ch 6 (index 1) */
    int opt  = 0;
    int last = -1;
    ui_draw_footer(";/. chan  m=opt  ENTER=fire  `=back");
    while (true) {
        int sig = ch_i * 97 + opt;
        if (sig != last) {
            last = sig;
            ui_clear_body();
            d.setTextColor(T_ACCENT2, T_BG);
            d.setCursor(4, BODY_Y + 2); d.print(title);
            d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT2);
            d.setTextColor(T_FG, T_BG);
            d.setCursor(6, BODY_Y + 24);
            d.printf("channel : %u  (%s)", CH_PRESET[ch_i], band_of(CH_PRESET[ch_i]));
            d.setTextColor(T_WARN, T_BG);
            d.setCursor(6, BODY_Y + 40);
            d.printf("mode    : %s", opts[opt]);
            d.setTextColor(T_DIM, T_BG);
            d.setCursor(6, BODY_Y + 58);
            d.print(";/. change channel   m change mode");
        }
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) return false;
        if (k == ';' || k == PK_UP)   { ch_i = (ch_i + 1) % CH_PRESET_N; }
        if (k == '.' || k == PK_DOWN) { ch_i = (ch_i + CH_PRESET_N - 1) % CH_PRESET_N; }
        if (k == 'm' || k == 'M')     { opt = (opt + 1) % opt_n; }
        if (k == PK_ENTER) { *ch_out = CH_PRESET[ch_i]; *opt_out = opt; return true; }
    }
}

/* --- Tier 2: 5 GHz / 2.4 GHz beacon spam ----------------------------- */
void feat_c5_beacon_spam(void)
{
    if (!c5_tier2_begin()) return;
    static const char *const MODES[] = { "meme list", "rickroll" };
    uint8_t ch; int mode;
    if (!c5_tx_picker("BEACON SPAM", MODES, 2, &ch, &mode)) return;

    /* mode 0 = built-in meme list, 1 = rickroll-themed set; both firmware-side,
     * so ssid_n=0 / ssids=nullptr. 0 duration = run until STOP. */
    c5_cmd_beacon_spam(ch, 0, (uint8_t)mode, 0, nullptr);
    char sub[32]; snprintf(sub, sizeof(sub), "%s @ %s", MODES[mode], band_of(ch));
    c5_tx_dashboard("C5 BEACON SPAM", sub);
}

/* --- Tier 2: CIW (SSID-injection beacon spam) ------------------------ */
void feat_c5_ciw(void)
{
    if (!c5_tier2_begin()) return;
    static const char *const CATS[] = { "cmd-inject", "fmt-string", "log4j", "xss", "path-trav" };
    uint8_t ch; int cat;
    if (!c5_tx_picker("CIW SSID-INJECT", CATS, 5, &ch, &cat)) return;

    c5_cmd_ciw(ch, 0, (uint8_t)cat, 500);   /* rotate SSID every 500 ms, until STOP */
    char sub[32]; snprintf(sub, sizeof(sub), "cat:%s", CATS[cat]);
    c5_tx_dashboard("C5 CIW INJECT", sub);
}

/* --- Tier 2: AP clone (SoftAP honeypot) ------------------------------ */
void feat_c5_apclone(void)
{
    if (!c5_tier2_begin()) return;

    /* Pick a target to clone. Scan dual-band, list what we found. */
    auto &d = M5Cardputer.Display;
    static c5_ap_t aps[64];
    int n = c5_aps(aps, 64);
    if (n == 0) {
        c5_clear_results();
        c5_cmd_scan_5g(300);
        ui_clear_body();
        d.setTextColor(T_ACCENT2, T_BG);
        d.setCursor(4, BODY_Y + 2); d.print("SCANNING...");
        d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT2);
        ui_draw_footer("`=abort");
        uint32_t deadline = millis() + 20000, stable = millis();
        int last_n = -1;
        while (millis() < deadline) {
            n = c5_aps(aps, 64);
            if (n != last_n) { last_n = n; stable = millis(); }
            d.setTextColor(n ? T_GOOD : T_DIM, T_BG);
            d.setCursor(4, BODY_Y + 30); d.printf("APs: %-3d ", n);
            ui_radar(SCR_W - 24, BODY_Y + 44, 14, T_ACCENT);
            if (n > 0 && millis() - stable > 1500) break;
            if (input_poll() == PK_ESC) break;
            delay(120);
        }
    }
    if (n == 0) { ui_toast("no APs found", T_WARN, 1500); return; }

    int cursor = 0, last_drawn = -2;
    ui_draw_footer(";/. pick  ENTER=clone  `=back");
    int chosen = -1;
    while (true) {
        if (cursor < 0) cursor = 0;
        if (cursor >= n) cursor = n - 1;
        if (cursor != last_drawn) {
            last_drawn = cursor;
            ui_clear_body();
            d.setTextColor(T_ACCENT2, T_BG);
            d.setCursor(4, BODY_Y + 2); d.print("PICK AP TO CLONE");
            d.drawFastHLine(4, BODY_Y + 12, SCR_W - 8, T_ACCENT2);
            int rows = 7, first = cursor - rows / 2;
            if (first < 0) first = 0;
            if (first + rows > n) first = (n > rows) ? (n - rows) : 0;
            for (int r = 0; r < rows && first + r < n; ++r) {
                int i = first + r;
                const c5_ap_t &a = aps[i];
                int y = BODY_Y + 18 + r * 12;
                bool sel = (i == cursor);
                if (sel) d.fillRect(0, y - 1, SCR_W, 12, T_SEL_BG);
                d.setTextColor(sel ? T_ACCENT : T_FG, sel ? T_SEL_BG : T_BG);
                d.setCursor(2, y);
                d.printf("%4d ch%-3u %.18s", a.rssi, a.channel,
                         a.ssid[0] ? a.ssid : "<hidden>");
            }
        }
        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }
        if (k == PK_ESC) return;
        if (k == ';' || k == PK_UP)   { if (cursor > 0) cursor--; }
        if (k == '.' || k == PK_DOWN) { cursor++; }
        if (k == PK_ENTER) { chosen = cursor; break; }
    }

    const c5_ap_t &a = aps[chosen];
    if (!a.ssid[0]) { ui_toast("hidden SSID, can't clone", T_WARN, 1500); return; }
    /* Host the open clone on ch 1 when the source is 5 GHz (5 GHz SoftAP is
     * unreliable on the C5 and would also break the ESP-NOW control link);
     * otherwise mirror the real channel. Open auth so saved-open clients bite. */
    uint8_t apch = (a.channel >= 1 && a.channel <= 13) ? a.channel : 1;
    c5_cmd_apclone(apch, a.ssid, true, nullptr);

    char sub[40]; snprintf(sub, sizeof(sub), "'%.20s' ch%u", a.ssid, apch);
    c5_tx_dashboard("C5 AP CLONE", sub);
}

/* --- Tier 2: Karma (probe-harvest -> SoftAP clone) ------------------- */
void feat_c5_karma(void)
{
    if (!c5_tier2_begin()) return;
    /* Host on ch 1 so the control link stays reachable for ESC; firmware
     * sniffs a directed probe first, then clones that SSID as an open AP. */
    c5_cmd_karma(1, 0);
    c5_tx_dashboard("C5 KARMA", "sniffing probe -> clone");
}
