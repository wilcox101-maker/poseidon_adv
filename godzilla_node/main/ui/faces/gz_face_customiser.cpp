/*
 * gz_face_customiser.cpp — on-device UI customiser screen (unit 4).
 * See gz_face_customiser.h. Pure model + maths: gz_customiser.h / gz_safearea.h
 * (native-tested); persistence: gz_face_store.h (unit 1).
 *
 * A column of eight rows laid inside the shape safe-area box: Face, Accent,
 * Background, Always-on, and one row per pod. A tap on a row steps that setting
 * with the pure cyclers (the pod picker offers only board-sourceable
 * complications) and writes the result to NVS. Drawing idioms mirror the other
 * faces. [render] — Claudius re-renders on the three boards.
 */
#include "gz_face_customiser.h"
#include "gz_face_caps.h"      /* gz_faces.h + GZ_FACE_BOARD_CAPS (this board) */
#include "gz_face_store.h"     /* NVS load / save (unit 1) */
#include "../../gz_customiser.h"
#include "../../gz_cust_layout.h"  /* item M: one source for the list row geometry */
#include "../../gz_geom.h"
#include "../../gz_accent.h"   /* the one accent palette (unit 6) */
#include "../../gz_alerts_page.h"  /* Alerts subpage pure model (row<->bit, count) */
#include "../../gz_ui.h"           /* gz_ui_apply_settings — live apply (item I) */

#include <stdio.h>

#define COL_BG      0x031627
#define COL_ROW     0x0A2A40
#define COL_NAME    0x7FD1DE
#define COL_VALUE   0xBDEBEB
#define COL_TITLE   0x2EC4B6

/* Accent swatches come from the one shared palette, gz_accent_rgb (unit 6), so
 * the customiser and every face show the same nine colours. */

enum { ROW_FACE = 0, ROW_ACCENT, ROW_FINISH, ROW_BG, ROW_AON, ROW_ALERTS,
       ROW_SLOT0, ROW_SLOT1, ROW_SLOT2, ROW_SLOT3, ROW_COUNT };

/* Case-finish swatch colours — the Classic watch's gold / silver case metal
 * (gz_ui_watch.cpp COL_BRASS_GOLD / COL_BRASS_SILVER), shown as a preview dot. */
static const uint32_t FINISH_SWATCH[GZ_FINISH_COUNT] = { 0xC9A227, 0xC7CCD4 };

static gz_ui_settings_t s_set;
static gz_rect_t        s_box;
static gz_cust_list_t   s_main_L;      /* main-page row geometry (gz_cust_layout.h) */
static lv_obj_t        *s_val[ROW_COUNT];
static lv_obj_t        *s_accent_sw;
static lv_obj_t        *s_finish_sw;

/* ---- Alerts subpage state (one page shown at a time) --------------------- */
static lv_obj_t        *s_alerts_scr;                      /* full-screen overlay */
static lv_obj_t        *s_alerts_val[GZ_ALERTS_FAMILY_COUNT];
static gz_rect_t        s_alerts_box;
static gz_cust_list_t   s_alerts_L;    /* subpage row geometry — all rows fit, no scroll (item M) */
static bool             s_on_alerts;                       /* true -> subpage up */

/* ---- names --------------------------------------------------------------- */
static const char *face_name(uint8_t f)
{
    switch (f) {
    case GZ_FACE_CLASSIC: return "Classic";
    case GZ_FACE_COMMAND: return "Command";
    case GZ_FACE_MODULAR: return "Modular";
    case GZ_FACE_MINIMAL: return "Minimal";
    default:              return "?";
    }
}

static const char *comp_name(uint8_t c)
{
    switch (c) {
    case GZ_COMP_NONE:     return "--";
    case GZ_COMP_STATUS:   return "Node";
    case GZ_COMP_CONTACTS: return "Contacts";
    case GZ_COMP_PUBLISH:  return "Publish";
    case GZ_COMP_CHANNEL:  return "Channel";
    case GZ_COMP_IMU:      return "Tilt";
    case GZ_COMP_BATTERY:  return "Battery";
    case GZ_COMP_RECON:    return "Recon";
    case GZ_COMP_DECODE:   return "Decode";
    case GZ_COMP_CLOCK:    return "Clock";
    default:               return "?";
    }
}

