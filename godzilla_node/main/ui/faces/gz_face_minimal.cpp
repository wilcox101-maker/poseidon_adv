/*
 * gz_face_minimal.cpp — Minimal face render (watch-faces orders, unit 5).
 * See gz_face_minimal.h. Pure layout + binding: gz_slotface.h (native-tested);
 * the status line via the shared slot renderer gz_slot_render.*.
 *
 * Draws a big digital time with one status line beneath it. The status line's
 * complication is the first the board can source among the persisted slots, or
 * node status otherwise — always honest, never faked. The drawing idioms mirror
 * gz_face_command.cpp. [render] — Claudius re-renders.
 */
#include "gz_face_minimal.h"
#include "gz_face_caps.h"       /* gz_faces.h + GZ_FACE_BOARD_CAPS (this board) */
#include "gz_face_store.h"      /* persisted settings, sanitised for this board */
#include "gz_slot_render.h"     /* shared slot title / value renderer           */
#include "../../gz_slotface.h"  /* minimal layout + status comp (native-tested)  */
#include "../../gz_geom.h"      /* the one geometry source (#88 §3.2)            */
#include "../../gz_clock.h"     /* current time, shared clock (false w/o RTC)    */

#include <Arduino.h>
#include <stdio.h>

/* Deepwater palette (shared with gz_face_command.cpp). */
#define COL_ABYSS   0x031627
#define COL_TEAL    0x2EC4B6
#define COL_FOAM    0xBDEBEB
#define COL_DIM     0x5E8CA8

#define MGN   GZ_UI_S(8)

static lv_obj_t      *s_time;    /* big digital time              */
static lv_obj_t      *s_status;  /* one status line, "TITLE value" */
static gz_subj_field_t s_field;  /* field the status line reads   */
static gz_comp_id_t    s_comp;   /* its complication              */

/* ---- helpers (mirror gz_face_command.cpp) -------------------------------- */
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

static void time_text(uint32_t now_ms, char *out, size_t n)
{
    gz_datetime_t t;
    if (gz_clock_now(now_ms, &t)) snprintf(out, n, "%02d:%02d", t.hour, t.minute);
    else                          snprintf(out, n, "--:--");
}

/* "TITLE value" for the status line, e.g. "CONTACT 7" or "NODE 128k". */
static void status_text(char *out, size_t n)
{
    char v[16];
    gz_slot_field_text(s_field, v, sizeof v);
    snprintf(out, n, "%s %s", gz_slot_comp_title(s_comp), v);
}

/* ---- build --------------------------------------------------------------- */
void gz_face_minimal_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    gz_ui_settings_t s;
    gz_face_store_load(&s);     /* already sanitised for this board */
    s_comp  = gz_min_status_comp(&s, GZ_FACE_BOARD_CAPS);
    s_field = gz_comp_subject(s_comp);

    int tx, ty, tw, th, sx, sy, sw, sh;
    gz_min_layout(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, MGN,
                  &tx, &ty, &tw, &th, &sx, &sy, &sw, &sh);

    char tb[8];
    time_text(millis(), tb, sizeof tb);
    s_time = label_at(scr, &lv_font_montserrat_28, COL_FOAM,
                      tx + tw / 2, ty + th / 2, tb);

    char sb[32];
    status_text(sb, sizeof sb);
    s_status = label_at(scr, &lv_font_montserrat_16, COL_TEAL,
                        sx + sw / 2, sy + sh / 2, sb);

    gz_face_minimal_refresh(millis());
}

/* ---- refresh ------------------------------------------------------------- */
void gz_face_minimal_refresh(uint32_t now_ms)
{
    char tb[8];
    time_text(now_ms, tb, sizeof tb);
    lv_label_set_text(s_time, tb);

    char sb[32];
    status_text(sb, sizeof sb);
    lv_label_set_text(s_status, sb);
}
