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
#include "gz_moon.h"   /* pure moon-phase + printed sub-scale math (unit 2) */

#include <Arduino.h>
#include <stdio.h>

#define COL_ABYSS    0x031627
#define COL_BRASS    0xC9A227
#define COL_BRASS_D  0x8A6D1B
#define COL_IVORY    0xF3E9D2
#define COL_INK      0x1B2A3A
#define COL_OXBLOOD  0xA63A2B
#define COL_BLUE     0x27406E   /* blued steel: hour + minute hands, sub pointers */
#define COL_NIGHT    0x0B1830   /* moon-aperture night ground                     */
#define COL_SILVER   0xD9DEE6   /* the lit moon disc                              */

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

/* ---- antique sub-scales (unit 2) ----------------------------------------- */
/* Three printed calendar sub-dials and a twin-aperture moon, nested inside the
 * numeral ring. Each sub-dial has its own centre, a printed scale and a short
 * blued pointer placed by its ring angle (gz_moon.h). The moon window shows the
 * lit fraction for the date. All gate with the file (GZ_BOARD_HAS_RTC); a board
 * without an RTC compiles none of this and shows "set via Cardputer" elsewhere. */
#define SUB_R        GZ_UI_S(30)                    /* sub-dial scale radius     */
#define SUB_PTR      GZ_UI_S(24)                    /* sub-dial pointer length   */
#define SUB_OFF      GZ_UI_S(52)                    /* sub-dial centre offset    */
#define SUB_DOW_CX   WCX                            /* day-of-week: above centre */
#define SUB_DOW_CY   (WCY - SUB_OFF)
#define SUB_MON_CX   (WCX - SUB_OFF)               /* month: lower left          */
#define SUB_MON_CY   (WCY + GZ_UI_S(8))
#define SUB_DATE_CX  (WCX + SUB_OFF)               /* date: lower right          */
#define SUB_DATE_CY  (WCY + GZ_UI_S(8))
#define MOON_CX      WCX                            /* moon aperture: below centre */
#define MOON_CY      (WCY + SUB_OFF)
#define MOON_R       GZ_UI_S(15)
#define MOON_STAR_R  GZ_UI_S(1)                      /* gold star dot in the night sky */
#define SUB_TICK_R   SUB_R                           /* division ticks sit on the rim  */
#define SUB_LBL_R    (SUB_R - GZ_UI_S(9))            /* printed numeral / abbrev radius */

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

/* Sub-scale pointers (month / day-of-week / date) and the moon window. */
static lv_obj_t *s_ptr_mon, *s_ptr_dow, *s_ptr_date;
static lv_point_precise_t s_pts_mon[2], s_pts_dow[2], s_pts_date[2];
static lv_obj_t *s_moon_lit;                 /* the lit disc inside the aperture */
static uint8_t  s_drawn_day = 0;             /* last date whose scales were drawn */
static lv_obj_t *s_date_ring;                /* date ticks+numerals (rebuilt monthly) */
static uint8_t  s_drawn_month = 0;           /* last month whose date ring was drawn */

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

/* A short pointer on a sub-dial: from the sub-dial centre out along a ring
 * angle (tenths of a degree, 0 = up). Positioned by its bounding box like the
 * main hands, so moving it invalidates only its own strip. */
static void place_sub_ptr(lv_obj_t *l, lv_point_precise_t pts[2], int32_t angle,
                          int cx, int cy, int len)
{
    int x1, y1;
    gz_watch_polar(angle, len, cx, cy, &x1, &y1);
    place_line(l, pts, cx, cy, x1, y1);
}

/* Build one printed calendar sub-dial base: an ivory disc in a dark-brass ring
 * and a blued hub, centred at (cx, cy). The printed scale (ticks + numerals /
 * abbreviations) and the pointer are added by the caller. */
static void build_subdial(lv_obj_t *scr, int cx, int cy)
{
    disc(scr, cx, cy, SUB_R + GZ_UI_S(3), 1, COL_BRASS_D, true, COL_IVORY);
    disc(scr, cx, cy, 3, 1, COL_BRASS_D, true, COL_BLUE);
}

/* A small ink tick on a sub-dial rim, at ring angle `angle` (tenths, 0 = up). */
static void sub_tick(lv_obj_t *parent, int cx, int cy, int32_t angle, uint32_t rgb)
{
    int x, y;
    gz_watch_polar(angle, SUB_TICK_R, cx, cy, &x, &y);
    disc(parent, x, y, GZ_UI_S(2), 0, 0, true, rgb);
}

