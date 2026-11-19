/*
 * gz_ui.cpp — LVGL 9 UI task and status screen for the GODZILLA node (P2a).
 *
 * Rendering: LVGL partial mode, two buffers of GZ_LCD_BUF_LINES full-width rows
 * in INTERNAL DMA RAM. (The P2a advice suggested PSRAM; the SPI master's DMA
 * from PSRAM isn't something we can rely on here, and the vendor demo also
 * keeps its buffers internal — 2 x 25.9 KB.) Invalidated areas are widened to
 * full-width, even-aligned rows: the vendor demo only ever pushes full-width
 * strips, so we keep the panel on that proven path.
 *
 * The flush callback byte-swaps RGB565 for the panel and queues the strip; the
 * SPI "color transfer done" ISR calls lv_display_flush_ready(). The backlight
 * comes on only after that first flush completes — no white flash at boot.
 *
 * Screens (P2b): the radar (gz_ui_radar.cpp) is home; a left or right swipe
 * flips between radar and status. Swipes come from LVGL's own gesture
 * detection on the touch pointer; a swipe never also counts as a tap.
 *
 * Look: "deepwater" palette. Original layout; no third-party artwork.
 */
#include "gz_ui.h"
#include "gz_display.h"
#include "gz_board.h"
#include "gz_stats.h"
#include "gz_st77916_rev.h"
#include "gz_touch.h"       /* sole ARDUINO user of the touch driver */
#include "gz_ui_radar.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

/* Deepwater palette */
#define GZ_COL_ABYSS   0x031627   /* background */
#define GZ_COL_FOAM    0xBDEBEB   /* primary text */
#define GZ_COL_TEAL    0x2EC4B6   /* accent / headings */
#define GZ_COL_DIM     0x5E8CA8   /* secondary text */

#define GZ_UI_TASK_STACK   6144
#define GZ_UI_TASK_PRIO    1          /* same as the Arduino loop: round-robin */
#define GZ_UI_TASK_CORE    1
#define GZ_UI_REFRESH_MS   250

static lv_display_t *s_disp = nullptr;
static lv_indev_t   *s_touch = nullptr;
static gz_touch_point_t s_touch_pt = {};   /* last report; LVGL task only */
static volatile bool s_first_flush_done = false;
static lv_obj_t *s_scr_radar = nullptr, *s_scr_status = nullptr;
static bool      s_gesture_seen = false;     /* suppress the tap that ends a swipe */

/* ---- display glue -------------------------------------------------------- */

static uint32_t gz_tick_ms(void) { return (uint32_t)millis(); }

static bool IRAM_ATTR on_flush_done(void *ctx)
{
    (void)ctx;
    if (s_disp) lv_display_flush_ready(s_disp);
    s_first_flush_done = true;
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)disp;
    uint32_t w = (uint32_t)lv_area_get_width(area);
    uint32_t h = (uint32_t)lv_area_get_height(area);
    lv_draw_sw_rgb565_swap(px_map, w * h);          /* panel wants big-endian */
    gz_display_draw(area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

/* Full-width rows, even y bounds — matches the vendor's full-width flushes. */
static void rounder_cb(lv_event_t *e)
{
    lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
    a->x1 = 0;
    a->x2 = GZ_LCD_WIDTH - 1;
    a->y1 &= ~1;
    a->y2 |= 1;
    if (a->y2 > GZ_LCD_HEIGHT - 1) a->y2 = GZ_LCD_HEIGHT - 1;
}

/* CST816S -> LVGL pointer. Keeps the last coordinates on release, as LVGL
 * expects; gz_touch_poll() only touches the bus after INT or while down. */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    gz_touch_poll(&s_touch_pt);
    data->point.x = (int32_t)s_touch_pt.x;
    data->point.y = (int32_t)s_touch_pt.y;
    data->state   = s_touch_pt.pressed ? LV_INDEV_STATE_PRESSED
                                       : LV_INDEV_STATE_RELEASED;
}

/* ---- status screen ------------------------------------------------------- */

static lv_obj_t *s_lbl_name, *s_lbl_up, *s_lbl_ble, *s_lbl_wifi,
                *s_lbl_age, *s_lbl_mem, *s_lbl_hw, *s_lbl_touch;

static lv_obj_t *add_label(lv_obj_t *parent, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

static void build_status_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(GZ_COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Column kept inside the round glass: inscribed radius 180 px, so a
     * ~250 px-wide column centred on the screen stays clear of the edge. */
    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(col, 250, LV_SIZE_CONTENT);
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 6, 0);

    s_lbl_name = add_label(col, &lv_font_montserrat_28, GZ_COL_TEAL);
    lv_obj_t *sub = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    lv_label_set_text(sub, "POSEIDON detection node");
    s_lbl_up   = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_ble  = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_wifi = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_age  = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_mem  = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    s_lbl_hw   = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    s_lbl_touch = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
}

