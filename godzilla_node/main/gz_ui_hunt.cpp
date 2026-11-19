/*
 * gz_ui_hunt.cpp — Hunt screen (P3): an RSSI trace for one contact.
 *
 * Pick the contact on the radar: tap its dot, then long-press the card. This
 * screen then shows that contact's identity, its latest RSSI in large type, a
 * stronger / weaker / steady trend, and a trace of the last GZ_HUNT_SAMPLES
 * published readings (about a minute), strong at the top. Walk with the node
 * and watch the number: that is how you close on a transmitter with one omni
 * antenna. It is a signal meter, not a direction finder.
 *
 * Long-press here stops hunting. With no target, the screen says how to pick
 * one. When the contact stops being heard for the radar's TTL the readout
 * turns dim and says "lost"; the trace stays until a new target is picked.
 *
 * Model and geometry are the pure, native-tested gz_hunt.h. The trace is one
 * lv_line over a static point array, positioned at the plot's top-left so
 * only the plot strip is invalidated. Deepwater palette, original layout.
 */
#include "gz_ui_hunt.h"
#include "gz_radar_store.h"
#include "gz_board.h"
#include "gz_geom.h"   /* the one geometry source (#88 §3.2) */
#include "gz_fmt.h"    /* gz_plural */

#include <stdio.h>
#include <string.h>

/* Deepwater palette (shared values with gz_ui.cpp / gz_ui_radar.cpp). */
#define COL_ABYSS    0x031627
#define COL_RING     0x123F5C
#define COL_TEAL     0x2EC4B6
#define COL_FOAM     0xBDEBEB
#define COL_DIM      0x5E8CA8
#define COL_SURVEIL  0xFF4D5A
#define COL_TRACKER  0xFFB020
#define COL_NEUTRAL  0x7FD1DE

/* Layout designed on the 360 canvas; gz_geom.h fits it to the panel: widths
 * clamp, y positions shift by GZ_UI_DY so the block sits centred. */
#define PLOT_X   gz_geom_hunt_plot_x(GZ_UI_W, GZ_UI_ROUND)   /* 60 on the 1.85B  */
#define PLOT_Y   (192 + GZ_UI_DY)
#define PLOT_W   gz_geom_hunt_plot_w(GZ_UI_W, GZ_UI_ROUND)   /* 240 on the 1.85B */
#define PLOT_H   76
#define LABEL_W  gz_geom_col_w(260, GZ_UI_W)                 /* 260 on the 1.85B */
#define PLOT_PAD 4              /* keep the 2 px line off the frame */

static lv_obj_t *s_fam, *s_addr, *s_radio, *s_rssi, *s_trend;
static lv_obj_t *s_plot, *s_trace, *s_foot, *s_stats, *s_age, *s_empty;
static lv_point_precise_t s_pts[GZ_HUNT_SAMPLES];   /* plot-local */
static gz_hunt_t s_h;           /* snapshot this refresh drew */

static uint32_t sev_colour(uint8_t sev)
{
    return sev == GZ_SEV_SURVEIL ? COL_SURVEIL
         : sev == GZ_SEV_TRACKER ? COL_TRACKER : COL_NEUTRAL;
}