/* A printed sub-scale label (month / weekday abbrev, or a date numeral) inside
 * the rim at ring angle `angle`, in the small engraved font. */
static void sub_label(lv_obj_t *parent, int cx, int cy, int32_t angle, const char *txt)
{
    if (!txt || !txt[0]) return;
    int x, y;
    gz_watch_polar(angle, SUB_LBL_R, cx, cy, &x, &y);
    dial_label(parent, &lv_font_montserrat_10, COL_INK, x, y, txt);
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

/* The four printed sub-scales, from the pure math in gz_moon.h: month, day-of-
 * week and date pointers, and the moon window's lit disc (its radius encodes the
 * illuminated fraction, 0 at new .. full at full moon). The calendar changes at
 * most once a day, so this only redraws when the date rolls. */
static void draw_calendar(const gz_datetime_t *t)
{
    /* Date ring: its divisions are the month's real length, so (re)print the
     * ticks + numerals when the month rolls. The numerals and the date pointer
     * share gz_date_angle_tenths(day, dim), so they read the same scale. */
    if (t->month != s_drawn_month) {
        lv_obj_clean(s_date_ring);
        int dim = gz_rtc_days_in_month(t->year, t->month);
        for (int d = 1; d <= dim; ++d) {
            if (!gz_date_label_shown(d, dim)) continue;
            int32_t a = gz_date_angle_tenths(d, dim);
            sub_tick(s_date_ring, SUB_DATE_CX, SUB_DATE_CY, a, COL_INK);
            char num[3];
            snprintf(num, sizeof num, "%d", d);
            sub_label(s_date_ring, SUB_DATE_CX, SUB_DATE_CY, a, num);
        }
        s_drawn_month = t->month;
    }

    place_sub_ptr(s_ptr_mon,  s_pts_mon,  gz_month_angle_tenths(t->month),
                  SUB_MON_CX, SUB_MON_CY, SUB_PTR);
    place_sub_ptr(s_ptr_dow,  s_pts_dow,  gz_weekday_angle_tenths(t->weekday),
                  SUB_DOW_CX, SUB_DOW_CY, SUB_PTR);
    place_sub_ptr(s_ptr_date, s_pts_date,
                  gz_date_angle_tenths(t->day, gz_rtc_days_in_month(t->year, t->month)),
                  SUB_DATE_CX, SUB_DATE_CY, SUB_PTR);

    double illum = gz_moon_illum(t->year, t->month, t->day);
    int lr = (int)lroundf((float)MOON_R * (float)illum);
    if (lr < 1) {
        lv_obj_add_flag(s_moon_lit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_moon_lit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_moon_lit, 2 * lr, 2 * lr);
        lv_obj_set_pos(s_moon_lit, MOON_CX - lr, MOON_CY - lr);
    }
    s_drawn_day = t->day;
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

    /* Dotted minute track: 60 marks, the minute dots fine, the five-minute
     * marks heavier. Four quarter lozenges sit just inside the track. */
    for (int i = 0; i < 60; ++i) {
        bool five = (i % 5) == 0;
        int x0, y0, x1, y1;
        gz_watch_polar(i * 60, five ? R_TICK_IN5 : R_TICK_IN, WCX, WCY, &x0, &y0);
        gz_watch_polar(i * 60, R_TICK_OUT, WCX, WCY, &x1, &y1);
        lv_obj_t *t = new_line(scr, COL_INK, five ? 3 : 2);
        place_line(t, s_tick_pts[i], x0, y0, x1, y1);
    }
    for (int q = 0; q < 4; ++q) {
        int x, y;
        gz_watch_polar(q * 900, R_TICK_IN5 - GZ_UI_S(7), WCX, WCY, &x, &y);
        disc(scr, x, y, GZ_UI_S(4), 1, COL_BRASS_D, true, COL_BRASS);  /* quarter lozenge */
    }

    /* Breguet-style Arabic numerals, upright. */
    for (int h = 1; h <= 12; ++h) {
        int x, y;
        char num[3];
        snprintf(num, sizeof num, "%d", h);
        gz_watch_polar(h * 300, R_NUMERAL, WCX, WCY, &x, &y);
        dial_label(scr, &lv_font_montserrat_20, COL_INK, x, y, num);
    }

    /* Three printed calendar sub-dials (month / day-of-week / date) and the
     * twin-aperture moon window, all read from the RTC via gz_moon.h. */
    build_subdial(scr, SUB_DOW_CX,  SUB_DOW_CY);
    build_subdial(scr, SUB_MON_CX,  SUB_MON_CY);
    build_subdial(scr, SUB_DATE_CX, SUB_DATE_CY);

    /* Day-of-week ring: SUN..SAT printed at all seven divisions. */
    for (int i = 0; i < 7; ++i) {
        int32_t a = gz_weekday_angle_tenths(i);
        sub_tick(scr, SUB_DOW_CX, SUB_DOW_CY, a, COL_INK);
        sub_label(scr, SUB_DOW_CX, SUB_DOW_CY, a, gz_weekday_abbrev(i));
    }
    /* Month ring: twelve ticks; abbreviations are printed at the quarter months
     * (JAN/APR/JUL/OCT) so the names stay legible on the small sub-dial — the
     * pointer still reads the exact month against all twelve ticks. (Visual
     * density of a fuller month ring is a re-render call for Claudius.) */
    for (int i = 0; i < 12; ++i) {
        int32_t a = gz_month_angle_tenths(i + 1);
        sub_tick(scr, SUB_MON_CX, SUB_MON_CY, a, COL_INK);
        if ((i % 3) == 0)
            sub_label(scr, SUB_MON_CX, SUB_MON_CY, a, gz_month_abbrev(i + 1));
    }
    /* Date ring: ticks + numerals at 5/10/15/20/25/last. Its division count is
     * the month's real length, so it is (re)built when the month rolls; the
     * container keeps those objects below the date pointer created after it. */
    s_date_ring = plain_obj(scr);
    lv_obj_set_size(s_date_ring, GZ_LCD_WIDTH, GZ_LCD_HEIGHT);
    lv_obj_set_pos(s_date_ring, 0, 0);

    /* Twin-aperture moon window: a cream scalloped mask ring, a night-blue sky
     * with a few gold stars, and the lit disc (its radius encodes the
     * illuminated fraction, 0 at new .. full at full moon). */
    disc(scr, MOON_CX, MOON_CY, MOON_R + GZ_UI_S(3), 1, COL_BRASS_D, true, COL_IVORY); /* cream mask */
    disc(scr, MOON_CX, MOON_CY, MOON_R + GZ_UI_S(1), 0, 0, true, COL_NIGHT);           /* night sky  */
    static const int star_dx[5] = { -8,  6, -3,  9, -9 };
    static const int star_dy[5] = { -6, -8,  7,  4,  2 };
    for (int s = 0; s < 5; ++s)
        disc(scr, MOON_CX + GZ_UI_S(star_dx[s]), MOON_CY + GZ_UI_S(star_dy[s]),
             MOON_STAR_R, 0, 0, true, COL_BRASS);                                       /* gold stars */
    s_moon_lit = disc(scr, MOON_CX, MOON_CY, MOON_R, 0, 0, true, COL_SILVER);
    dial_label(scr, &lv_font_montserrat_12, COL_BRASS_D, MOON_CX, MOON_CY + MOON_R + GZ_UI_S(10), "MOON");

    s_ptr_mon  = new_line(scr, COL_BLUE, 2);
    s_ptr_dow  = new_line(scr, COL_BLUE, 2);
    s_ptr_date = new_line(scr, COL_BLUE, 2);

    dial_label(scr, &lv_font_montserrat_12, COL_BRASS_D, WCX, WCY - GZ_UI_S(44), "POSEIDON");
    s_note = dial_label(scr, &lv_font_montserrat_14, COL_INK, WCX, WCY + GZ_UI_S(46), "");

    /* Hands, bottom to top: hour, minute, second, then the centre cap. The
     * hour and minute hands are blued steel; the second hand stays oxblood. */
    s_hand_h = new_line(scr, COL_BLUE, 7);
    s_hand_m = new_line(scr, COL_BLUE, 4);
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
    draw_calendar(&t);
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
    bool force = s_force_draw;
    if (force || t.second != s_drawn_second) {
        draw_hands(&t);
        s_force_draw = false;
    }
    if (force || t.day != s_drawn_day)
        draw_calendar(&t);          /* month / weekday / date / moon: roll by day */
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