/* ---- helpers ------------------------------------------------------------- */
static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label_tl(lv_obj_t *scr, uint32_t rgb, int x, int y, const char *text)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, x, y);
    return l;
}

static const char *finish_name(uint8_t f)
{
    return f == GZ_FINISH_SILVER ? "Silver" : "Gold";
}

/* ---- row value text ------------------------------------------------------ */
static void row_value(int row, char *out, size_t n)
{
    switch (row) {
    case ROW_FACE:   snprintf(out, n, "%s", face_name(s_set.face));             break;
    case ROW_ACCENT: snprintf(out, n, "#%d", s_set.accent + 1);                 break;
    case ROW_FINISH: snprintf(out, n, "%s", finish_name(s_set.finish));         break;
    case ROW_BG:     snprintf(out, n, "%s", s_set.background == GZ_BG_TEXTURE ? "Texture" : "Black"); break;
    case ROW_AON:    snprintf(out, n, "%s", s_set.always_on ? "On" : "Off");    break;
    case ROW_ALERTS: snprintf(out, n, "%d/%d on", gz_alerts_count_on(s_set.alert_mask), GZ_ALERTS_TOTAL); break;
    default:         snprintf(out, n, "%s", comp_name(s_set.slots[row - ROW_SLOT0])); break;
    }
}

static void refresh_rows(void)
{
    char buf[16];
    for (int r = 0; r < ROW_COUNT; ++r) {
        row_value(r, buf, sizeof buf);
        lv_label_set_text(s_val[r], buf);
    }
    lv_obj_set_style_bg_color(s_accent_sw, lv_color_hex(gz_accent_rgb(s_set.accent)), 0);
    uint8_t fin = s_set.finish < GZ_FINISH_COUNT ? s_set.finish : GZ_FINISH_GOLD;
    lv_obj_set_style_bg_color(s_finish_sw, lv_color_hex(FINISH_SWATCH[fin]), 0);
}

/* ---- Alerts subpage ------------------------------------------------------ */
/* The customiser's "Alerts" row opens this page: one On/Off row per alerting
 * family (gz_alerts_page.h) plus Back. Each tap flips one bit in
 * s_set.alert_mask and persists through the same NVS writer as the main rows;
 * no device, NVS-beyond-settings, RTC or SD state is touched, and nothing
 * transmits. LE_EQUIP and ALPR have no bit and no row (radar-only) — the ASCII
 * footer says so. [render] — Claudius re-renders on the three boards. */
static void alerts_refresh(void)
{
    for (int r = 0; r < GZ_ALERTS_FAMILY_COUNT; ++r)
        lv_label_set_text(s_alerts_val[r],
                          gz_alerts_row_on(s_set.alert_mask, r) ? "On" : "Off");
}

