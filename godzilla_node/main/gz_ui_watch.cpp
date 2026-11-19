/*
 * gz_ui_watch.cpp — old-fashioned pocket-watch face (P2c, Dale's spec).
 *
 * Time: the pure gz_watch.h software clock, synced from the PCF85063 when the
 * face is built, whenever it becomes visible, and about once a minute while
 * visible. No I2C per frame. A failed read keeps the hands running from the
 * software clock and shows "RTC?"; the hands never freeze or blank. If the RTC
 * never answers at all, the clock runs from the firmware build time.
 *
 * Drawing (all LVGL primitives, original artwork — no traced watch face, no
 * image or font asset):
 *   - a brass case ring around an ivory dial, a thin inner ring, and a crown
 *     with a bow at 12 o'clock, all inside the round glass (the case is centred
 *     8 px low to make room for the bow);
 *   - 60 minute ticks (every fifth heavier) and upright Roman numerals in
 *     Montserrat 20, with the traditional dial "IIII";
 *   - hour, minute and second hands as lv_line objects positioned by their
 *     bounding box (same technique as the radar sweep), so a move invalidates
 *     only the hand's own strip. The second hand TICKS at 1 Hz — it does not
 *     sweep — and the face redraws only when the second changes.
 *
 * Set mode (no wire change; I2C to a soldered part): long-press enters, a tap
 * on the upper half is hour +1, on the lower half minute +1, long-press again
 * writes the RTC, 15 s idle leaves without writing. "SET" shows on the dial
 * and the hand last changed blinks. Until the time has been set by hand after
 * a build-time seed, the dial shows "hold to set".
 *
 * No date on the dial: set mode sets hours and minutes only, so a date could
 * be shown wrong (CI images are built in UTC) with no way to fix it.
 *
 * Palette: abyss 0x031627 (surround), brass 0xC9A227 / dark brass 0x8A6D1B
 * (case, crown, cap), ivory 0xF3E9D2 (dial), ink 0x1B2A3A (numerals, ticks,
 * hour + minute hands), oxblood 0xA63A2B (second hand, SET).
 */
#include "gz_board.h"
#if GZ_BOARD_HAS_RTC   /* the watch runs only where an RTC exists (#88 §4) */
#include "gz_ui_watch.h"
#include "gz_watch.h"
#include "gz_stats.h"
#include "gz_clock.h"
#include "gz_board.h"
#include "gz_geom.h"   /* the one geometry source (#88 §3.2) */

#include <Arduino.h>

#define COL_ABYSS    0x031627
#define COL_BRASS    0xC9A227
#define COL_BRASS_D  0x8A6D1B
#define COL_IVORY    0xF3E9D2
#define COL_INK      0x1B2A3A
#define COL_OXBLOOD  0xA63A2B

/* Designed on the 1.85B's 360 canvas; every radial length scales with the
 * panel's safe radius via gz_geom.h (#88 §3.2), identity on the 1.85B. */
#define WCX          GZ_UI_CX                      /* 180 on the 1.85B */
#define WCY          (GZ_UI_CY + GZ_UI_S(8))       /* 188: room for the bow above */
#define R_CASE       GZ_UI_S(150)                  /* outer edge of the brass ring */
#define CASE_W       GZ_UI_S(7)
#define R_INNER      GZ_UI_S(139)                  /* thin inner ring */
#define R_TICK_OUT   GZ_UI_S(135)
#define R_TICK_IN    GZ_UI_S(129)                  /* minute ticks */
#define R_TICK_IN5   GZ_UI_S(123)                  /* five-minute ticks */
#define R_NUMERAL    GZ_UI_S(103)
#define LEN_HOUR     GZ_UI_S(62)
#define LEN_MINUTE   GZ_UI_S(96)
#define LEN_SECOND   GZ_UI_S(112)
#define TAIL_HM      GZ_UI_S(14)
#define TAIL_SECOND  GZ_UI_S(24)
#define BOW_Y        (WCY - R_CASE - 21)           /* 17 on the 1.85B */
#define BLINK_MS     500u