static lv_obj_t *label(lv_obj_t *scr, const lv_font_t *font, uint32_t rgb, int y)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, LABEL_W);
    lv_label_set_text(l, "");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y + GZ_UI_DY);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static void fmt_age(uint32_t age_ms, char *buf, size_t n)
{
    uint32_t s = age_ms / 1000u;
    if (s < 60) snprintf(buf, n, "%lus", (unsigned long)s);
    else        snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static void show_target(bool on)
{
    lv_obj_t *parts[] = { s_fam, s_addr, s_radio, s_rssi, s_trend, s_plot, s_foot };
    for (lv_obj_t *o : parts) {
        if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    if (on) lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_remove_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
}

/* ---- build --------------------------------------------------------------- */

void gz_ui_hunt_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_fam   = label(scr, &lv_font_montserrat_20, COL_FOAM, 52);
    s_addr  = label(scr, &lv_font_montserrat_14, COL_FOAM, 80);
    s_radio = label(scr, &lv_font_montserrat_14, COL_DIM, 100);
    s_rssi  = label(scr, &lv_font_montserrat_28, COL_FOAM, 124);
    s_trend = label(scr, &lv_font_montserrat_16, COL_TEAL, 160);

    s_plot = lv_obj_create(scr);
    lv_obj_remove_style_all(s_plot);
    lv_obj_remove_flag(s_plot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_plot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_plot, PLOT_W, PLOT_H);
    lv_obj_set_pos(s_plot, PLOT_X, PLOT_Y);
    lv_obj_set_style_border_width(s_plot, 1, 0);
    lv_obj_set_style_border_color(s_plot, lv_color_hex(COL_RING), 0);
    lv_obj_set_style_border_opa(s_plot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_plot, 8, 0);

    /* Trace points are in plot-local coordinates. */
    s_trace = lv_line_create(s_plot);
    lv_line_set_points(s_trace, s_pts, 0);
    lv_obj_set_pos(s_trace, 0, 0);
    lv_obj_set_style_line_color(s_trace, lv_color_hex(COL_TEAL), 0);
    lv_obj_set_style_line_width(s_trace, 2, 0);
    lv_obj_set_style_line_rounded(s_trace, true, 0);
    lv_obj_remove_flag(s_trace, LV_OBJ_FLAG_CLICKABLE);

    /* Footer: two lines in a flex column (gz_geom.h), so they stack with a
     * fixed gap and a wrapped line can never overlap the next one. */
    s_foot = lv_obj_create(scr);
    lv_obj_remove_style_all(s_foot);
    lv_obj_remove_flag(s_foot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_foot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_foot, LABEL_W, LV_SIZE_CONTENT);
    lv_obj_align(s_foot, LV_ALIGN_TOP_MID, 0, gz_geom_hunt_footer_y(GZ_UI_H));
    lv_obj_set_flex_flow(s_foot, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_foot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_foot, GZ_GEOM_HUNT_FOOT_GAP, 0);
    lv_obj_t **lines[] = { &s_stats, &s_age };
    for (lv_obj_t **l : lines) {
        *l = lv_label_create(s_foot);
        lv_obj_set_style_text_font(*l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(*l, lv_color_hex(COL_DIM), 0);
        lv_obj_set_style_text_align(*l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(*l, LABEL_W);
        lv_obj_remove_flag(*l, LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(*l, "");
    }

    s_empty = label(scr, &lv_font_montserrat_16, COL_DIM, 0);
    lv_label_set_text(s_empty, "HUNT\n\nno contact picked\n\n"
                               "on the radar, tap a dot,\nthen long-press its card");
    lv_obj_align(s_empty, LV_ALIGN_CENTER, 0, 0);

    gz_hunt_clear(&s_h);
    show_target(false);
}

/* ---- refresh ------------------------------------------------------------- */

void gz_ui_hunt_refresh(uint32_t now)
{
    gz_radar_store_hunt_snapshot(&s_h);
    if (!s_h.active) { show_target(false); return; }
    show_target(true);

    const bool lost = gz_hunt_lost(&s_h, now);
    const uint32_t sev_rgb = sev_colour(gz_radar_severity(s_h.family));

    lv_obj_set_style_text_color(s_fam, lv_color_hex(sev_rgb), 0);
    lv_label_set_text(s_fam, gz_radar_family_name(s_h.family));
    lv_label_set_text_fmt(s_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
                          s_h.addr[0], s_h.addr[1], s_h.addr[2],
                          s_h.addr[3], s_h.addr[4], s_h.addr[5]);
    if (s_h.kind == GZ_RADAR_WIFI)
        lv_label_set_text_fmt(s_radio, "Wi-Fi  ch %u", (unsigned)s_h.aux);
    else
        lv_label_set_text_fmt(s_radio, "BLE  %s", s_h.aux ? "random" : "public");

    lv_obj_set_style_text_color(s_rssi, lv_color_hex(lost ? COL_DIM : COL_FOAM), 0);
    lv_label_set_text_fmt(s_rssi, "%d dBm", (int)gz_hunt_latest(&s_h));

    if (lost) {
        lv_obj_set_style_text_color(s_trend, lv_color_hex(COL_DIM), 0);
        lv_label_set_text(s_trend, "lost");
    } else {
        lv_obj_set_style_text_color(s_trend, lv_color_hex(COL_TEAL), 0);
        switch (gz_hunt_trend(&s_h)) {
        case GZ_HUNT_TREND_STRONGER: lv_label_set_text(s_trend, LV_SYMBOL_UP " stronger"); break;
        case GZ_HUNT_TREND_WEAKER:   lv_label_set_text(s_trend, LV_SYMBOL_DOWN " weaker"); break;
        case GZ_HUNT_TREND_STEADY:   lv_label_set_text(s_trend, "steady"); break;
        default:                     lv_label_set_text(s_trend, "listening..."); break;
        }
    }

    /* Trace: plot-local, inset by PLOT_PAD, newest at the right edge. */
    const int l = PLOT_PAD, r = PLOT_W - 1 - PLOT_PAD;
    const int t = PLOT_PAD, b = PLOT_H - 1 - PLOT_PAD;
    for (int i = 0; i < s_h.n; ++i) {
        s_pts[i].x = gz_hunt_x(i, s_h.n, l, r);
        s_pts[i].y = gz_hunt_y(gz_hunt_at(&s_h, i), t, b);
    }
    lv_line_set_points(s_trace, s_pts, s_h.n);
    lv_obj_set_style_line_opa(s_trace, lost ? LV_OPA_40 : LV_OPA_COVER, 0);

    int8_t lo, hi;
    if (gz_hunt_minmax(&s_h, &lo, &hi))
    {
        if (gz_geom_hunt_compact(GZ_UI_W))      /* narrow: fits 224 px unwrapped */
            lv_label_set_text_fmt(s_stats, "%d..%d dBm | %u", (int)lo, (int)hi, (unsigned)s_h.n);
        else
            lv_label_set_text_fmt(s_stats, "min %d  |  max %d  |  %u %s",
                                  (int)lo, (int)hi, (unsigned)s_h.n,
                                  gz_plural(s_h.n, "sample", "samples"));
    }
    char age[16];
    fmt_age(now - s_h.last_ms, age, sizeof age);
    if (gz_geom_hunt_compact(GZ_UI_W))
        lv_label_set_text_fmt(s_age, "%s ago | hold: stop", age);
    else
        lv_label_set_text_fmt(s_age, "heard %s ago  |  hold to stop", age);
}

void gz_ui_hunt_long_press(void)
{
    gz_radar_store_hunt_clear();
    gz_hunt_clear(&s_h);
    show_target(false);
}
