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
 *   - a case ring (gold, or silver via GZ_FACE_FINISH) around an ivory dial and
 *     a thin inner ring, centred and drawn to fill the glass then pulled in to
 *     GZ_FACE_PCT so the case clears the edge with a night margin (no bow/crown);
 *   - 60 minute ticks (every fifth heavier) and upright Roman numerals in
 *     Montserrat 18, with the traditional dial "IIII" (gz_watch_roman);
 *   - three printed sub-dials (month / two-letter weekday / date) and a moon
 *     window whose sliding night-shadow is clipped to the aperture;
 *   - black hour and minute hands, and a blue second hand carrying a small
 *     trident head — all lv_line objects positioned by their bounding box (same
 *     technique as the radar sweep), so a move invalidates only that strip. The
 *     second hand TICKS at 1 Hz — it does not sweep — and the face redraws only
 *     when the second changes.
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
 * Palette: abyss 0x031627 (surround), gold 0xC9A227 / dark gold 0x8A6D1B (case,
 * cap) — or silver 0xC7CCD4 / 0x878D97 with GZ_FACE_FINISH=1, ivory 0xF3E9D2
 * (dial), ink 0x1B2A3A (numerals, ticks), black 0x10131A (hour + minute hands),
 * trident-blue 0x2E7BE6 (second hand), oxblood 0xA63A2B (SET / RTC note).
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
#include "gz_timeset.h"/* pure epoch -> civil time for CMD_TIMESET (step 3) */

#include <Arduino.h>
#include <stdio.h>
#include <math.h>

/* Classic is drawn at GZ_FACE_PCT of each panel's safe radius so the case
 * clears the glass with a night margin (Dale, 2026-11-20). One factor feeds
 * every scaled dimension in this file via GZ_UI_SF. */
#define GZ_FACE_PCT 93
#define GZ_UI_SF(v) GZ_UI_S((v) * GZ_FACE_PCT / 100)

#define COL_ABYSS    0x031627
/* Case finish: gold (default) or silver. Compile-time for now; a follow-up
 * wires this to a persisted, customiser-selectable setting (Dale, 2026-11-20). */
#ifndef GZ_FACE_FINISH
#define GZ_FACE_FINISH 0           /* 0 = gold, 1 = silver */
#endif
#if GZ_FACE_FINISH == 1
#define COL_BRASS    0xC7CCD4      /* brushed-silver case / ring */
#define COL_BRASS_D  0x878D97      /* dark silver                */
#else
#define COL_BRASS    0xC9A227      /* gold case / ring           */
#define COL_BRASS_D  0x8A6D1B      /* dark gold                  */
#endif
#define COL_IVORY    0xF3E9D2
#define COL_INK      0x1B2A3A
#define COL_OXBLOOD  0xA63A2B
#define COL_BLUE     0x27406E   /* blued steel: sub-dial pointers                 */
#define COL_NIGHT    0x0B1830   /* moon-aperture night ground                     */
#define COL_SILVER   0xD9DEE6   /* the lit moon disc                              */
#define COL_BLACK    0x10131A   /* hour + minute hands                            */
#define COL_TRIDENT  0x2E7BE6   /* blue trident second hand                       */
#define COL_CRATER   0xB4BAC6   /* moon craters (grey on the lit disc)            */

/* Designed on the 1.85B's 360 canvas; every radial length scales with the
 * panel's safe radius via gz_geom.h (#88 §3.2). The Classic dial is drawn to
 * fill the glass, then pulled in to GZ_FACE_PCT so the case clears the edge with
 * a night margin (Dale, 2026-11-20); GZ_UI_SF folds that factor into GZ_UI_S. */
#define WCX          GZ_UI_CX                      /* 180 on the 1.85B */
#define WCY          (GZ_UI_CY)                    /* centred: no bow             */
#define R_CASE       GZ_UI_SF(178)                  /* outer edge of the case ring */
#define CASE_W       GZ_UI_SF(7)
#define R_INNER      GZ_UI_SF(166)                  /* thin inner ring */
#define R_TICK_OUT   GZ_UI_SF(161)
#define R_TICK_IN    GZ_UI_SF(154)                  /* minute ticks */
#define R_TICK_IN5   GZ_UI_SF(147)                  /* five-minute ticks */
#define R_NUMERAL    GZ_UI_SF(123)
#define LEN_HOUR     GZ_UI_SF(74)
#define LEN_MINUTE   GZ_UI_SF(114)
#define LEN_SECOND   GZ_UI_SF(133)
#define TAIL_HM      GZ_UI_SF(14)
#define TAIL_SECOND  GZ_UI_SF(24)
#define BLINK_MS     500u