static void refresh_status_screen(void)
{
    gz_status_t st;
    gz_stats_snapshot(&st);
    uint32_t now = millis();
    char up[24], age[16];
    gz_fmt_uptime(now, up, sizeof up);
    gz_fmt_pub_age(&st, now, age, sizeof age);

    lv_label_set_text(s_lbl_name, st.node_name[0] ? st.node_name : "GODZILLA");
    lv_label_set_text_fmt(s_lbl_up, "up %s | ch %u", up, (unsigned)st.link_channel);
    lv_label_set_text_fmt(s_lbl_ble, "BLE  %lu sent | %u last",
                          (unsigned long)st.ble_frames, (unsigned)st.ble_last_entries);
    lv_label_set_text_fmt(s_lbl_wifi, "Wi-Fi  %lu sent | %u last",
                          (unsigned long)st.wifi_frames, (unsigned)st.wifi_last_entries);
    lv_label_set_text_fmt(s_lbl_age, "last publish %s", age);
    lv_label_set_text_fmt(s_lbl_mem, "heap %u KB | PSRAM %lu KB",
                          (unsigned)(ESP.getFreeHeap() / 1024u),
                          (unsigned long)st.psram_kb);
    lv_label_set_text_fmt(s_lbl_hw, "I2C %u dev | panel rev %d",
                          (unsigned)st.i2c_found, gz_display_panel_rev());
    /* Touch bench aid: chip, last controller gesture, last point. */
    if (gz_touch_chip_id() == 0) {
        lv_label_set_text(s_lbl_touch, "touch: not found");
    } else {
        lv_label_set_text_fmt(s_lbl_touch, "%s | %s %u,%u",
                              gz_touch_chip_name(gz_touch_chip_id()),
                              gz_touch_gesture_name(gz_touch_last_gesture()),
                              (unsigned)s_touch_pt.x, (unsigned)s_touch_pt.y);
    }
}

/* ---- navigation ---------------------------------------------------------- */

static void screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        s_gesture_seen = false;
    } else if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
            s_gesture_seen = true;
            lv_obj_t *next = (lv_screen_active() == s_scr_radar) ? s_scr_status
                                                                 : s_scr_radar;
            lv_screen_load(next);      /* no slide animation: QSPI full-screen moves are slow */
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (s_gesture_seen || lv_screen_active() != s_scr_radar) return;
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        gz_ui_radar_tap(p.x, p.y, millis());
    }
}

static void wire_screen(lv_obj_t *scr)
{
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_CLICKED, nullptr);
}

/* ---- task ---------------------------------------------------------------- */

static void ui_task(void *arg)
{
    (void)arg;
    s_scr_radar  = lv_obj_create(nullptr);
    s_scr_status = lv_obj_create(nullptr);
    gz_ui_radar_build(s_scr_radar);
    build_status_screen(s_scr_status);
    wire_screen(s_scr_radar);
    wire_screen(s_scr_status);
    gz_ui_radar_refresh(millis());
    refresh_status_screen();
    lv_screen_load(s_scr_radar);        /* radar is home */

    bool     backlight_on = false;
    uint32_t last_refresh = 0;
    for (;;) {
        uint32_t wait = lv_timer_handler();

        if (!backlight_on && s_first_flush_done) {
            gz_display_backlight(GZ_LCD_BL_DEFAULT_PCT);
            backlight_on = true;
        }
        uint32_t now = millis();
        if ((uint32_t)(now - last_refresh) >= GZ_UI_REFRESH_MS) {
            last_refresh = now;
            /* Only the visible screen; the other catches up when shown. */
            if (lv_screen_active() == s_scr_radar) gz_ui_radar_refresh(now);
            else                                   refresh_status_screen();
        }
        if (wait < 5)  wait = 5;
        if (wait > 10) wait = 10;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

bool gz_ui_start(void)
{
    if (!gz_display_init(on_flush_done, nullptr)) {
        Serial.println("!! UI: display init failed — running headless");
        return false;
    }

    const size_t buf_bytes = (size_t)GZ_LCD_WIDTH * GZ_LCD_BUF_LINES * 2u;
    void *b1 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *b2 = heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!b1 || !b2) {
        Serial.printf("!! UI: draw buffers (2 x %u B internal DMA) failed — headless\n",
                      (unsigned)buf_bytes);
        heap_caps_free(b1);
        heap_caps_free(b2);
        return false;
    }

    lv_init();
    lv_tick_set_cb(gz_tick_ms);
    s_disp = lv_display_create(GZ_LCD_WIDTH, GZ_LCD_HEIGHT);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_display_set_buffers(s_disp, b1, b2, (uint32_t)buf_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_add_event_cb(s_disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, nullptr);

    /* Touch on the shared I2C bus (Wire already up from the boot scan). The
     * indev is registered even if the chip didn't answer: it may just be
     * asleep, and a touch wakes it and fires INT. */
    gz_touch_begin();
    s_touch = lv_indev_create();
    lv_indev_set_type(s_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch, touch_read_cb);
    lv_indev_set_display(s_touch, s_disp);

    BaseType_t ok = xTaskCreatePinnedToCore(ui_task, "gz_ui", GZ_UI_TASK_STACK,
                                            nullptr, GZ_UI_TASK_PRIO, nullptr,
                                            GZ_UI_TASK_CORE);
    if (ok != pdPASS) {
        Serial.println("!! UI: task create failed — headless");
        return false;
    }
    Serial.printf("  UI: LVGL %d.%d.%d, 2 x %u B draw buffers, radar + status screens\n",
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                  (unsigned)buf_bytes);
    return true;
}
