/*
 * gz_face_minimal.cpp — Minimal face render (watch-faces orders, unit 5).
 * See gz_face_minimal.h. Big HH:MM time + one honest status line.
 *
 * Reuses gz_faceclock.h (pure time format, native-tested) and gz_face_slot for
 * the status value. The status line follows the wearer's first pod choice
 * (unit-1 settings); an empty pod 1 falls back to the contact count — always an
 * all-board field. [render] — Claudius re-renders on the three boards.
 */
#include "gz_face_minimal.h"
#include "gz_face_slot.h"
#include "gz_face_store.h"
#include "../../gz_faceclock.h"
#include "../../gz_geom.h"
#include "../../gz_clock.h"

#include <Arduino.h>
#include <stdio.h>

#define COL_BG     0x031627
#define COL_TIME   0xF3E9D2
#define COL_LINE   0x7FD1DE

static gz_ui_settings_t s_set;
static gz_comp_id_t     s_line_comp;
static lv_obj_t        *s_time, *s_line;
static uint8_t          s_drawn_min = 0xFF;

void gz_face_minimal_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_face_store_load(&s_set);
    /* Status line: the first pod's complication, or contacts if that pod is
     * empty (contacts is an all-board field). */
    s_line_comp = (gz_comp_id_t)s_set.slots[0];
    if (s_line_comp == GZ_COMP_NONE) s_line_comp = GZ_COMP_CONTACTS;

    s_time = lv_label_create(scr);
    lv_obj_set_style_text_font(s_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time, lv_color_hex(COL_TIME), 0);
    lv_label_set_text(s_time, "--:--");
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -GZ_UI_S(10));

    s_line = lv_label_create(scr);
    lv_obj_set_style_text_font(s_line, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_line, lv_color_hex(COL_LINE), 0);
    lv_label_set_text(s_line, "");
    lv_obj_align(s_line, LV_ALIGN_CENTER, 0, GZ_UI_S(18));

    gz_face_minimal_refresh(millis());
}

void gz_face_minimal_refresh(uint32_t now_ms)
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

    char val[16];
    gz_face_field_text(gz_comp_subject(s_line_comp), val, sizeof val);
    char line[32];
    snprintf(line, sizeof line, "%s  %s", gz_face_comp_title(s_line_comp), val);
    lv_label_set_text(s_line, line);
}