/* ---- antique sub-scales (unit 2) ----------------------------------------- */
/* Three printed calendar sub-dials and a twin-aperture moon, nested inside the
 * numeral ring. Each sub-dial has its own centre, a printed scale and a short
 * blued pointer placed by its ring angle (gz_moon.h). The moon window shows the
 * lit fraction for the date. All gate with the file (GZ_BOARD_HAS_RTC); a board
 * without an RTC has no clock faces at all (no-RTC-no-clock-faces), so none of
 * this compiles there. Until the clock has been set — by hand or by a
 * CMD_TIMESET from the Cardputer (gz_ui_watch_apply_timeset) — the dial shows
 * the "hold to set" hint; after a sync it shows the real time/date/moon. */
#define SUB_R        GZ_UI_SF(30)                    /* sub-dial scale radius     */
#define SUB_PTR      GZ_UI_SF(24)                    /* sub-dial pointer length   */
#define SUB_OFF      GZ_UI_SF(62)                    /* sub-dial centre offset    */
#define SUB_DOW_CX   WCX                            /* day-of-week: above centre */
#define SUB_DOW_CY   (WCY - SUB_OFF)
#define SUB_MON_CX   (WCX - SUB_OFF)               /* month: lower left          */
#define SUB_MON_CY   (WCY + GZ_UI_SF(8))
#define SUB_DATE_CX  (WCX + SUB_OFF)               /* date: lower right          */
#define SUB_DATE_CY  (WCY + GZ_UI_SF(8))
#define MOON_CX      WCX                            /* moon aperture: below centre */
#define MOON_CY      (WCY + SUB_OFF)
#define MOON_R       GZ_UI_SF(15)
#define MOON_STAR_R  GZ_UI_SF(1)                      /* gold star dot in the night sky */
#define SUB_TICK_R   SUB_R                           /* division ticks sit on the rim  */
#define SUB_LBL_R    (SUB_R - GZ_UI_SF(9))            /* printed numeral / abbrev radius */

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
static lv_obj_t *s_tri[4];                   /* trident head segments on the second hand */
static lv_point_precise_t s_tri_pts[4][2];

/* Sub-scale pointers (month / day-of-week / date) and the moon window. */
static lv_obj_t *s_ptr_mon, *s_ptr_dow, *s_ptr_date;
static lv_point_precise_t s_pts_mon[2], s_pts_dow[2], s_pts_date[2];
static lv_obj_t *s_moon_lit;                 /* the lit disc inside the aperture */
static lv_obj_t *s_moon_shadow;              /* night disc that carves the phase  */
static int        s_moon_mc = 0;             /* moon centre inside the clip box   */
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
    disc(scr, cx, cy, SUB_R + GZ_UI_SF(3), 1, COL_BRASS_D, true, COL_IVORY);
    disc(scr, cx, cy, 3, 1, COL_BRASS_D, true, COL_BLUE);
}

/* A small ink tick on a sub-dial rim, at ring angle `angle` (tenths, 0 = up). */
static void sub_tick(lv_obj_t *parent, int cx, int cy, int32_t angle, uint32_t rgb)
{
    int x, y;
    gz_watch_polar(angle, SUB_TICK_R, cx, cy, &x, &y);
    disc(parent, x, y, GZ_UI_SF(2), 0, 0, true, rgb);
}

/* A printed sub-scale label (month / weekday abbrev, or a date numeral) inside
 * the rim at ring angle `angle`, in the small engraved font. */
