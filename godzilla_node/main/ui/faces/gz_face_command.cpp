/*
 * gz_face_command.cpp — Command face render (watch-faces orders, unit 3).
 * See gz_face_command.h. Pure model + maths: gz_cmdface.h (native-tested).
 *
 * Draws a radar dial (range rings, crosshair, slow sweep, central node glyph)
 * with analog hour/minute hands over it, four complication pods bound to the
 * live subjects, and a bottom status ticker. Live device dots belong to the
 * existing radar renderer and are added when the face joins the screen ring;
 * this file draws the dial and the honest pod/ticker values only.
 *
 * Firmware-only (LVGL). The drawing idioms (plain object, disc, bounding-box
 * line) mirror gz_ui_watch.cpp / gz_ui_radar.cpp so the face stays consistent
 * with the rest of the UI. [render] — Claudius re-renders on the three boards.
 */
#include "gz_face_command.h"
#include "gz_face_caps.h"     /* gz_faces.h + GZ_FACE_BOARD_CAPS (this board) */
#include "gz_face_subjects.h"
#include "../../gz_geom.h"    /* the one geometry source (#88 §3.2) */
#include "../../gz_watch.h"   /* hand angles + bounding-box hand */
#include "../../gz_clock.h"   /* current time, shared clock (false w/o RTC) */

#include <Arduino.h>
#include <stdio.h>

/* Deepwater palette (shared with gz_ui_radar.cpp). */
#define COL_ABYSS   0x031627
#define COL_RING    0x123F5C
#define COL_TEAL    0x2EC4B6
#define COL_FOAM    0xBDEBEB
#define COL_DIM     0x5E8CA8
#define COL_SWEEP   0x2EC4B6
#define COL_HAND    0xBDEBEB
#define COL_OXBLOOD 0xA63A2B

#define CCX       GZ_UI_CX
#define CCY       GZ_UI_CY
#define R_MAX     gz_geom_radar_r_max(GZ_UI_W, GZ_UI_H)   /* 158 on the 1.85B */
#define POD_R     GZ_UI_S(GZ_CMD_POD_R_DESIGN)             /* disc radius (scaled) */
#define POD_MGN   gz_cmd_pod_margin(GZ_UI_W, GZ_UI_H)       /* edge keep-out (scaled) */
/* Pods are PLACED by their footprint (disc + unscaled title label), not the bare
 * disc, so a wide title never clips the glass on the narrow ES3C28P (B1). */
#define POD_HW    gz_cmd_pod_hw(GZ_UI_W, GZ_UI_H)
#define SWEEP_STEP_DEG 9
#define LEN_HOUR  GZ_UI_S(52)
#define LEN_MIN   GZ_UI_S(78)
#define TAIL_HM   GZ_UI_S(12)

static lv_obj_t *s_sweep, *s_hand_h, *s_hand_m, *s_ticker;
static lv_point_precise_t s_sweep_pts[2], s_pts_h[2], s_pts_m[2];
static uint16_t s_sweep_deg;
static uint8_t  s_drawn_min = 0xFF;

/* one pod: a title fixed at build, a value refreshed from its subject */
static lv_obj_t      *s_pod_val[GZ_FACE_SLOTS];
static gz_subj_field_t s_pod_field[GZ_FACE_SLOTS];

/* ---- helpers (mirror gz_ui_watch.cpp) ------------------------------------ */
static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

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

static lv_obj_t *label_at(lv_obj_t *scr, const lv_font_t *font, uint32_t rgb,
                          int x, int y, const char *text)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_CENTER, x - GZ_UI_W / 2, y - GZ_UI_H / 2);
    return l;
}

/* Short title for a pod's complication. */
static const char *comp_title(gz_comp_id_t c)
{
    switch (c) {
    case GZ_COMP_STATUS:   return "NODE";
    case GZ_COMP_CONTACTS: return "CONTACT";
    case GZ_COMP_PUBLISH:  return "PUBLISH";
    case GZ_COMP_CHANNEL:  return "CHAN";
    case GZ_COMP_IMU:      return "TILT";
    case GZ_COMP_BATTERY:  return "BATT";
    case GZ_COMP_RECON:    return "RECON";
    default:               return "";
    }
}

static lv_subject_t *field_subject(gz_subj_field_t f)
{
    gz_subjects_t *s = gz_subjects();
    switch (f) {
    case GZ_SUBJ_HEAP_KB:   return &s->heap_kb;
    case GZ_SUBJ_CONTACTS:  return &s->contacts;
    case GZ_SUBJ_RSSI_BEST: return &s->rssi_best;
    case GZ_SUBJ_PUB_AGE_S: return &s->pub_age_s;
    case GZ_SUBJ_CHANNEL:   return &s->channel;
    case GZ_SUBJ_IMU_TILT:  return &s->imu_tilt;
    case GZ_SUBJ_BATT_PCT:  return &s->batt_pct;
    case GZ_SUBJ_RECON_ON:  return &s->recon_on;
    default:                return nullptr;
    }
}

