/*
 * gz_face_modular.cpp — Modular face render (watch-faces orders, unit 5).
 * See gz_face_modular.h. Pure grid/time: gz_faceclock.h (native-tested); honest
 * slot values: gz_face_slot; per-slot choices: unit-1 settings (gz_face_store).
 *
 * A big digital time across the top of the shape-safe box, then a 2x2 grid of
 * pods beneath it, each showing the complication the wearer assigned to that
 * slot. Drawing idioms mirror the other faces. [render] — Claudius re-renders.
 */
#include "gz_face_modular.h"
#include "gz_face_slot.h"
#include "gz_face_store.h"      /* saved per-slot settings (unit 1/4) */
#include "../../gz_faceclock.h"
#include "../../gz_geom.h"
#include "../../gz_clock.h"

#include <Arduino.h>
#include <stdio.h>

#define COL_BG     0x031627
#define COL_CELL   0x0A2A40
#define COL_TIME   0xF3E9D2
#define COL_NAME   0x2EC4B6
#define COL_VALUE  0xBDEBEB

static gz_ui_settings_t s_set;
static lv_obj_t *s_time;
static lv_obj_t *s_val[GZ_FACE_SLOTS];
static uint8_t   s_drawn_min = 0xFF;

static lv_obj_t *label(lv_obj_t *scr, const lv_font_t *font, uint32_t rgb,
                       int x, int y, const char *text)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, text);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, x, y);
    return l;
}

void gz_face_modular_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_face_store_load(&s_set);

    gz_rect_t box = gz_safe_box(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, GZ_UI_S(6));
    int time_h = GZ_UI_S(40);

    /* Big digital time across the top of the box. */
    s_time = label(scr, &lv_font_montserrat_20, COL_TIME, box.x, box.y, "--:--");
    lv_obj_set_width(s_time, box.w);
    lv_obj_set_style_text_align(s_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, box.y);

    /* 2x2 grid of pods under the time. */
    gz_rect_t gridbox = { box.x, box.y + time_h, box.w, box.h - time_h };
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        gz_rect_t c;
        gz_grid2x2_cell(gridbox, GZ_UI_S(6), i, &c);
        lv_obj_t *cell = lv_obj_create(scr);
        lv_obj_remove_style_all(cell);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(cell, c.w, c.h);
        lv_obj_set_pos(cell, c.x, c.y);
        lv_obj_set_style_bg_color(cell, lv_color_hex(COL_CELL), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_50, 0);
        lv_obj_set_style_radius(cell, 6, 0);

        gz_comp_id_t comp = (gz_comp_id_t)s_set.slots[i];
        label(scr, &lv_font_montserrat_12, COL_NAME,
              c.x + GZ_UI_S(6), c.y + GZ_UI_S(5), gz_face_comp_title(comp));
        s_val[i] = label(scr, &lv_font_montserrat_14, COL_VALUE,
                         c.x + GZ_UI_S(6), c.y + c.h - GZ_UI_S(20), "--");
    }

    gz_face_modular_refresh(millis());
}

void gz_face_modular_refresh(uint32_t now_ms)
{
    gz_datetime_t t;
    if (gz_clock_now(now_ms, &t)) {
        if (t.minute != s_drawn_min) {
            char hhmm[6];
            gz_fmt_hhmm(t.hour, t.minute, hhmm);
            lv_label_set_text(s_time, hhmm);
            s_drawn_min = t.minute;
        }
    } else {
        lv_label_set_text(s_time, "--:--");
    }

    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        char buf[16];
        gz_face_field_text(gz_comp_subject((gz_comp_id_t)s_set.slots[i]), buf, sizeof buf);
        lv_label_set_text(s_val[i], buf);
    }
}