static void build_alerts_page(lv_obj_t *scr)
{
    s_alerts_scr = plain_obj(scr);
    lv_obj_set_size(s_alerts_scr, GZ_UI_W, GZ_UI_H);
    lv_obj_set_pos(s_alerts_scr, 0, 0);
    lv_obj_set_style_bg_color(s_alerts_scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_alerts_scr, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_alerts_scr, LV_OBJ_FLAG_HIDDEN);          /* main page first */

    s_alerts_box = gz_safe_box(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, GZ_UI_S(6));
    /* Title and footer get their own lines INSIDE the safe box; the eight rows
     * divide the rest evenly (gz_cust_layout.h). All eight fit every board, so
     * the list is NOT scrolled — the old scrollable band could never be dragged
     * anyway (its parents were non-clickable), and the rows stay at least the
     * Montserrat-14 line height (fonts do not scale — #150). Item M. */
    int th = gz_cust_band_h(lv_font_get_line_height(&lv_font_montserrat_12), GZ_UI_S(4));
    s_alerts_L = gz_cust_list(s_alerts_box, th, th, GZ_ALERTS_ROW_COUNT);

    int valx = s_alerts_box.x + s_alerts_box.w / 2;
    int lh14 = lv_font_get_line_height(&lv_font_montserrat_14);
    for (int r = 0; r < GZ_ALERTS_ROW_COUNT; ++r) {
        int ry = s_alerts_L.rows_y + r * s_alerts_L.row_h;
        bool back = (r == GZ_ALERTS_ROW_BACK);

        lv_obj_t *strip = plain_obj(s_alerts_scr);
        lv_obj_set_size(strip, s_alerts_box.w, s_alerts_L.row_h - 2);
        lv_obj_set_pos(strip, s_alerts_box.x, ry + 1);
        lv_obj_set_style_bg_color(strip, lv_color_hex(COL_ROW), 0);
        lv_obj_set_style_bg_opa(strip, (r & 1) ? LV_OPA_40 : LV_OPA_20, 0);
        lv_obj_set_style_radius(strip, 4, 0);

        int ty = gz_cust_row_text_y(&s_alerts_L, r, lh14);
        lv_obj_t *nm = lv_label_create(s_alerts_scr);
        lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nm, lv_color_hex(back ? COL_TITLE : COL_NAME), 0);
        lv_label_set_text(nm, gz_alerts_row_name(r));
        lv_obj_set_pos(nm, s_alerts_box.x + GZ_UI_S(6), ty);

        if (!back) {
            lv_obj_t *v = lv_label_create(s_alerts_scr);
            lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(v, lv_color_hex(COL_VALUE), 0);
            lv_label_set_text(v, "Off");
            lv_obj_set_pos(v, valx + GZ_UI_S(16), ty);
            s_alerts_val[r] = v;
        }
    }

    /* Title at the top of the box, informational footer below the last row
     * (ASCII only), both inside the safe box. */
    lv_obj_t *t = lv_label_create(s_alerts_scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(COL_TITLE), 0);
    lv_label_set_text(t, "ALERTS");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, s_alerts_box.y);

    lv_obj_t *f = lv_label_create(s_alerts_scr);
    lv_obj_set_style_text_font(f, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(f, lv_color_hex(COL_NAME), 0);
    lv_label_set_text(f, "LE gear / ALPR: radar only");
    lv_obj_align(f, LV_ALIGN_TOP_MID, 0, s_alerts_L.footer_y + GZ_UI_S(2));

    alerts_refresh();
}

static void show_alerts_page(void)
{
    if (!s_alerts_scr) return;
    s_on_alerts = true;
    alerts_refresh();
    lv_obj_remove_flag(s_alerts_scr, LV_OBJ_FLAG_HIDDEN);
}

static void hide_alerts_page(void)
{
    s_on_alerts = false;
    if (s_alerts_scr) lv_obj_add_flag(s_alerts_scr, LV_OBJ_FLAG_HIDDEN);
    refresh_rows();                                        /* main "n/7 on" updates */
}

/* Route a tap while the Alerts subpage is up. All rows are on screen (no scroll,
 * item M), so the row is a direct y -> row map (gz_cust_layout.h). */
static void alerts_tap(int x, int y)
{
    int row = gz_cust_row_at(&s_alerts_L, s_alerts_box, x, y, GZ_ALERTS_ROW_COUNT);
    if (row < 0) return;

    if (row == GZ_ALERTS_ROW_BACK) { hide_alerts_page(); return; }
    if (gz_alerts_row_is_family(row)) {
        s_set.alert_mask = gz_alerts_toggle(s_set.alert_mask, row);
        gz_face_store_save(&s_set);     /* same NVS writer as the main rows */
        gz_ui_apply_settings(&s_set);   /* alert overlay picks up the new mask live */
        alerts_refresh();
    }
}