/* Format a field's current value into `out`. */
static void field_text(gz_subj_field_t f, char *out, size_t n)
{
    lv_subject_t *subj = field_subject(f);
    int v = subj ? (int)lv_subject_get_int(subj) : 0;
    switch (f) {
    case GZ_SUBJ_HEAP_KB:   snprintf(out, n, "%dk", v);            break;
    case GZ_SUBJ_CONTACTS:  snprintf(out, n, "%d", v);            break;
    case GZ_SUBJ_PUB_AGE_S: if (v < 0) snprintf(out, n, "--");
                            else       snprintf(out, n, "%ds", v); break;
    case GZ_SUBJ_CHANNEL:   snprintf(out, n, "%d", v);            break;
    case GZ_SUBJ_IMU_TILT:  snprintf(out, n, "%d\xC2\xB0", v);    break;
    case GZ_SUBJ_BATT_PCT:  if (v < 0) snprintf(out, n, "--");
                            else       snprintf(out, n, "%d%%", v); break;
    case GZ_SUBJ_RECON_ON:  snprintf(out, n, v ? "ON" : "off");   break;
    default:                snprintf(out, n, "--");               break;
    }
}

/* ---- build --------------------------------------------------------------- */
void gz_face_command_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Three range rings + crosshair. */
    for (int k = 1; k <= 3; ++k)
        disc(scr, CCX, CCY, R_MAX * k / 3, 1, COL_RING, false, 0);
    static lv_point_precise_t s_hx[2], s_vx[2];
    lv_obj_t *hx = new_line(scr, COL_RING, 1);
    place_line(hx, s_hx, CCX - R_MAX, CCY, CCX + R_MAX, CCY);
    lv_obj_t *vx = new_line(scr, COL_RING, 1);
    place_line(vx, s_vx, CCX, CCY - R_MAX, CCX, CCY + R_MAX);

    /* Slow sweep line (updated each refresh). */
    s_sweep = new_line(scr, COL_SWEEP, 2);

    /* Four complication pods at round-safe corners (gz_cmdface.h). */
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        int px, py;
        gz_cmd_pod_pos(i, GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, POD_HW, POD_MGN, &px, &py);
        gz_comp_id_t c = gz_cmd_pod_comp(i, GZ_FACE_BOARD_CAPS);
        s_pod_field[i] = gz_comp_subject(c);
        disc(scr, px, py, POD_R, 1, COL_DIM, true, COL_RING);
        label_at(scr, &lv_font_montserrat_12, COL_TEAL, px, py - GZ_UI_S(9), comp_title(c));
        s_pod_val[i] = label_at(scr, &lv_font_montserrat_14, COL_FOAM, px, py + GZ_UI_S(7), "--");
    }

    /* Central node glyph + analog hands over the dial. */
    s_hand_h = new_line(scr, COL_HAND, 5);
    s_hand_m = new_line(scr, COL_HAND, 3);
    disc(scr, CCX, CCY, GZ_UI_S(6), 1, COL_DIM, true, COL_TEAL);

    /* Bottom status ticker. */
    s_ticker = label_at(scr, &lv_font_montserrat_12, COL_DIM, CCX, CCY + R_MAX + GZ_UI_S(10), "");

    gz_face_command_refresh(millis());
}

/* ---- refresh ------------------------------------------------------------- */
void gz_face_command_refresh(uint32_t now_ms)
{
    /* Sweep advances every refresh. */
    s_sweep_deg = (uint16_t)((s_sweep_deg + SWEEP_STEP_DEG) % 360u);
    int sx, sy;
    gz_watch_polar((int32_t)s_sweep_deg * 10, R_MAX, CCX, CCY, &sx, &sy);
    place_line(s_sweep, s_sweep_pts, CCX, CCY, sx, sy);

    /* Analog hands, only once the clock has a real time (no RTC -> hidden). */
    gz_datetime_t t;
    if (gz_clock_now(now_ms, &t)) {
        if (t.minute != s_drawn_min) {
            int x0, y0, x1, y1;
            gz_watch_hand(gz_watch_hour_angle(t.hour, t.minute), LEN_HOUR, TAIL_HM,
                          CCX, CCY, &x0, &y0, &x1, &y1);
            place_line(s_hand_h, s_pts_h, x0, y0, x1, y1);
            gz_watch_hand(gz_watch_minute_angle(t.minute, t.second), LEN_MIN, TAIL_HM,
                          CCX, CCY, &x0, &y0, &x1, &y1);
            place_line(s_hand_m, s_pts_m, x0, y0, x1, y1);
            s_drawn_min = t.minute;
        }
        lv_obj_remove_flag(s_hand_h, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_hand_m, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_hand_h, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hand_m, LV_OBJ_FLAG_HIDDEN);
    }

    /* Pod values from the live subjects. */
    char buf[16];
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        field_text(s_pod_field[i], buf, sizeof buf);
        lv_label_set_text(s_pod_val[i], buf);
    }

    /* Status ticker: the honest headline numbers. */
    gz_subjects_t *s = gz_subjects();
    char tick[48];
    int age = (int)lv_subject_get_int(&s->pub_age_s);
    char agebuf[8];
    if (age < 0) snprintf(agebuf, sizeof agebuf, "--");
    else         snprintf(agebuf, sizeof agebuf, "%ds", age);
    snprintf(tick, sizeof tick, "CONTACTS %d  CH %d  PUB %s",
             (int)lv_subject_get_int(&s->contacts),
             (int)lv_subject_get_int(&s->channel), agebuf);
    lv_label_set_text(s_ticker, tick);
}

/* ---- tap ----------------------------------------------------------------- */
gz_cmd_feature_t gz_face_command_tap(int x, int y)
{
    return gz_cmd_quadrant(x, y, CCX, CCY);
}
