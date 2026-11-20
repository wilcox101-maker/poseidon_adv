/*
 * gz_face_modular.cpp — Modular face render (watch-faces orders, unit 5).
 * See gz_face_modular.h. Pure layout + binding: gz_slotface.h (native-tested);
 * tiles via the shared slot renderer gz_slot_render.*.
 *
 * Draws a digital time across a top band and a 2x2 grid of complication tiles
 * below it, each a rounded panel with the complication's title and its live
 * value. Honest bindings only: a slot the board can't source shows an empty
 * tile. The drawing idioms mirror gz_face_command.cpp so the face stays
 * consistent with the rest of the UI. [render] — Claudius re-renders.
 */
#include "gz_face_modular.h"
#include "gz_face_caps.h"       /* gz_faces.h + GZ_FACE_BOARD_CAPS (this board) */
#include "gz_face_store.h"      /* persisted settings, sanitised for this board */
#include "gz_slot_render.h"     /* shared slot title / value renderer           */
#include "../../gz_slotface.h"  /* 2x2 grid + time-band layout (native-tested)   */
#include "../../gz_geom.h"      /* the one geometry source (#88 §3.2)            */
#include "../../gz_clock.h"     /* current time, shared clock (false w/o RTC)    */

#include <Arduino.h>
#include <stdio.h>

/* Deepwater palette (shared with gz_ui_radar.cpp / gz_face_command.cpp). */
#define COL_ABYSS   0x031627
#define COL_RING    0x123F5C
#define COL_TEAL    0x2EC4B6
#define COL_FOAM    0xBDEBEB
#define COL_DIM     0x5E8CA8

#define MGN   GZ_UI_S(8)
#define GAP   GZ_UI_S(8)

static lv_obj_t      *s_time;                      /* big digital time          */
static lv_obj_t      *s_val[GZ_FACE_SLOTS];        /* one value label per tile  */
static gz_subj_field_t s_field[GZ_FACE_SLOTS];     /* field each tile reads     */
static bool           s_tile_live[GZ_FACE_SLOTS];  /* tile bound to a subject?  */

/* ---- helpers (mirror gz_face_command.cpp) -------------------------------- */
static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
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

/* A rounded tile panel at (x, y, w, h). */
static lv_obj_t *tile_panel(lv_obj_t *scr, int x, int y, int w, int h)
{
    lv_obj_t *o = plain_obj(scr);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, GZ_UI_S(10), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(COL_RING), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_30, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(COL_DIM), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    return o;
}

/* HH:MM from the shared clock, or "--:--" before the clock has a time. */
static void time_text(uint32_t now_ms, char *out, size_t n)
{
    gz_datetime_t t;
    if (gz_clock_now(now_ms, &t)) snprintf(out, n, "%02d:%02d", t.hour, t.minute);
    else                          snprintf(out, n, "--:--");
}

/* ---- build --------------------------------------------------------------- */
void gz_face_modular_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_ui_settings_t s;
    gz_face_store_load(&s);     /* already sanitised for this board */

    /* Digital time band across the top of the content box. */
    int tx, ty, tw, th;
    gz_mod_time_band(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, MGN, &tx, &ty, &tw, &th);
    char tb[8];
    time_text(millis(), tb, sizeof tb);
    s_time = label_at(scr, &lv_font_montserrat_28, COL_FOAM,
                      tx + tw / 2, ty + th / 2, tb);

    /* 2x2 complication tiles below the time band. */
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        int cx, cy, cw, ch;
        gz_mod_cell(i, GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, MGN, GAP, &cx, &cy, &cw, &ch);
        tile_panel(scr, cx, cy, cw, ch);

        gz_comp_id_t c = gz_mod_cell_comp(&s, i, GZ_FACE_BOARD_CAPS);
        s_field[i]     = gz_mod_cell_field(&s, i, GZ_FACE_BOARD_CAPS);
        s_tile_live[i] = (c != GZ_COMP_NONE);

        int mid_x = cx + cw / 2;
        label_at(scr, &lv_font_montserrat_12, COL_TEAL,
                 mid_x, cy + ch / 2 - GZ_UI_S(12), gz_slot_comp_title(c));
        s_val[i] = label_at(scr, &lv_font_montserrat_16, COL_FOAM,
                            mid_x, cy + ch / 2 + GZ_UI_S(8), s_tile_live[i] ? "--" : "");
    }

    gz_face_modular_refresh(millis());
}

/* ---- refresh ------------------------------------------------------------- */
void gz_face_modular_refresh(uint32_t now_ms)
{
    char tb[8];
    time_text(now_ms, tb, sizeof tb);
    lv_label_set_text(s_time, tb);

    char buf[16];
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        if (!s_tile_live[i]) continue;         /* empty tile stays blank */
        gz_slot_field_text(s_field[i], buf, sizeof buf);
        lv_label_set_text(s_val[i], buf);
    }
}