static void sub_label(lv_obj_t *parent, int cx, int cy, int32_t angle, const char *txt)
{
    if (!txt || !txt[0]) return;
    int x, y;
    gz_watch_polar(angle, SUB_LBL_R, cx, cy, &x, &y);
    dial_label(parent, &lv_font_montserrat_8, COL_INK, x, y, txt);
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

/* Point in the second-hand frame: `along` outward from the dial centre, `lat`
 * perpendicular (right of the hand). Used to draw the trident head. */
static void tri_pt(int32_t a_tenths, int along, int lat, int *x, int *y)
{
    float A = (float)a_tenths * 0.0017453292519943f;
    float sa = sinf(A), ca = cosf(A);
    *x = WCX + (int)lroundf((float)along * sa + (float)lat * ca);
    *y = WCY + (int)lroundf(-(float)along * ca + (float)lat * sa);
}
static void tri_seg(int i, int32_t a, int al1, int la1, int al2, int la2)
{
    int x0,y0,x1,y1; tri_pt(a,al1,la1,&x0,&y0); tri_pt(a,al2,la2,&x1,&y1);
    if (s_tri[i]) place_line(s_tri[i], s_tri_pts[i], x0,y0,x1,y1);
}

static void draw_hands(const gz_datetime_t *t)
{
    place_hand(s_hand_h, s_pts_h, gz_watch_hour_angle(t->hour, t->minute),
               LEN_HOUR, TAIL_HM);
    place_hand(s_hand_m, s_pts_m, gz_watch_minute_angle(t->minute, t->second),
               LEN_MINUTE, TAIL_HM);
    place_hand(s_hand_s, s_pts_s, gz_watch_second_angle(t->second),
               LEN_SECOND, TAIL_SECOND);
    {   /* Classic trident head riding the second hand (Dale: style 1). */
        int32_t a = gz_watch_second_angle(t->second);
        const int B  = LEN_SECOND - GZ_UI_SF(22);  /* crossbar / head base */
        const int cw = GZ_UI_SF(7);                /* crossbar half-width  */
        const int TL = GZ_UI_SF(20);               /* outer tine length    */
        const int CL = GZ_UI_SF(28);               /* centre tine (longer) */
        tri_seg(0, a, B, -cw, B,     cw);         /* crossbar             */
        tri_seg(1, a, B,   0, B+CL,   0);         /* centre tine          */
        tri_seg(2, a, B, -cw, B+TL, -cw);         /* left tine (parallel) */
        tri_seg(3, a, B,  cw, B+TL,  cw);         /* right tine           */
    }
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
    /* Slide the night shadow across the lit disc inside the clipped aperture:
     * fully covering at new moon, fully clear at full moon. The clip keeps it
     * from ever spilling onto the dial. */
    int dx = (int)lroundf((float)(2.0 * illum) * (float)MOON_R);  /* 0..2R east */
    lv_obj_set_pos(s_moon_shadow, (s_moon_mc - MOON_R) + dx, s_moon_mc - MOON_R);
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

    /* Dial-only: the pocket-watch bow/crown chain ring is gone and the case
     * fills the glass (Dale, 2026-11-20). */

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
        gz_watch_polar(q * 900, R_TICK_IN5 - GZ_UI_SF(7), WCX, WCY, &x, &y);
        disc(scr, x, y, GZ_UI_SF(4), 1, COL_BRASS_D, true, COL_BRASS);  /* quarter lozenge */
    }

    /* Upright Roman numerals (traditional dial "IIII"), from gz_watch_roman. */
    for (int h = 1; h <= 12; ++h) {
        int x, y;
        gz_watch_polar(h * 300, R_NUMERAL, WCX, WCY, &x, &y);
        dial_label(scr, &lv_font_montserrat_18, COL_INK, x, y, gz_watch_roman(h));
    }

    /* Three printed calendar sub-dials (month / day-of-week / date) and the
     * twin-aperture moon window, all read from the RTC via gz_moon.h. */
    build_subdial(scr, SUB_DOW_CX,  SUB_DOW_CY);
    build_subdial(scr, SUB_MON_CX,  SUB_MON_CY);
    build_subdial(scr, SUB_DATE_CX, SUB_DATE_CY);

    /* Day-of-week ring: two-letter labels at all seven divisions (Dale). */
    static const char *const DOW1[7] = { "Su","Mo","Tu","Wd","Th","Fr","Sa" };
    for (int i = 0; i < 7; ++i) {
        int32_t a = gz_weekday_angle_tenths(i);
        sub_tick(scr, SUB_DOW_CX, SUB_DOW_CY, a, COL_INK);
        sub_label(scr, SUB_DOW_CX, SUB_DOW_CY, a, DOW1[i]);
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

    /* Moon window: lit disc, craters and the sliding night-shadow all live in a
     * CIRCULAR-CLIPPED container (radius = aperture) so the phase shadow can
     * never spill onto the dial. The cream bezel stays outside the clip. */
    disc(scr, MOON_CX, MOON_CY, MOON_R + GZ_UI_SF(3), 1, COL_BRASS_D, true, COL_IVORY); /* cream bezel */
    const int MCLR = MOON_R + GZ_UI_SF(1);           /* clip-container radius */
    s_moon_mc = MCLR;                                /* moon centre inside the clip */
    lv_obj_t *clip = plain_obj(scr);
    lv_obj_set_size(clip, 2 * MCLR, 2 * MCLR);
    lv_obj_set_pos(clip, MOON_CX - MCLR, MOON_CY - MCLR);
    lv_obj_set_style_radius(clip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(clip, lv_color_hex(COL_NIGHT), 0);
    lv_obj_set_style_bg_opa(clip, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(clip, true, 0);     /* clip children to the circle */
    static const int star_dx[5] = { -8,  6, -3,  9, -9 };
    static const int star_dy[5] = { -6, -8,  7,  4,  2 };
    for (int s = 0; s < 5; ++s)
        disc(clip, s_moon_mc + GZ_UI_SF(star_dx[s]), s_moon_mc + GZ_UI_SF(star_dy[s]),
             MOON_STAR_R, 0, 0, true, COL_BRASS);                                        /* gold stars */
    s_moon_lit = disc(clip, s_moon_mc, s_moon_mc, MOON_R, 0, 0, true, COL_SILVER);
    static const int cr_dx[4]={ -5, 3, 6, -2 }, cr_dy[4]={ -4, -6, 3, 6 }, cr_r[4]={ 3,2,2,3 };
    for (int c=0;c<4;++c) disc(clip, s_moon_mc+GZ_UI_SF(cr_dx[c]), s_moon_mc+GZ_UI_SF(cr_dy[c]),
                               GZ_UI_SF(cr_r[c]), 0,0,true, COL_CRATER);
    s_moon_shadow = disc(clip, s_moon_mc, s_moon_mc, MOON_R, 0, 0, true, COL_NIGHT);
    dial_label(scr, &lv_font_montserrat_12, COL_BRASS_D, MOON_CX, MOON_CY + MOON_R + GZ_UI_SF(10), "MOON");

    s_ptr_mon  = new_line(scr, COL_BLUE, 2);
    s_ptr_dow  = new_line(scr, COL_BLUE, 2);
    s_ptr_date = new_line(scr, COL_BLUE, 2);

    s_note = dial_label(scr, &lv_font_montserrat_10, COL_INK, WCX,
                        MOON_CY + MOON_R + GZ_UI_SF(26), "");

    /* Hands, bottom to top: hour, minute, second, then the centre cap. The
     * hour and minute hands are blued steel; the second hand stays oxblood. */
    s_hand_h = new_line(scr, COL_BLACK, 8);
    s_hand_m = new_line(scr, COL_BLACK, 5);
    s_hand_s = new_line(scr, COL_TRIDENT, 2);
    for (int i = 0; i < 4; ++i) s_tri[i] = new_line(scr, COL_TRIDENT, i < 2 ? 3 : 2);
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

void gz_ui_watch_apply_timeset(uint32_t epoch, int16_t tz_offset_min, uint32_t now_ms)
{
    gz_datetime_t t;
    gz_timeset_to_datetime(epoch, tz_offset_min, &t);
    if (!gz_rtc_valid(&t)) return;             /* defensive: never set a bad time */

    bool ok = gz_rtc_write(&t);                /* set the hardware RTC */
    set_clock(&t, now_ms);                     /* software clock + gz_clock_publish */
    s_rtc_fault  = !ok;
    s_needs_set  = false;                      /* synced: drop the "hold to set" note */
    s_force_draw = true;                       /* redraw hands + calendar next refresh */
    update_note();
    Serial.printf("  watch: CMD_TIMESET -> %04u-%02u-%02u %02u:%02u:%02u RTC %s\n",
                  t.year, t.month, t.day, t.hour, t.minute, t.second,
                  ok ? "written" : "WRITE FAILED");
}

#endif /* GZ_BOARD_HAS_RTC */
