/*
 * Deepwater shared chrome and feedback. Live data remains feature-owned.
 */
#include "ui.h"
#include "theme.h"
#include "input.h"
#include "c5_cmd.h"
#include <stdarg.h>
#include <math.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <ui_layout.h>

#include <Preferences.h>

static bool s_big_text_loaded = false;
static bool s_big_text = false;

bool ui_big_text(void)
{
    if (!s_big_text_loaded) {
        Preferences p;
        if (p.begin("pui", false)) {
            s_big_text = p.getBool("bigtxt", false);
            p.end();
        }
        s_big_text_loaded = true;
    }
    return s_big_text;
}

void ui_big_text_set(bool on)
{
    Preferences p;
    if (!p.begin("pui", false)) {
        Serial.println("[UI] cannot open notice preferences");
        ui_toast("Cannot save notice size", T_BAD, 900);
        return;
    }
    const bool saved = p.putBool("bigtxt", on) != 0;
    p.end();
    if (!saved) {
        Serial.println("[UI] cannot save notice size");
        ui_toast("Cannot save notice size", T_BAD, 900);
        return;
    }
    s_big_text = on;
    s_big_text_loaded = true;
}

void ui_init(void)
{
    auto &d = M5Cardputer.Display;
    d.fillScreen(T_BG);
    d.setTextWrap(false, false);
    d.setTextSize(1);
    (void)ui_big_text();
    (void)ui_motion_enabled();
}

void ui_clear_body(void)
{
    M5Cardputer.Display.fillRect(0, BODY_Y, SCR_W, BODY_H, T_BG);
}

/* Force a real clear — use for screen transitions, menu entry/exit.
 * Also invalidates the ui_draw_status cache because some screen
 * transitions follow a full-screen fill that wipes the status bar too. */
void ui_force_clear_body(void)
{
    M5Cardputer.Display.fillRect(0, BODY_Y, SCR_W, BODY_H, T_BG);
    ui_status_invalidate();
}

void ui_text(int x, int y, uint16_t fg, const char *fmt, ...)
{
    char buf[64];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    ui_label(x, y, SCR_W - 4 - x, fg, T_BG, buf);
}

void ui_text_w(int x, int y, int w, uint16_t fg, const char *fmt, ...)
{
    char buf[64];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    ui_label(x, y, w, fg, T_BG, buf);
}

static void ui_footer_invalidate(void);

/* Heap churn and decorative pulses do not invalidate visible status. */
static char     s_st_radio[24] = {0};
static char     s_st_extra[24] = {0};
static int      s_st_c5_n = -1;
static int      s_st_bat_bucket = -1;
static bool     s_st_chg = false;
static bool     s_st_valid = false;
static theme_id_t s_st_theme = THEME__COUNT;

