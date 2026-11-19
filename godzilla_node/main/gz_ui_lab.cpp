/*
 * gz_ui_lab.cpp — BLE Lab screen (#98 §2.4b).
 *
 * List view: up to gz_lab_rows_fit() rows from the recent-advert table
 * (gz_ble_lab_store), strongest first, re-sorted once a second so a row does
 * not move while you aim at it. Devices that announce HID show in teal. Tap a
 * row: the detail view decodes that device's last advert field by field
 * (gz_ble_lab_view.h). Tap anywhere on the detail to go back. If the device
 * drops out of the table the detail says so and keeps the address.
 *
 * Passive only: nothing here connects, pairs or sends. Layout and strings are
 * the pure, native-tested gz_ble_lab_view.h; this file only places them.
 * Deepwater palette, original layout.
 */
#include "gz_ui_lab.h"
#include "gz_ble_lab_store.h"
#include "gz_ble_lab_view.h"
#include "gz_board.h"
#include "gz_geom.h"   /* the one geometry source (#88 §3.2) */

#include <stdio.h>
#include <string.h>

/* Deepwater palette (shared values with gz_ui.cpp / gz_ui_hunt.cpp). */
#define COL_ABYSS    0x031627
#define COL_TEAL     0x2EC4B6
#define COL_FOAM     0xBDEBEB
#define COL_DIM      0x5E8CA8

#define LABEL_W      gz_geom_col_w(260, GZ_UI_W)   /* 260 on the 1.85B */
#define LIST_RESORT_MS 1000u

static lv_obj_t *s_title, *s_hint, *s_empty, *s_detail;
static lv_obj_t *s_row[GZ_LAB_VIEW_MAX];
static int       s_shown_max;                  /* rows this panel fits */

static gz_lab_row_t s_rows[GZ_LAB_MAX];         /* last snapshot, sorted */
static int          s_n;
static uint32_t     s_last_sort;
static bool         s_sorted_once;

static bool     s_detail_on;
static uint8_t  s_sel[6];                      /* the device being shown */

static lv_obj_t *label(lv_obj_t *scr, const lv_font_t *font, uint32_t rgb, int y)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, LABEL_W);
    lv_label_set_text(l, "");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static void set_hidden(lv_obj_t *o, bool hidden)
{
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

/* ---- build --------------------------------------------------------------- */

void gz_ui_lab_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_title = label(scr, &lv_font_montserrat_16, COL_TEAL, GZ_LAB_TITLE_Y + GZ_UI_DY);
    lv_label_set_text(s_title, "BLE LAB");

    s_shown_max = gz_lab_rows_fit(GZ_UI_H, GZ_UI_ROUND);
    const int top = gz_lab_list_top(GZ_UI_H);
    for (int i = 0; i < GZ_LAB_VIEW_MAX; ++i) {
        s_row[i] = label(scr, &lv_font_montserrat_14, COL_FOAM, top + i * GZ_LAB_ROW_H);
        lv_obj_set_style_text_align(s_row[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(s_row[i], LV_LABEL_LONG_DOT);   /* one line, "..." */
        lv_obj_set_height(s_row[i], GZ_LAB_ROW_H - 4);
        set_hidden(s_row[i], true);
    }

    s_detail = label(scr, &lv_font_montserrat_14, COL_FOAM, top);
    lv_label_set_long_mode(s_detail, LV_LABEL_LONG_WRAP);
    set_hidden(s_detail, true);

    s_hint = label(scr, &lv_font_montserrat_14, COL_DIM, gz_lab_hint_y(GZ_UI_H, GZ_UI_ROUND));

    s_empty = label(scr, &lv_font_montserrat_16, COL_DIM, 0);
    lv_label_set_text(s_empty, "BLE LAB\n\nlistening...\n\nno adverts heard yet");
    lv_obj_align(s_empty, LV_ALIGN_CENTER, 0, 0);

    s_n = 0;
    s_sorted_once = false;
    s_detail_on = false;
}

/* ---- refresh ------------------------------------------------------------- */

static void take_snapshot(uint32_t now)
{
    s_n = gz_ble_lab_snapshot(s_rows, GZ_LAB_MAX);
    gz_lab_sort_by_rssi(s_rows, s_n);
    s_last_sort = now;
    s_sorted_once = true;
}

static void draw_list(void)
{
    const int shown = s_n < s_shown_max ? s_n : s_shown_max;
    set_hidden(s_detail, true);
    set_hidden(s_empty, s_n != 0);
    set_hidden(s_title, s_n == 0);
    set_hidden(s_hint, s_n == 0);
    char buf[112];
    for (int i = 0; i < GZ_LAB_VIEW_MAX; ++i) {
        if (i >= shown) { set_hidden(s_row[i], true); continue; }
        bool hid = gz_lab_row_text(&s_rows[i], buf, sizeof buf);
        lv_obj_set_style_text_color(s_row[i], lv_color_hex(hid ? COL_TEAL : COL_FOAM), 0);
        lv_label_set_text(s_row[i], buf);
        set_hidden(s_row[i], false);
    }
    if (s_n > shown) lv_label_set_text_fmt(s_hint, "%d of %d | tap a row", shown, s_n);
    else             lv_label_set_text(s_hint, "tap a row for detail");
}

static void draw_detail(uint32_t now)
{
    for (int i = 0; i < GZ_LAB_VIEW_MAX; ++i) set_hidden(s_row[i], true);
    set_hidden(s_empty, true);
    set_hidden(s_title, false);
    set_hidden(s_hint, false);
    set_hidden(s_detail, false);
    int i = gz_lab_find(s_rows, s_n, s_sel);
    if (i < 0) {
        char addr[18];
        gz_lab_fmt_addr(s_sel, addr, sizeof addr);
        lv_label_set_text_fmt(s_detail, "%s\n\nno longer heard", addr);
    } else {
        static char text[512];
        gz_lab_detail_text(&s_rows[i], now, text, sizeof text);
        lv_label_set_text(s_detail, text);
    }
    lv_label_set_text(s_hint, "tap to go back");
}

void gz_ui_lab_refresh(uint32_t now)
{
    /* The detail follows its device every tick; the list re-sorts at 1 Hz. */
    if (s_detail_on || !s_sorted_once || (uint32_t)(now - s_last_sort) >= LIST_RESORT_MS)
        take_snapshot(now);
    if (s_detail_on) draw_detail(now);
    else             draw_list();
}

void gz_ui_lab_tap(int y, uint32_t now)
{
    if (s_detail_on) {
        s_detail_on = false;
        take_snapshot(now);
        draw_list();
        return;
    }
    const int shown = s_n < s_shown_max ? s_n : s_shown_max;
    int i = gz_lab_row_hit(y, GZ_UI_H, shown);
    if (i < 0) return;
    memcpy(s_sel, s_rows[i].addr, 6);
    s_detail_on = true;
    draw_detail(now);
}

void gz_ui_lab_hide(void)
{
    s_detail_on = false;
}
