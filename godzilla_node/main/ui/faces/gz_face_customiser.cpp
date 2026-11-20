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
#include "../../gz_geom.h"
#include "../../gz_accent.h"   /* the one accent palette (unit 6) */

#include <stdio.h>

#define COL_BG      0x031627
#define COL_ROW     0x0A2A40
#define COL_NAME    0x7FD1DE
#define COL_VALUE   0xBDEBEB
#define COL_TITLE   0x2EC4B6

/* Accent swatches come from the one shared palette, gz_accent_rgb (unit 6), so
 * the customiser and every face show the same nine colours. */

enum { ROW_FACE = 0, ROW_ACCENT, ROW_BG, ROW_AON,
       ROW_SLOT0, ROW_SLOT1, ROW_SLOT2, ROW_SLOT3, ROW_COUNT };

static gz_ui_settings_t s_set;
static gz_rect_t        s_box;
static int              s_rowh;
static lv_obj_t        *s_val[ROW_COUNT];
static lv_obj_t        *s_accent_sw;

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

/* ---- row value text ------------------------------------------------------ */
static void row_value(int row, char *out, size_t n)
{
    switch (row) {
    case ROW_FACE:   snprintf(out, n, "%s", face_name(s_set.face));             break;
    case ROW_ACCENT: snprintf(out, n, "#%d", s_set.accent + 1);                 break;
    case ROW_BG:     snprintf(out, n, "%s", s_set.background == GZ_BG_TEXTURE ? "Texture" : "Black"); break;
    case ROW_AON:    snprintf(out, n, "%s", s_set.always_on ? "On" : "Off");    break;
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
}

/* ---- build --------------------------------------------------------------- */
void gz_face_customiser_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_face_store_load(&s_set);                 /* saved + sanitised for this board */

    s_box  = gz_safe_box(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, GZ_UI_S(6));
    s_rowh = s_box.h / ROW_COUNT;

    static const char *const NAME[ROW_COUNT] = {
        "Face", "Accent", "Background", "Always-on",
        "Pod 1", "Pod 2", "Pod 3", "Pod 4",
    };
    int valx = s_box.x + s_box.w / 2;
    for (int r = 0; r < ROW_COUNT; ++r) {
        int y = s_box.y + r * s_rowh + (s_rowh - GZ_UI_S(14)) / 2;
        /* row strip */
        lv_obj_t *strip = plain_obj(scr);
        lv_obj_set_size(strip, s_box.w, s_rowh - 2);
        lv_obj_set_pos(strip, s_box.x, s_box.y + r * s_rowh + 1);
        lv_obj_set_style_bg_color(strip, lv_color_hex(COL_ROW), 0);
        lv_obj_set_style_bg_opa(strip, (r & 1) ? LV_OPA_40 : LV_OPA_20, 0);
        lv_obj_set_style_radius(strip, 4, 0);

        label_tl(scr, COL_NAME, s_box.x + GZ_UI_S(6), y, NAME[r]);
        s_val[r] = label_tl(scr, COL_VALUE, valx + GZ_UI_S(16), y, "");
    }

    /* Accent swatch sits just left of the accent value. */
    s_accent_sw = plain_obj(scr);
    int sw = GZ_UI_S(12);
    lv_obj_set_size(s_accent_sw, sw, sw);
    lv_obj_set_style_radius(s_accent_sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_accent_sw, LV_OPA_COVER, 0);
    lv_obj_align(s_accent_sw, LV_ALIGN_TOP_LEFT, valx - GZ_UI_S(2),
                 s_box.y + ROW_ACCENT * s_rowh + (s_rowh - sw) / 2);

    /* Title band. */
    lv_obj_t *t = lv_label_create(scr);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(COL_TITLE), 0);
    lv_label_set_text(t, "CUSTOMISE");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, s_box.y - GZ_UI_S(2));

    refresh_rows();
}

void gz_face_customiser_show(void)
{
    gz_face_store_load(&s_set);
    refresh_rows();
}

/* ---- tap ----------------------------------------------------------------- */
void gz_face_customiser_tap(int x, int y)
{
    if (s_rowh <= 0) return;
    if (y < s_box.y || y >= s_box.y + s_rowh * ROW_COUNT) return;
    if (x < s_box.x || x >= s_box.x + s_box.w) return;
    int row = (y - s_box.y) / s_rowh;
    if (row < 0 || row >= ROW_COUNT) return;

    const uint8_t caps = GZ_FACE_BOARD_CAPS;
    switch (row) {
    case ROW_FACE:
        s_set.face = (uint8_t)gz_cust_cycle_face((gz_face_id_t)s_set.face, +1, caps);
        break;
    case ROW_ACCENT:
        s_set.accent = (uint8_t)gz_cust_cycle(s_set.accent, +1, GZ_ACCENT_COUNT);
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
    refresh_rows();
}

const gz_ui_settings_t *gz_face_customiser_settings(void) { return &s_set; }