void ui_draw_status(const char *radio, const char *extra)
{
    auto &d = M5Cardputer.Display;

    const char *rr = radio ? radio : "idle";
    const char *ee = (extra && *extra) ? extra : "";
    int c5_n = c5_any_online() ? c5_peer_count() : 0;
    char radio_text[24], extra_text[24];
    ui_layout::fit(rr, radio_text, sizeof(radio_text), 14);
    ui_layout::fit(ee, extra_text, sizeof(extra_text), c5_n > 0 ? 6 : 14);

    int32_t bat_level = M5Cardputer.Power.getBatteryLevel();
    if (bat_level < 0)   bat_level = 0;
    if (bat_level > 100) bat_level = 100;
    int bat_bucket = (int)bat_level;
    bool chg = M5Cardputer.Power.isCharging();

    bool changed = !s_st_valid
                || strcmp(radio_text, s_st_radio) != 0
                || strcmp(extra_text, s_st_extra) != 0
                || c5_n != s_st_c5_n
                || bat_bucket != s_st_bat_bucket
                || chg != s_st_chg
                || s_st_theme != theme_current_id();

    if (!changed) return;

    d.setTextSize(1);
    d.fillRect(0, 0, SCR_W, STATUS_H, theme().status_bg);
    d.drawFastVLine(7, 2, 7, T_ACCENT);
    d.drawFastVLine(4, 2, 3, T_ACCENT);
    d.drawFastVLine(10, 2, 3, T_ACCENT);
    d.drawFastHLine(4, 5, 7, T_ACCENT);
    ui_label(18, 2, 84, T_FG, theme().status_bg, radio_text);
    ui_label(108, 2, c5_n > 0 ? 36 : 84, T_DIM, theme().status_bg, extra_text);

    /* Battery Icon on far right (14px wide) */
    const int bat_x = SCR_W - 16;
    const int bat_y = 2;
    d.drawRect(bat_x, bat_y, 13, 7, T_DIM);
    d.drawFastVLine(bat_x + 13, bat_y + 2, 3, T_DIM);

    int fill_w = (bat_level * 11) / 100;
    uint16_t bat_col = chg ? T_ACCENT : (bat_level > 50 ? T_GOOD : (bat_level > 20 ? T_WARN : T_BAD));
    if (fill_w > 0) {
        d.fillRect(bat_x + 1, bat_y + 1, fill_w, 5, bat_col);
    }
    if (chg) {
        d.drawPixel(bat_x + 6, bat_y + 3, T_FG);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", (long)bat_level);
    ui_label(196, 2, 24, bat_col, theme().status_bg, buf);

    /* Satellite and battery slots never overlap radio or extra text. */
    if (c5_n > 0) {
        char badge[16];
        snprintf(badge, sizeof(badge), "C5:%d", c5_n);
        ui_label(150, 2, 42, T_GOOD, theme().status_bg, badge);
    }

    d.drawFastHLine(0, STATUS_H - 1, SCR_W, theme().rule);

    strcpy(s_st_radio, radio_text);
    strcpy(s_st_extra, extra_text);
    s_st_c5_n        = c5_n;
    s_st_bat_bucket  = bat_bucket;
    s_st_chg         = chg;
    s_st_valid = true;
    s_st_theme = theme_current_id();
}

/* Invalidate the ui_draw_status cache — call this when navigating to a
 * new screen or whenever the status bar was overwritten by a full-body
 * redraw, so the next ui_draw_status() forces a fresh paint. */
void ui_status_invalidate(void) { s_st_valid = false; ui_footer_invalidate(); }

static char s_footer_hints[256] = "";
static int s_footer_page = 0;
static bool s_footer_valid = false;
static theme_id_t s_footer_theme = THEME__COUNT;
static void ui_footer_invalidate(void) { s_footer_valid = false; }

const char *ui_footer_hints(void) { return s_footer_hints; }

static void draw_footer_page(void)
{
    auto &d = M5Cardputer.Display;
    d.fillRect(0, FOOTER_Y, SCR_W, FOOTER_H, theme().footer_bg);
    d.drawFastHLine(0, FOOTER_Y, SCR_W, theme().rule);
    const bool paged = strlen(s_footer_hints) > 38;
    const char *p = s_footer_hints;
    char line[64] = "";
    for (int i = 0; i <= s_footer_page; ++i)
        ui_layout::wrap_next(p, line, sizeof(line), paged ? 25 : 38);
    ui_label(4, FOOTER_Y + 2, paged ? 150 : SCR_W - 8, T_DIM, theme().footer_bg, line);
    if (paged) {
        char more[24];
        snprintf(more, sizeof(more), "%d ^/ more", s_footer_page + 1);
        ui_label(166, FOOTER_Y + 2, 72, T_ACCENT, theme().footer_bg, more);
    }
    s_footer_valid = true;
    s_footer_theme = theme_current_id();
}

void ui_draw_footer(const char *hints)
{
    if (!hints) hints = "";
    const bool changed = strcmp(hints, s_footer_hints) != 0;
    if (changed) {
        if (strlen(hints) >= sizeof(s_footer_hints))
            Serial.println("[UI] footer exceeds 255-byte hint budget");
        ui_layout::fit(hints, s_footer_hints, sizeof(s_footer_hints), sizeof(s_footer_hints) - 1);
        s_footer_page = 0;
    }
    if (changed || !s_footer_valid || s_footer_theme != theme_current_id())
        draw_footer_page();
}

void ui_footer_next(void)
{
    if (strlen(s_footer_hints) <= 38) return;
    const int pages = static_cast<int>(ui_layout::line_count(s_footer_hints, 25));
    s_footer_page = (s_footer_page + 1) % pages;
    draw_footer_page();
}

void ui_toast(const char *msg, uint16_t color, uint32_t ms)
{
    auto &d = M5Cardputer.Display;
    uint8_t scale = ui_big_text() ? 2 : 1;
    /* Glyph base is 6x8 in the default font. At scale 2 it's 12x16.
     * Don't use d.textWidth() here because the size setter after the
     * width call would make the stored width stale. */
    int gw = 6 * scale, gh = 8 * scale;
    const int cells = (SCR_W - 24) / gw;
    const char *p = msg;
    char lines[6][40] = {};
    int count = 0;
    int tw = 0;
    while (count < 6 && ui_layout::wrap_next(p, lines[count], sizeof(lines[count]), cells)) {
        tw = max(tw, static_cast<int>(strlen(lines[count])) * gw);
        ++count;
    }
    if (p && *p) ui_layout::fit("...", lines[5], sizeof(lines[5]), cells);
    if (count == 0) count = 1;
    /* Clamp width so a long toast at 2x still fits the 240-wide screen. */
    int w = tw + 16;
    if (w > SCR_W - 4) w = SCR_W - 4;
    int h = count * (gh + 2) + 12;
    int x = (SCR_W - w) / 2;
    int y = (SCR_H - h) / 2;
    d.fillRoundRect(x, y, w, h, 3, T_BG);
    d.drawRoundRect(x, y, w, h, 3, color);
    for (int i = 0; i < count; ++i)
        ui_label(x + 8, y + 6 + i * (gh + 2), w - 16, color, T_BG, lines[i], scale);
    const uint32_t start = millis();
    while (millis() - start < ms) {
        if (input_poll() != PK_NONE) break;
        delay(10);
    }
    ui_status_invalidate();
    s_footer_valid = false;
}

/* ---- splash ---- */

/* The automatic, skippable Deepwater reveal lives in splash.cpp. */

void ui_body_println(int row, uint16_t color, const char *fmt, ...)
{
    auto &d = M5Cardputer.Display;
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int y = BODY_Y + 2 + row * 11;
    d.fillRect(0, y, SCR_W, 11, T_BG);
    ui_label(4, y, SCR_W - 8, color, T_BG, buf);
}

/* ==================== animations ==================== */

/* Compatibility entry point: navigation redraws without framebuffer loans. */
void ui_slide_transition(ui_draw_fn build_new, int direction)
{
    if (!build_new) return;
    (void)direction;
    // Navigation never borrows radio heap or waits for a decorative slide.
    ui_force_clear_body();
    build_new();
}

/* Spinner: rotating trident silhouette. Drawn as a small 3-tine shape
 * rotated in 45° steps. */
void ui_spinner(int cx, int cy, uint16_t color)
{
    auto &d = M5Cardputer.Display;
    uint32_t t = ui_motion_enabled() ? millis() / 100 : 0;
    int phase = (int)(t & 7);

    /* 8 phases — draw lines with one selected tine highlighted. */
    int r = 8;
    for (int i = 0; i < 8; ++i) {
        float a = (i * 3.14159f / 4.0f) + (t * 0.05f);
        int x = cx + (int)(cosf(a) * r);
        int y = cy + (int)(sinf(a) * r);
        uint16_t c = (i == phase) ? T_FG : color;
        /* dim tail trail */
        if (i == ((phase - 1 + 8) & 7)) c = color;
        d.fillCircle(x, y, i == phase ? 2 : 1, c);
    }
    /* Central hub */
    d.fillCircle(cx, cy, 2, color);
}

/* Retained API for callers; notices no longer slide across unrelated chrome. */
void ui_notify_slide(const char *title, const char *sub,
                     uint16_t color, uint32_t hold_ms)
{
    char message[160];
    snprintf(message, sizeof(message), "%.70s\n%.80s", title ? title : "", sub ? sub : "");
    ui_toast(message, color, hold_ms);
}

/* Ripple: expanding ring, 6 frames at 20ms = 120ms total. */
void ui_ripple(int cx, int cy, uint16_t color)
{
    if (!ui_motion_enabled()) return;
    auto &d = M5Cardputer.Display;
    for (int r = 3; r < 24; r += 3) {
        d.drawCircle(cx, cy, r, color);
        delay(20);
        d.drawCircle(cx, cy, r, T_BG);
    }
}

static uint16_t blend565(uint16_t a, uint16_t b, uint8_t t);

void ui_waves(int cx, int cy, int max_radius, uint16_t base_color)
{
    auto &d = M5Cardputer.Display;
    if (!ui_motion_enabled()) {
        d.drawCircle(cx, cy, max_radius, theme().rule);
        d.fillCircle(cx, cy, 3, base_color);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const uint32_t phase = (millis() + i * 600) % 1800;
        const int radius = max_radius * ui_layout::ease_out(phase, 1800) / 256;
        if (radius > 0)
            d.drawCircle(cx, cy, radius, blend565(T_BG, base_color, 255 - phase * 220 / 1800));
    }
    d.fillCircle(cx, cy, 2, base_color);
}

/* Color blend used by radar + others. */
static uint16_t blend565(uint16_t a, uint16_t b, uint8_t t)
{
    uint8_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint8_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint8_t r = (ar * (255 - t) + br * t) / 255;
    uint8_t g = (ag * (255 - t) + bg * t) / 255;
    uint8_t bl = (ab * (255 - t) + bb * t) / 255;
    return (r << 11) | (g << 5) | bl;
}

/* ---- radar sweep ----
 * Rotating line with phosphor fade, outer grid, and occasional contact
 * blips that fade over time. */
struct radar_blip_t { float a; int r; uint32_t when; };
#define RADAR_BLIPS 6
static radar_blip_t s_blips[RADAR_BLIPS] = {0};

void ui_radar(int cx, int cy, int radius, uint16_t color)
{
    auto &d = M5Cardputer.Display;
    if (!ui_motion_enabled()) {
        d.drawCircle(cx, cy, radius, theme().rule);
        d.fillCircle(cx, cy, 1, color);
        return;
    }
    static float s_angle = 0;
    s_angle += 0.12f;
    if (s_angle > 6.28318f) s_angle -= 6.28318f;

    /* Outer ring + cross. */
    d.drawCircle(cx, cy, radius, theme().rule);
    d.drawCircle(cx, cy, radius * 2 / 3, theme().rule);
    d.drawCircle(cx, cy, radius / 3, theme().rule);
    d.drawFastHLine(cx - radius, cy, radius * 2, theme().rule);
    d.drawFastVLine(cx, cy - radius, radius * 2, theme().rule);

    /* Fading afterglow sweep — 30 degrees trailing. */
    for (int i = 0; i < 15; ++i) {
        float a = s_angle - i * 0.04f;
        int brightness = 255 - i * 17;
        uint16_t c = blend565(T_BG, color, (uint8_t)brightness);
        for (int r = 0; r < radius; r += 2) {
            int x = cx + (int)(cosf(a) * r);
            int y = cy + (int)(sinf(a) * r);
            d.drawPixel(x, y, c);
        }
    }

    /* Leading edge — bright white. */
    for (int r = 0; r < radius; ++r) {
        int x = cx + (int)(cosf(s_angle) * r);
        int y = cy + (int)(sinf(s_angle) * r);
        d.drawPixel(x, y, T_FG);
    }

    /* Random blips. Guard against small radius: callers pass r=8 in
     * corner-radar slots, which would make (radius - 8) == 0 and
     * esp_random() % 0 is a hardware divide-by-zero panic. */
    int blip_range = radius - 8;
    if (blip_range < 1) blip_range = 1;
    if ((esp_random() & 0xFF) < 12) {
        int slot = esp_random() % RADAR_BLIPS;
        s_blips[slot].a = s_angle;
        s_blips[slot].r = 6 + (esp_random() % blip_range);
        s_blips[slot].when = millis();
    }
    for (int i = 0; i < RADAR_BLIPS; ++i) {
        uint32_t age = millis() - s_blips[i].when;
        if (age > 3000 || s_blips[i].when == 0) continue;
        uint8_t alpha = 255 - (age * 85 / 1000);
        int bx = cx + (int)(cosf(s_blips[i].a) * s_blips[i].r);
        int by = cy + (int)(sinf(s_blips[i].a) * s_blips[i].r);
        uint16_t bc = blend565(T_BG, T_ACCENT, alpha);
        d.fillCircle(bx, by, 2, bc);
    }
}

/* ---- shared scan / connect screens ---- */
void ui_scanning_indicator(const char *label, int found)
{
    auto &d = M5Cardputer.Display;
    /* Corner radar animates every call for smoothness. */
    d.fillRect(SCR_W - 18, BODY_Y + 1, 16, 14, T_BG);
    ui_radar(SCR_W - 11, BODY_Y + 8, 6, T_ACCENT);

    /* Throttle the text/dot animation so it does not flicker. */
    static uint32_t s_ms = 0;
    static int s_dots = 0;
    if (millis() - s_ms < 250) return;
    s_ms = millis();
    s_dots = ui_motion_enabled() ? (s_dots + 1) % 4 : 0;
    const char *tail = s_dots == 0 ? "   " : s_dots == 1 ? ".  "
                     : s_dots == 2 ? ".. " : "...";
    char buf[40];
    if (found >= 0) snprintf(buf, sizeof(buf), "%s%s %d", label, tail, found);
    else            snprintf(buf, sizeof(buf), "%s%s", label, tail);

    /* Draw in a fixed top-right strip, left of the corner radar. */
    int x = SCR_W - 128;
    ui_label(x, BODY_Y + 2, 108, T_ACCENT, T_BG, buf);
}

void ui_connecting_screen(const char *target)
{
    ui_clear_body();
    ui_header("CONNECTING");
    ui_label(6, BODY_Y + 30, 228, T_FG, T_BG, target);
    ui_spinner(SCR_W / 2, BODY_Y + 54, T_ACCENT);
    ui_label(6, BODY_Y + 74, 228, T_DIM, T_BG, "Establishing link, please wait");
}

/* ---- hex data stream ---- */
void ui_hexstream(int x, int y, int w, int h, uint16_t color)
{
    if (!ui_motion_enabled()) return;
    auto &d = M5Cardputer.Display;
    static uint32_t s_phase = 0;
    s_phase += 2;

    /* Three rows, each scrolls at a different speed. */
    int rows = h / 10;
    if (rows > 5) rows = 5;
    for (int r = 0; r < rows; ++r) {
        int row_y = y + r * 10;
        int speed = 1 + (r & 1);
        uint32_t off = (s_phase * speed) / 3;
        /* Clear this row. */
        d.fillRect(x, row_y, w, 10, 0x0000);
        /* Draw hex pairs scrolling from right. */
        for (int col = 0; col < w / 18 + 2; ++col) {
            uint32_t seed = (r * 7919) ^ (col + off);
            seed = seed * 2654435761u;
            uint8_t hi = (seed >> 8) & 0xFF;
            int xp = x + w - (int)((col * 18 + (s_phase % 18) * speed) % (w + 18));
            if (xp < x - 12 || xp > x + w) continue;
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", hi);
            /* Occasional "fresh" byte highlights bright. */
            uint16_t col_col = (col == 0) ? 0xFFFF : color;
            d.setTextColor(col_col, 0x0000);
            d.setCursor(xp, row_y + 1);
            d.print(buf);
        }
    }
}

/* ---- glitch blocks ---- */
void ui_glitch(int x, int y, int w, int h)
{
    if (!ui_motion_enabled() || w < 2 || h <= 0) return;
    auto &d = M5Cardputer.Display;
    static uint32_t s_last = 0;
    /* Occasional bursts only. */
    if ((esp_random() & 0xFF) > 40) {
        /* No glitch this frame — clear just in case of residue. */
        if (millis() - s_last > 100) return;
    }
    s_last = millis();
    const uint16_t glitch_cols[] = {
        T_ACCENT2, T_ACCENT, T_DIM, theme().rule
    };
    for (int i = 0; i < 3; ++i) {
        int sy = y + (esp_random() % h);
        int sh = 1 + (esp_random() % 4);
        int sx = x + (esp_random() % (w / 2));
        int sw = (esp_random() % (w - (sx - x)));
        uint16_t c = glitch_cols[esp_random() % (sizeof(glitch_cols)/sizeof(*glitch_cols))];
        d.fillRect(sx, sy, sw, sh, c);
    }
}

/* ---- EQ bars ---- */
void ui_eq_bars(int x, int y, int bar_w, int bar_h_max, uint16_t color)
{
    auto &d = M5Cardputer.Display;
    /* 5 bars, each with a smoothed random target. */
    static uint8_t level[5] = { 4, 7, 3, 8, 5 };
    static uint8_t target[5] = { 8, 3, 9, 4, 7 };
    static uint32_t s_last = 0;
    if (ui_motion_enabled() && millis() - s_last > 60) {
        s_last = millis();
        for (int i = 0; i < 5; ++i) {
            if (level[i] < target[i]) level[i]++;
            else if (level[i] > target[i]) level[i]--;
            if (level[i] == target[i]) target[i] = esp_random() % 10;
        }
    }
    for (int i = 0; i < 5; ++i) {
        int bh = level[i] * bar_h_max / 9;
        int bx = x + i * (bar_w + 2);
        /* Bar trail (faded max mark). */
        d.drawFastHLine(bx, y + bar_h_max - bh - 1, bar_w, theme().rule);
        /* Clear below. */
        d.fillRect(bx, y + bar_h_max - bh, bar_w, bh, color);
        /* Empty above. */
        d.fillRect(bx, y, bar_w, bar_h_max - bh, T_BG);
    }
}

/* ---- magenta dashboard chrome ---- */

static uint32_t s_dash_flash_start = 0;
static uint32_t s_dash_last_flash  = 0;

void ui_dashboard_chrome(const char *title, bool flash_now)
{
    auto &d = M5Cardputer.Display;
    uint32_t now = millis();

    /* Rate-limit flashes: ignore new triggers within 900ms of the
     * previous one so the border doesn't strobe every frame. */
    if (flash_now && (now - s_dash_last_flash) > 900) {
        s_dash_flash_start = now;
        s_dash_last_flash  = now;
    }

    /* Hex storm backdrop disabled — it repainted hex digits across the
     * whole body every frame, which made the dashboard visibly flash
     * every time a feature redrew its status text. Static chrome is
     * easier on the eyes and faster. Re-enable via ui_hexstream() in a
     * specific feature if you want it back in a smaller strip. */

    /* Smooth fade: peak at t=0, fades to nothing over 500 ms. */
    uint32_t dt = now - s_dash_flash_start;
    ui_header(title);
    if (ui_motion_enabled() && s_dash_flash_start && dt < 500) {
        uint16_t c = blend565(theme().rule, T_ACCENT, 255 - dt * 255 / 500);
        d.drawFastHLine(6, BODY_Y + 14, SCR_W - 12, c);
    }

    /* Radar is drawn separately by ui_dashboard_radar(), which each screen
     * calls AFTER painting its body. A doubled (r12) radar is taller than the
     * 14px strip between the top banner and the body-content clear, so drawing
     * it here (before the body fillRect) would clip it. */
}

/* Upper-right dashboard radar, doubled (r12) and nudged down+left so it clears
 * the top status banner (y<BODY_Y) and stays on-screen. MUST be called at the
 * end of a redraw — after the screen's body fillRect and content — or the body
 * clear slices it. Self-clearing via the pre-clear box. */
void ui_dashboard_radar(void)
{
    auto &d = M5Cardputer.Display;
    d.fillRect(SCR_W - 36, BODY_Y + 2, 34, 30, T_BG);
    ui_radar(SCR_W - 20, BODY_Y + 16, 12, T_ACCENT);
}

void ui_freq_bars(int x, int y, int bar_w, int bar_h_max)
{
    ui_eq_bars(x, y, bar_w, bar_h_max, T_ACCENT);
}

/* ---- full-screen action overlay ----
 * Two entry points share one implementation. The plain ui_action_overlay
 * forwards to the with-tick variant with a null callback so neither has
 * to duplicate the (sizeable) animation loop. POS-AUDIT-009 introduced
 * the tick path to keep captive-portal DNS + HTTP responsive while a
 * CRED-CAPTURED overlay is on screen. */
void ui_action_overlay(const char *headline, const char *subtitle,
                       action_anim_t bg, uint16_t color, uint32_t duration_ms)
{
    ui_action_overlay_with_tick(headline, subtitle, bg, color, duration_ms,
                                 nullptr, nullptr);
}

void ui_action_overlay_with_tick(const char *headline, const char *subtitle,
                                  action_anim_t bg, uint16_t color,
                                  uint32_t duration_ms,
                                  void (*tick_cb)(void *), void *cb_ctx)
{
    auto &d = M5Cardputer.Display;
    (void)bg;
    ui_force_clear_body();
    ui_header("EVENT");
    d.fillRoundRect(6, BODY_Y + 23, 228, 76, 5, T_SEL_BG);
    d.fillRect(6, BODY_Y + 29, 2, 64, color);
    const char *p = headline;
    char line[40];
    int row = 0;
    while (row < 2 && ui_layout::wrap_next(p, line, sizeof(line), 17)) {
        ui_label(16, BODY_Y + 32 + row * 19, 204, color, T_SEL_BG, line, 2);
        ++row;
    }
    if (p && *p) ui_label(16, BODY_Y + 51, 204, color, T_SEL_BG, "...", 2);
    ui_label(16, BODY_Y + 78, 204, T_FG, T_SEL_BG, subtitle);
    ui_draw_footer("Any key dismisses");
    const uint32_t start = millis();
    int last_progress = -1;
    while (millis() - start < duration_ms) {
        if (ui_motion_enabled()) {
            const int progress = static_cast<int>(static_cast<uint64_t>(millis() - start) * 216 / duration_ms);
            if (progress != last_progress) {
                last_progress = progress;
                d.drawFastHLine(12, BODY_Y + 105, progress, color);
            }
        }
        if (tick_cb) tick_cb(cb_ctx);
        if (input_poll() != PK_NONE) break;
        delay(10);
    }
    ui_status_invalidate();
}

/* ---- matrix rain ----
 * Column state: each column has a "head" y-position and speed.
 * Each render tick:
 *   - draws a fresh bright glyph at head
 *   - draws a fading trail above
 *   - advances head down; resets when off-screen
 * Glyph pool: printable katakana-ish via random printable chars.
 */
#define MATRIX_COLS 42
static int8_t  mx_head[MATRIX_COLS];      /* -1 = inactive */
static uint8_t mx_speed[MATRIX_COLS];
static char    mx_glyph[MATRIX_COLS];
static uint32_t mx_last_tick = 0;
static bool     mx_initialized = false;

void ui_matrix_rain(int x, int y, int w, int h, uint16_t color)
{
    if (!ui_motion_enabled()) return;
    auto &d = M5Cardputer.Display;
    /* Font cell: 6×8 default. Column spacing ~6px, row spacing ~8. */
    int col_w = 6;
    int row_h = 8;
    int n_cols = w / col_w;
    if (n_cols > MATRIX_COLS) n_cols = MATRIX_COLS;
    int rows = h / row_h;

    if (!mx_initialized) {
        for (int c = 0; c < MATRIX_COLS; ++c) {
            mx_head[c] = -1;
            mx_speed[c] = 0;
            mx_glyph[c] = '?';
        }
        mx_initialized = true;
    }

    uint32_t now = millis();
    bool advance = (now - mx_last_tick > 80);
    if (advance) mx_last_tick = now;

    for (int c = 0; c < n_cols; ++c) {
        if (mx_head[c] < 0) {
            /* Chance to spawn a new rain drop. */
            if ((esp_random() & 0xFF) < 10) {
                mx_head[c] = 0;
                mx_speed[c] = 1 + (esp_random() & 1);
            }
            continue;
        }
        /* Draw fading trail. */
        for (int t = 0; t < 5; ++t) {
            int ty = mx_head[c] - t;
            if (ty < 0 || ty >= rows) continue;
            uint16_t tcol = (t == 0) ? 0xFFFF : color;
            if (t == 1) tcol = color;
            else if (t == 2) tcol = 0x0440;  /* dim */
            else if (t >= 3) tcol = 0x0220;
            d.setTextColor(tcol, T_BG);
            d.setCursor(x + c * col_w, y + ty * row_h);
            char g = 0x21 + (char)(esp_random() % 0x5D);
            d.printf("%c", g);
        }
        if (advance) {
            /* Erase the tail row below the 5-char trail. */
            int erase_y = mx_head[c] - 5;
            if (erase_y >= 0 && erase_y < rows) {
                d.fillRect(x + c * col_w, y + erase_y * row_h, col_w, row_h, T_BG);
            }
            mx_head[c] += mx_speed[c];
            if (mx_head[c] >= rows + 5) {
                mx_head[c] = -1;  /* done */
                /* Also wipe any leftover pixels in this column. */
                d.fillRect(x + c * col_w, y, col_w, h, T_BG);
            }
        }
    }
}