static gz_watch_clock_t s_clock;
static gz_watch_set_t   s_set;
static bool     s_rtc_fault;        /* last RTC read or write failed            */
static bool     s_needs_set;        /* seeded from build time, not yet set      */
static uint8_t  s_drawn_second = 0xFF;
static bool     s_force_draw   = true;
static bool     s_blink_hidden;

static lv_obj_t *s_hand_h, *s_hand_m, *s_hand_s, *s_note;
static lv_point_precise_t s_pts_h[2], s_pts_m[2], s_pts_s[2];
static lv_point_precise_t s_tick_pts[60][2];

/* ---- helpers ------------------------------------------------------------- */

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* A circle centred on the dial: optional fill, optional border. */
static lv_obj_t *disc(lv_obj_t *scr, int cx, int cy, int r, int border_w,
                      uint32_t border_rgb, bool filled, uint32_t fill_rgb)
{
    lv_obj_t *o = plain_obj(scr);
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_pos(o, cx - r, cy - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    if (filled) {
        lv_obj_set_style_bg_color(o, lv_color_hex(fill_rgb), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    }
    if (border_w > 0) {
        lv_obj_set_style_border_width(o, border_w, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(border_rgb), 0);
        lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    }
    return o;
}

/* Point a two-point line at screen coords (x0,y0)-(x1,y1), positioned by its
 * bounding box so only that box is invalidated when it moves. */
static void place_line(lv_obj_t *l, lv_point_precise_t pts[2],
                       int x0, int y0, int x1, int y1)
{
    int bx = x0 < x1 ? x0 : x1, by = y0 < y1 ? y0 : y1;
    pts[0].x = x0 - bx; pts[0].y = y0 - by;
    pts[1].x = x1 - bx; pts[1].y = y1 - by;
    lv_line_set_points(l, pts, 2);
    lv_obj_set_pos(l, bx, by);
}

static lv_obj_t *new_line(lv_obj_t *scr, uint32_t rgb, int width)
{
    lv_obj_t *l = lv_line_create(scr);
    lv_obj_set_style_line_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_line_width(l, width, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static void place_hand(lv_obj_t *l, lv_point_precise_t pts[2], int32_t angle,
                       int len, int tail)
{
    int x0, y0, x1, y1;
    gz_watch_hand(angle, len, tail, WCX, WCY, &x0, &y0, &x1, &y1);
    place_line(l, pts, x0, y0, x1, y1);
}

static lv_obj_t *dial_label(lv_obj_t *scr, const lv_font_t *font, uint32_t rgb,
                            int x, int y, const char *text)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, text);
    /* Centre the label on (x, y): the screen is the 360x360 parent. */
    lv_obj_align(l, LV_ALIGN_CENTER, x - GZ_LCD_WIDTH / 2, y - GZ_LCD_HEIGHT / 2);
    return l;
}

/* Every change to the software clock goes through here, so other tasks (the
 * Recon writer) see the same time via gz_clock without touching I2C. */
static void set_clock(const gz_datetime_t *t, uint32_t now_ms)
{
    gz_watch_sync(&s_clock, t, now_ms);
    gz_clock_publish(&s_clock);
}

/* Try the RTC; on success the software clock restarts from it. */
static void resync(uint32_t now_ms)
{
    gz_datetime_t t;
    if (gz_rtc_read(&t) && gz_rtc_valid(&t)) {
        set_clock(&t, now_ms);
        s_rtc_fault = false;
    } else {
        s_rtc_fault = true;
        if (!s_clock.synced) {
            /* Never had a time: run from the build stamp so the hands move. */
            gz_datetime_t build;
            if (gz_rtc_parse_build(__DATE__, __TIME__, &build))
                set_clock(&build, now_ms);
        } else {
            /* Keep running, but don't hammer a dead bus every refresh. */
            gz_datetime_t cur;
            gz_watch_now(&s_clock, now_ms, &cur);
            set_clock(&cur, now_ms);
        }
    }
    s_force_draw = true;
}

static void update_note(void)
{
    const char *text = "";
    uint32_t rgb = COL_INK;
    if (s_set.active)      { text = "SET"; rgb = COL_OXBLOOD; }
    else if (s_rtc_fault)  { text = "RTC?"; rgb = COL_OXBLOOD; }
    else if (s_needs_set)  { text = "hold to set"; }
    lv_obj_set_style_text_color(s_note, lv_color_hex(rgb), 0);
    lv_label_set_text(s_note, text);
}

static void draw_hands(const gz_datetime_t *t)
{
    place_hand(s_hand_h, s_pts_h, gz_watch_hour_angle(t->hour, t->minute),
               LEN_HOUR, TAIL_HM);
    place_hand(s_hand_m, s_pts_m, gz_watch_minute_angle(t->minute, t->second),
               LEN_MINUTE, TAIL_HM);
    place_hand(s_hand_s, s_pts_s, gz_watch_second_angle(t->second),
               LEN_SECOND, TAIL_SECOND);
    s_drawn_second = t->second;
}

static void set_blink(bool hidden)
{
    lv_obj_t *blinker = (s_set.field == GZ_WATCH_FIELD_HOUR) ? s_hand_h : s_hand_m;
    lv_obj_t *steady  = (s_set.field == GZ_WATCH_FIELD_HOUR) ? s_hand_m : s_hand_h;
    if (hidden) lv_obj_add_flag(blinker, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_remove_flag(blinker, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(steady, LV_OBJ_FLAG_HIDDEN);
    s_blink_hidden = hidden;
}

static void leave_set_mode(void)
{
    gz_watch_set_cancel(&s_set);
    set_blink(false);
    s_force_draw = true;
    update_note();
}

/* ---- build --------------------------------------------------------------- */

void gz_ui_watch_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Bow (ring) and crown (stem) first, so the case overlaps their roots. */
    disc(scr, WCX, BOW_Y, 13, 4, COL_BRASS, false, 0);
    lv_obj_t *crown = plain_obj(scr);
    lv_obj_set_size(crown, 22, 16);
    lv_obj_set_pos(crown, WCX - 11, WCY - R_CASE - 12);
    lv_obj_set_style_radius(crown, 3, 0);
    lv_obj_set_style_bg_color(crown, lv_color_hex(COL_BRASS_D), 0);
    lv_obj_set_style_bg_opa(crown, LV_OPA_COVER, 0);

    /* Case + dial in one object, then the thin inner ring. */
    disc(scr, WCX, WCY, R_CASE, CASE_W, COL_BRASS, true, COL_IVORY);
    disc(scr, WCX, WCY, R_INNER, 1, COL_BRASS_D, false, 0);

    /* Minute track: 60 ticks, every fifth longer and heavier. */
    for (int i = 0; i < 60; ++i) {
        bool five = (i % 5) == 0;
        int x0, y0, x1, y1;
        gz_watch_polar(i * 60, five ? R_TICK_IN5 : R_TICK_IN, WCX, WCY, &x0, &y0);
        gz_watch_polar(i * 60, R_TICK_OUT, WCX, WCY, &x1, &y1);
        lv_obj_t *t = new_line(scr, COL_INK, five ? 3 : 1);
        place_line(t, s_tick_pts[i], x0, y0, x1, y1);
    }

    /* Upright Roman numerals. */
    for (int h = 1; h <= 12; ++h) {
        int x, y;
        gz_watch_polar(h * 300, R_NUMERAL, WCX, WCY, &x, &y);
        dial_label(scr, &lv_font_montserrat_20, COL_INK, x, y, gz_watch_roman((uint8_t)h));
    }

    dial_label(scr, &lv_font_montserrat_12, COL_BRASS_D, WCX, WCY - GZ_UI_S(44), "POSEIDON");
    s_note = dial_label(scr, &lv_font_montserrat_14, COL_INK, WCX, WCY + GZ_UI_S(46), "");

    /* Hands, bottom to top: hour, minute, second, then the centre cap. */
    s_hand_h = new_line(scr, COL_INK, 7);
    s_hand_m = new_line(scr, COL_INK, 4);
    s_hand_s = new_line(scr, COL_OXBLOOD, 2);
    disc(scr, WCX, WCY, 7, 2, COL_BRASS_D, true, COL_BRASS);

    gz_status_t st;
    gz_stats_snapshot(&st);
    s_needs_set = st.rtc_seeded;

    uint32_t now = millis();
    resync(now);
    update_note();
    gz_datetime_t t;
    gz_watch_now(&s_clock, now, &t);
    draw_hands(&t);
}

/* ---- run ----------------------------------------------------------------- */

void gz_ui_watch_show(uint32_t now_ms)
{
    resync(now_ms);
    update_note();
    gz_ui_watch_refresh(now_ms);
}

void gz_ui_watch_hide(void)
{
    if (s_set.active) leave_set_mode();
}

void gz_ui_watch_refresh(uint32_t now_ms)
{
    if (s_set.active) {
        if (gz_watch_set_timed_out(&s_set, now_ms)) {
            leave_set_mode();                       /* nothing written */
        } else {
            /* Hands show the time being set; the second hand rests at 12. */
            if (s_force_draw) {
                draw_hands(&s_set.edit);
                s_force_draw = false;
            }
            bool hide = ((uint32_t)(now_ms - s_set.last_input_ms) / BLINK_MS) & 1u;
            if (hide != s_blink_hidden) set_blink(hide);
            return;
        }
    }

    if (gz_watch_resync_due(&s_clock, now_ms)) {
        bool was_fault = s_rtc_fault;
        resync(now_ms);
        if (was_fault != s_rtc_fault) update_note();
    }
    gz_datetime_t t;
    gz_watch_now(&s_clock, now_ms, &t);
    if (s_force_draw || t.second != s_drawn_second) {
        draw_hands(&t);
        s_force_draw = false;
    }
}

void gz_ui_watch_tap(int32_t y, uint32_t now_ms)
{
    if (!s_set.active) return;
    gz_watch_set_tap(&s_set, (int)y, WCY, now_ms);
    set_blink(false);                               /* show the change at once */
    s_force_draw = true;
    gz_ui_watch_refresh(now_ms);
}

void gz_ui_watch_long_press(uint32_t now_ms)
{
    if (!s_set.active) {
        gz_datetime_t cur;
        gz_watch_now(&s_clock, now_ms, &cur);
        gz_watch_set_enter(&s_set, &cur, now_ms);
        s_set.edit.second = 0;                      /* second hand rests at 12 */
        s_force_draw = true;
        update_note();
        gz_ui_watch_refresh(now_ms);
        return;
    }

    gz_datetime_t t;
    gz_watch_set_commit(&s_set, &t);
    t.weekday = gz_rtc_weekday(t.year, t.month, t.day);
    bool ok = gz_rtc_write(&t);
    set_clock(&t, now_ms);                          /* run from the set time either way */
    s_rtc_fault = !ok;
    if (ok) s_needs_set = false;
    Serial.printf("  watch: set %02u:%02u:00 -> RTC %s\n",
                  (unsigned)t.hour, (unsigned)t.minute, ok ? "written" : "WRITE FAILED");
    set_blink(false);
    s_force_draw = true;
    update_note();
    gz_ui_watch_refresh(now_ms);
}

bool gz_ui_watch_time(uint32_t now_ms, gz_datetime_t *out)
{
    if (!s_clock.synced) return false;
    gz_watch_now(&s_clock, now_ms, out);
    return true;
}

#endif /* GZ_BOARD_HAS_RTC */