/* ---- build --------------------------------------------------------------- */
void gz_face_customiser_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_face_store_load(&s_set);                 /* saved + sanitised for this board */

    s_box  = gz_safe_box(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, GZ_UI_S(6));
    /* Title gets its own line inside the box; the ten rows divide the rest
     * evenly (gz_cust_layout.h — render review: the title used to overlap the
     * first row). */
    int th  = gz_cust_band_h(lv_font_get_line_height(&lv_font_montserrat_12), GZ_UI_S(4));
    s_main_L = gz_cust_list(s_box, th, 0, ROW_COUNT);   /* no footer */

    static const char *const NAME[ROW_COUNT] = {
        "Face", "Accent", "Finish", "Background", "Always-on", "Alerts",
        "Pod 1", "Pod 2", "Pod 3", "Pod 4",
    };
    int valx = s_box.x + s_box.w / 2;
    int lh14 = lv_font_get_line_height(&lv_font_montserrat_14);
    for (int r = 0; r < ROW_COUNT; ++r) {
        int y = gz_cust_row_text_y(&s_main_L, r, lh14);
        /* row strip */
        lv_obj_t *strip = plain_obj(scr);
        lv_obj_set_size(strip, s_box.w, s_main_L.row_h - 2);
        lv_obj_set_pos(strip, s_box.x, s_main_L.rows_y + r * s_main_L.row_h + 1);
        lv_obj_set_style_bg_color(strip, lv_color_hex(COL_ROW), 0);
        lv_obj_set_style_bg_opa(strip, (r & 1) ? LV_OPA_40 : LV_OPA_20, 0);
        lv_obj_set_style_radius(strip, 4, 0);

        label_tl(scr, COL_NAME, s_box.x + GZ_UI_S(6), y, NAME[r]);
        s_val[r] = label_tl(scr, COL_VALUE, valx + GZ_UI_S(16), y, "");
    }

    /* Accent + finish swatches sit just left of their row values. */
    int sw = GZ_UI_S(12);
    s_accent_sw = plain_obj(scr);
    lv_obj_set_size(s_accent_sw, sw, sw);
    lv_obj_set_style_radius(s_accent_sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_accent_sw, LV_OPA_COVER, 0);
    lv_obj_align(s_accent_sw, LV_ALIGN_TOP_LEFT, valx - GZ_UI_S(2),
                 s_main_L.rows_y + ROW_ACCENT * s_main_L.row_h + (s_main_L.row_h - sw) / 2);

    s_finish_sw = plain_obj(scr);
    lv_obj_set_size(s_finish_sw, sw, sw);
    lv_obj_set_style_radius(s_finish_sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_finish_sw, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_finish_sw, 1, 0);
    lv_obj_set_style_border_color(s_finish_sw, lv_color_hex(COL_NAME), 0);
    lv_obj_align(s_finish_sw, LV_ALIGN_TOP_LEFT, valx - GZ_UI_S(2),
                 s_main_L.rows_y + ROW_FINISH * s_main_L.row_h + (s_main_L.row_h - sw) / 2);

    /* Title band. */
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(COL_TITLE), 0);
    lv_label_set_text(t, "CUSTOMISE");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, s_box.y);

    build_alerts_page(scr);         /* the Alerts subpage overlay (hidden) */
    s_on_alerts = false;
    refresh_rows();
}

void gz_face_customiser_show(void)
{
    gz_face_store_load(&s_set);
    hide_alerts_page();             /* always reopen on the main page */
}

/* ---- tap ----------------------------------------------------------------- */
void gz_face_customiser_tap(int x, int y)
{
    if (s_on_alerts) { alerts_tap(x, y); return; }   /* subpage owns the taps */

    int row = gz_cust_row_at(&s_main_L, s_box, x, y, ROW_COUNT);
    if (row < 0) return;

    if (row == ROW_ALERTS) { show_alerts_page(); return; }  /* open, nothing to save */

    const uint8_t caps = GZ_FACE_BOARD_CAPS;
    switch (row) {
    case ROW_FACE:
        s_set.face = (uint8_t)gz_cust_cycle_face((gz_face_id_t)s_set.face, +1, caps);
        break;
    case ROW_ACCENT:
        s_set.accent = (uint8_t)gz_cust_cycle(s_set.accent, +1, GZ_ACCENT_COUNT);
        break;
    case ROW_FINISH:
        s_set.finish = (uint8_t)gz_cust_cycle(s_set.finish, +1, GZ_FINISH_COUNT);
        break;
    case ROW_BG:
        s_set.background = (uint8_t)gz_cust_cycle(s_set.background, +1, GZ_BG_COUNT);
        break;
    case ROW_AON:
        s_set.always_on = s_set.always_on ? 0 : 1;
        break;
    default: {
        int slot = row - ROW_SLOT0;
        s_set.slots[slot] =
            (uint8_t)gz_cust_cycle_comp((gz_comp_id_t)s_set.slots[slot], +1, caps);
        break;
    }
    }
    gz_face_store_save(&s_set);     /* persist every change (unit-1 writer) */
    gz_ui_apply_settings(&s_set);   /* finish rebuild / overlay update, no reboot (item I) */
    refresh_rows();
}

const gz_ui_settings_t *gz_face_customiser_settings(void) { return &s_set; }
