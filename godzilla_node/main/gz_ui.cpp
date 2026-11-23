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
 * Screens: a swipe ring, radar -> hunt -> BLE lab -> watch -> status (left = next,
 * right = previous). The home screen is GZ_UI_HOME. Swipes come from LVGL's own
 * gesture detection on the touch pointer; a swipe or a long-press never also
 * counts as a tap. Only the visible screen refreshes.
 *
 * Look: "deepwater" palette. Original layout; no third-party artwork.
 */
#include "gz_ui.h"
#include "gz_display.h"
#include "gz_board.h"
#include "gz_stats.h"
#include "gz_st77916_rev.h"
#include "gz_touch.h"       /* sole ARDUINO user of the touch driver */
#include "gz_imu.h"         /* QMI8658 status line (boards with an IMU) */
#include "gz_power.h"       /* battery / supply line (boards with a readout) */
#include "gz_wake.h"        /* idle dim / wake on touch or motion (#88 §3.7) */
#include "gz_rotate.h"      /* auto-rotate from the IMU (#98 §1.3) */
#include "gz_ui_radar.h"
#include "gz_ui_hunt.h"
#include "gz_ui_lab.h"       /* BLE Lab, passive (#98 §2.4) */
#include "gz_ui_watch.h"
#include "gz_ui_alert.h"     /* §4 item 6: priority ALERT overlay (top layer) */
#include "ui/faces/gz_face_store.h" /* persisted face settings (finish, alert mask) */
#include "gz_ui_apply.h"     /* which live consumers a settings save touches (item I) */
#include "gz_link.h"         /* CMD_TIMESET latch drained on this (RTC) task */
#include "gz_geom.h"         /* the one geometry source (#88 §3.2) */
#include "gz_safearea.h"     /* item J self-test: shape-aware safe box */
#include "gz_rtc.h"          /* item J self-test: read-only RTC read */
#include "gz_recon_task.h"   /* item J self-test: SD mount state (read-only) */
#include "gz_wifi_scan.h"    /* item J self-test: captured-frame counter */
#include "gz_ble_scan.h"     /* item J self-test: captured-advert counter */
#include "gz_selftest.h"     /* item J: pure verdict / rate / serial model */

#include <Arduino.h>
#include <Wire.h>            /* item J self-test: read-only I2C presence probe */
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <stdio.h>

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

/* Auto-rotate (gz_rotate.h): only on boards with an IMU, behind one constant.
 * LVGL rotates touch points itself; the flush below rotates the pixels into
 * s_rot_buf (one is enough: LVGL waits for flush_ready before the next flush). */
#define GZ_UI_ROTATE (GZ_BOARD_HAS_IMU && GZ_UI_AUTO_ROTATE)
#if GZ_UI_ROTATE
static uint8_t *s_rot_buf = nullptr;     /* nullptr: rotation unavailable */
static uint8_t  s_rot_cur = 0;           /* applied quarter-turn */
static uint8_t  s_rot_imu = 0;           /* IMU orientation (hysteresis state) */
#endif

/* Idle dim / wake (gz_wake.h): every board. Touched by the touch read and
 * the motion sample, applied in the task loop; all on this (LVGL) task. */
static gz_wake_t s_wake;
/* Swipe ring order. GZ_UI_HOME is the screen shown at boot — one line to
 * change after the bench. */
#if GZ_BOARD_HAS_RTC
enum { GZ_SCR_RADAR = 0, GZ_SCR_HUNT = 1, GZ_SCR_LAB = 2, GZ_SCR_WATCH = 3,
       GZ_SCR_STATUS = 4, GZ_SCR_COUNT = 5 };
#else
/* No RTC (ES3C28P): the watch runs only where an RTC exists (#88 §4), so the
 * ring is radar -> hunt -> BLE lab -> status and GZ_SCR_WATCH never matches
 * a screen. */
enum { GZ_SCR_RADAR = 0, GZ_SCR_HUNT = 1, GZ_SCR_LAB = 2, GZ_SCR_STATUS = 3,
       GZ_SCR_COUNT = 4, GZ_SCR_WATCH = -1 };
#endif
#define GZ_UI_HOME GZ_SCR_RADAR

static lv_obj_t *s_scr[GZ_SCR_COUNT];
static int       s_cur = GZ_UI_HOME;
static bool      s_suppress_click = false;   /* the tap that ends a swipe / long-press */

/* Persisted alert-overlay settings. Read once at boot, then kept live by
 * gz_ui_apply_settings when the customiser saves (item I). */
static uint8_t   s_alert_mask = GZ_FACE_ALERT_MASK_ALL;
static bool      s_alert_always_on = false;

/* The settings currently applied to the live consumers, so a customiser save
 * can diff against them (gz_ui_apply.h) and re-apply only what changed. */
static gz_ui_settings_t s_face_applied;

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
    uint32_t w = (uint32_t)lv_area_get_width(area);
    uint32_t h = (uint32_t)lv_area_get_height(area);
    lv_draw_sw_rgb565_swap(px_map, w * h);          /* panel wants big-endian */
#if GZ_UI_ROTATE
    lv_display_rotation_t rot = lv_display_get_rotation(disp);
    if (rot != LV_DISPLAY_ROTATION_0 && s_rot_buf) {
        /* Logical strip -> physical: rotate the area and the pixels. */
        lv_area_t pa = *area;
        lv_display_rotate_area(disp, &pa);
        uint32_t dst_stride = (uint32_t)lv_area_get_width(&pa) * 2u;
        lv_draw_sw_rotate(px_map, s_rot_buf, (int32_t)w, (int32_t)h, (int32_t)(w * 2u),
                          (int32_t)dst_stride, rot, LV_COLOR_FORMAT_RGB565);
        gz_display_draw(pa.x1, pa.y1, pa.x2 + 1, pa.y2 + 1, s_rot_buf);
        return;
    }
#else
    (void)disp;
#endif
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
    if (s_touch_pt.pressed) gz_wake_activity(&s_wake, (uint32_t)millis());
    data->point.x = (int32_t)s_touch_pt.x;
    data->point.y = (int32_t)s_touch_pt.y;
    data->state   = s_touch_pt.pressed ? LV_INDEV_STATE_PRESSED
                                       : LV_INDEV_STATE_RELEASED;
}

/* ---- status screen ------------------------------------------------------- */

static lv_obj_t *s_lbl_name, *s_lbl_up, *s_lbl_ble, *s_lbl_wifi,
                *s_lbl_age, *s_lbl_mem, *s_lbl_hw, *s_lbl_touch, *s_lbl_sub;
#if GZ_BOARD_POWER
static lv_obj_t *s_lbl_pwr;
#endif
#if GZ_BOARD_HAS_IMU
static lv_obj_t *s_lbl_imu;
static uint8_t   s_imu_rot = 0;   /* last orientation, feeds the hysteresis */
#endif

/* ---- bench self-test page (item J) --------------------------------------- */
/* A second screen, reached by a tap on the Status screen and dismissed by a tap.
 * Shows, per board capability flag, whether each on-board part answered and one
 * live reading where fitted. Read-only: nothing here writes a device, NVS, RTC
 * or SD, and nothing new transmits. */
static lv_obj_t *s_scr_selftest = nullptr;
static lv_obj_t *s_st_list = nullptr;
static lv_obj_t *s_st_name[GZ_ST_LINE_COUNT];
static lv_obj_t *s_st_val[GZ_ST_LINE_COUNT];
static bool      s_in_selftest = false;
static bool      s_touch_seen  = false;   /* at least one tap registered */
/* Capture-rate baseline: previous frame/advert counts and when they were read,
 * so a refresh shows frames/s over the elapsed window. */
static uint32_t  s_st_prev_wifi = 0, s_st_prev_ble = 0, s_st_prev_ms = 0;
#define GZ_COL_WARN 0xE86A5C   /* self-test FAIL rows */

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
    lv_obj_set_size(col, gz_geom_col_w(250, GZ_UI_W), LV_SIZE_CONTENT);   /* 250 on the 1.85B */
    lv_obj_center(col);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 6, 0);

    s_lbl_name = add_label(col, &lv_font_montserrat_28, GZ_COL_TEAL);
    lv_obj_t *sub = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    s_lbl_sub = sub;
    s_lbl_up   = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_ble  = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_wifi = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_age  = add_label(col, &lv_font_montserrat_16, GZ_COL_FOAM);
    s_lbl_mem  = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    s_lbl_hw   = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
    s_lbl_touch = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
#if GZ_BOARD_HAS_IMU
    s_lbl_imu   = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
#endif
#if GZ_BOARD_POWER
    s_lbl_pwr   = add_label(col, &lv_font_montserrat_14, GZ_COL_DIM);
#endif
    /* Item J: a tap on this screen opens the bench self-test page. */
    lv_obj_t *hint = add_label(col, &lv_font_montserrat_14, GZ_COL_TEAL);
    lv_label_set_text(hint, "tap: self-test");
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
#if GZ_BOARD_HAS_RTC
    gz_datetime_t clk;
    if (gz_ui_watch_time(now, &clk))
        lv_label_set_text_fmt(s_lbl_sub, "POSEIDON node | %02u:%02u:%02u",
                              (unsigned)clk.hour, (unsigned)clk.minute,
                              (unsigned)clk.second);
    else
#endif
        lv_label_set_text(s_lbl_sub, "POSEIDON detection node");
    lv_label_set_text_fmt(s_lbl_up, "up %s | ch %u", up, (unsigned)st.link_channel);
    lv_label_set_text_fmt(s_lbl_ble, "BLE  %lu sent | %u last",
                          (unsigned long)st.ble_frames, (unsigned)st.ble_last_entries);
    lv_label_set_text_fmt(s_lbl_wifi, "Wi-Fi  %lu sent | %u last",
                          (unsigned long)st.wifi_frames, (unsigned)st.wifi_last_entries);
    lv_label_set_text_fmt(s_lbl_age, "last publish %s", age);
    lv_label_set_text_fmt(s_lbl_mem, "heap %u KB | PSRAM %lu KB",
                          (unsigned)(ESP.getFreeHeap() / 1024u),
                          (unsigned long)st.psram_kb);
#if GZ_BOARD_DISPLAY == 1
    lv_label_set_text_fmt(s_lbl_hw, "I2C %u dev | panel rev %d",
                          (unsigned)st.i2c_found, gz_display_panel_rev());
#else
    lv_label_set_text_fmt(s_lbl_hw, "I2C %u dev | %s",
                          (unsigned)st.i2c_found,
                          GZ_BOARD_DISPLAY == 2 ? "CO5300 AMOLED" : "ILI9341V");
#endif
    /* Touch bench aid: chip, last controller gesture, last point. */
    if (!gz_touch_found()) {
        lv_label_set_text(s_lbl_touch, "touch: not found");
    } else {
        lv_label_set_text_fmt(s_lbl_touch, "%s | %s %u,%u",
                              gz_touch_name(),
                              gz_touch_gesture_name(gz_touch_last_gesture()),
                              (unsigned)s_touch_pt.x, (unsigned)s_touch_pt.y);
    }
#if GZ_BOARD_HAS_IMU
    /* IMU bench aid: still / moving and the orientation quarter-turn. One live
     * read on this (LVGL) task; Wire is mutexed in arduino-esp32. */
    if (gz_imu_chip_id() != GZ_IMU_WHOAMI_VALUE) {
        lv_label_set_text(s_lbl_imu, "IMU: not found");
    } else {
        gz_imu_sample_t im;
        if (gz_imu_read(&im, s_imu_rot)) {
            s_imu_rot = im.rotation;
            lv_label_set_text_fmt(s_lbl_imu, "IMU %s | rot %u",
                                  im.moving ? "moving" : "still", (unsigned)im.rotation);
        } else {
            lv_label_set_text(s_lbl_imu, "IMU: read error");
        }
    }
#endif
#if GZ_BOARD_POWER
    /* Power: only what the hardware measured (gz_power.h). */
    gz_power_t pw;
    if (!gz_power_read(&pw)) {
        lv_label_set_text(s_lbl_pwr, "power: no answer");
    } else {
        char v[12] = "", pc[8] = "";
        if (pw.mv_valid)  snprintf(v, sizeof v, " %u.%02u V", (unsigned)(pw.mv / 1000u),
                                   (unsigned)((pw.mv % 1000u) / 10u));
        if (pw.pct_valid) snprintf(pc, sizeof pc, " %u%%", (unsigned)pw.pct);
#if GZ_BOARD_POWER == 1
        if (!pw.batt)
            lv_label_set_text(s_lbl_pwr, pw.vbus ? "USB power, no battery" : "power: no battery");
        else
            lv_label_set_text_fmt(s_lbl_pwr, "batt%s%s%s%s", pc, v,
                                  (pw.mv_valid || pw.pct_valid) ? " | " : " | gauge off | ",
                                  gz_power_chg_name(pw.chg));
#else
        if (pw.mv_valid) lv_label_set_text_fmt(s_lbl_pwr, "batt%s", v);
        else             lv_label_set_text(s_lbl_pwr, "batt: not measured");
#endif
    }
#endif
}

/* ---- bench self-test page (item J) --------------------------------------- */

/* Gather the live self-test snapshot from the subsystems. All reads: an I2C
 * presence poll (address only, no register write) for the gauge / mic, a time
 * read for the RTC, one IMU sample, the SD mount flag, the capture counters and
 * the publish stats. Runs on the UI task, like the status-screen reads. */
static void gz_st_gather(gz_selftest_t *s)
{
    memset(s, 0, sizeof *s);
    uint32_t now = millis();

    s->disp_ok = s_first_flush_done;         /* panel completed a flush */

    s->touch_found = gz_touch_found();
    s->touch_seen  = s_touch_seen;
    s->touch_x = s_touch_pt.x;
    s->touch_y = s_touch_pt.y;

#if GZ_BOARD_HAS_RTC
    s->cap_rtc = true;
    gz_datetime_t dt;
    if (gz_rtc_read(&dt)) { s->rtc_ok = true; s->rtc_osc_stopped = dt.osc_stopped; }
#endif

#if GZ_BOARD_HAS_IMU
    s->cap_imu = true;
    if (gz_imu_chip_id() == GZ_IMU_WHOAMI_VALUE) {
        gz_imu_sample_t im;
        if (gz_imu_read(&im, s_imu_rot)) { s->imu_ok = true; s->imu_moving = im.moving; }
    }
#endif

#if GZ_BOARD_HAS_GAUGE
    s->cap_gauge = true;
    Wire.beginTransmission(GZ_I2C_ADDR_GAUGE);
    s->gauge_ok = (Wire.endTransmission() == 0);
#if GZ_BOARD_POWER
    { gz_power_t pw; if (gz_power_read(&pw) && pw.mv_valid) s->gauge_mv = pw.mv; }
#endif
#endif

#if GZ_BOARD_HAS_MIC_ADC
    s->cap_mic = true;
    Wire.beginTransmission(GZ_I2C_ADDR_MIC_ADC);
    s->mic_ok = (Wire.endTransmission() == 0);
#endif

#if GZ_BOARD_HAS_SD
    s->cap_sd = true;
    s->sd_mounted = gz_recon_sd_mounted();
#endif

    /* Radios: frames/adverts per second over the window since the last gather. */
    uint32_t wf = gz_wifi_scan_frames();
    uint32_t ba = gz_ble_scan_adverts();
    s->wifi_any   = (wf > 0u);
    s->ble_any    = (ba > 0u);
    s->wifi_delta = wf - s_st_prev_wifi;
    s->ble_delta  = ba - s_st_prev_ble;
    s->rate_window_ms = (uint32_t)(now - s_st_prev_ms);
    s_st_prev_wifi = wf;
    s_st_prev_ble  = ba;
    s_st_prev_ms   = now;

    /* ESP-NOW link: brought up at boot; liveness is a real detection publish. */
    gz_status_t stt;
    gz_stats_snapshot(&stt);
    s->link_up       = true;
    s->has_published = stt.has_published;
    s->pub_age_ms    = stt.has_published ? (uint32_t)(now - stt.last_pub_ms) : 0u;
}

static lv_obj_t *st_plain(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *st_label(lv_obj_t *parent, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, "");
    return l;
}

static void build_selftest_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(GZ_COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* One geometry source (gz_geom.h via gz_safearea.h): the safe box keeps the
     * list off a round panel's edge; title and footer sit on their own lines
     * INSIDE the box; the rows scroll in the band between them, by real font
     * line height so nothing shrinks (#150, #152). */
    gz_rect_t box = gz_safe_box(GZ_UI_W, GZ_UI_H, GZ_UI_ROUND, GZ_UI_S(6));
    int rowh = lv_font_get_line_height(&lv_font_montserrat_14) + GZ_UI_S(6);
    int th   = lv_font_get_line_height(&lv_font_montserrat_14) + GZ_UI_S(4);
    int fh   = lv_font_get_line_height(&lv_font_montserrat_12) + GZ_UI_S(4);

    gz_rect_t band = box;
    band.y += th;
    band.h -= th + fh;
    /* Fit all lines in the band (the list cannot be drag-scrolled: its parents
     * are not clickable). Shrink the GAP, never below the font line height —
     * the round 1.85B lost the last line by 5 px at the full gap (render
     * review). */
    int fit = band.h / GZ_ST_LINE_COUNT;
    int minh = lv_font_get_line_height(&lv_font_montserrat_14);
    if (rowh > fit) rowh = (fit > minh) ? fit : minh;

    s_st_list = st_plain(scr);
    lv_obj_set_pos(s_st_list, band.x, band.y);
    lv_obj_set_size(s_st_list, band.w, band.h);
    lv_obj_set_scroll_dir(s_st_list, LV_DIR_VER);
    lv_obj_add_flag(s_st_list, LV_OBJ_FLAG_SCROLLABLE);

    int valx = box.w / 2;
    for (int r = 0; r < GZ_ST_LINE_COUNT; ++r) {
        int ry = r * rowh;
        int ty = ry + (rowh - lv_font_get_line_height(&lv_font_montserrat_14)) / 2;
        lv_obj_t *nm = st_label(s_st_list, &lv_font_montserrat_14, GZ_COL_FOAM);
        lv_label_set_text(nm, gz_selftest_label(r));
        lv_obj_set_pos(nm, GZ_UI_S(4), ty);
        s_st_name[r] = nm;
        lv_obj_t *v = st_label(s_st_list, &lv_font_montserrat_14, GZ_COL_FOAM);
        lv_obj_set_pos(v, valx, ty);
        s_st_val[r] = v;
    }

    lv_obj_t *t = st_label(scr, &lv_font_montserrat_14, GZ_COL_TEAL);
    lv_label_set_text(t, "SELF-TEST");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, box.y);

    lv_obj_t *f = st_label(scr, &lv_font_montserrat_12, GZ_COL_DIM);
    lv_label_set_text(f, "tap to return");
    lv_obj_align(f, LV_ALIGN_TOP_MID, 0, band.y + band.h + GZ_UI_S(2));
}

static void refresh_selftest_screen(void)
{
    gz_selftest_t st;
    gz_st_gather(&st);
    char val[24];
    for (int r = 0; r < GZ_ST_LINE_COUNT; ++r) {
        gz_st_result_t res = gz_selftest_fmt_value(&st, r, val, sizeof val);
        lv_label_set_text(s_st_val[r], val);
        uint32_t vc = (res == GZ_ST_PASS) ? GZ_COL_FOAM
                    : (res == GZ_ST_FAIL) ? GZ_COL_WARN : GZ_COL_DIM;
        lv_obj_set_style_text_color(s_st_val[r], lv_color_hex(vc), 0);
        lv_obj_set_style_text_color(s_st_name[r],
            lv_color_hex(res == GZ_ST_SKIP ? GZ_COL_DIM : GZ_COL_FOAM), 0);
    }
}

static void selftest_open(void)
{
    if (!s_scr_selftest) return;
    s_in_selftest  = true;
    s_st_prev_wifi = gz_wifi_scan_frames();     /* reset the rate window */
    s_st_prev_ble  = gz_ble_scan_adverts();
    s_st_prev_ms   = millis();
    refresh_selftest_screen();
    lv_screen_load(s_scr_selftest);
}

static void selftest_close(void)
{
    s_in_selftest = false;
    refresh_status_screen();
    lv_screen_load(s_scr[GZ_SCR_STATUS]);
}

/* Print the one-line self-test summary to serial. Called once at boot from
 * main.cpp behind GZ_SELFTEST_SERIAL (default off). */
void gz_ui_selftest_serial_emit(void)
{
    gz_selftest_t st;
    gz_st_gather(&st);
    char line[128];
    gz_selftest_serial(&st, line, sizeof line);
    Serial.println(line);
}

/* ---- navigation ---------------------------------------------------------- */

static void refresh_current(uint32_t now)
{
    if (s_cur == GZ_SCR_RADAR)      gz_ui_radar_refresh(now);
    else if (s_cur == GZ_SCR_HUNT)  gz_ui_hunt_refresh(now);
    else if (s_cur == GZ_SCR_LAB)   gz_ui_lab_refresh(now);
#if GZ_BOARD_HAS_RTC
    else if (s_cur == GZ_SCR_WATCH) gz_ui_watch_refresh(now);
#endif
    else                            refresh_status_screen();
}

static void go_to(int idx)
{
    uint32_t now = millis();
    if (s_cur == GZ_SCR_LAB) gz_ui_lab_hide();         /* reopens on the list */
#if GZ_BOARD_HAS_RTC
    if (s_cur == GZ_SCR_WATCH) gz_ui_watch_hide();     /* drops an unfinished set */
    s_cur = idx;
    if (s_cur == GZ_SCR_WATCH) gz_ui_watch_show(now);  /* resync before the first frame */
    else                       refresh_current(now);
#else
    s_cur = idx;
    refresh_current(now);
#endif
    lv_screen_load(s_scr[s_cur]);   /* no slide animation: QSPI full-screen moves are slow */
}

static void screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    /* Self-test page (item J): a clean tap returns to Status; a swipe only
     * scrolls the row band (and marks the release as not a tap). */
    if (s_in_selftest) {
        if (code == LV_EVENT_PRESSED)       s_suppress_click = false;
        else if (code == LV_EVENT_GESTURE)  s_suppress_click = true;
        else if (code == LV_EVENT_CLICKED) {
            if (s_suppress_click) return;
            s_touch_seen = true;
            if (gz_ui_alert_tap()) return;     /* an alert overlay eats the tap first */
            selftest_close();
        }
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        s_suppress_click = false;
    } else if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
            s_suppress_click = true;
            int step = (dir == LV_DIR_LEFT) ? 1 : GZ_SCR_COUNT - 1;
            go_to((s_cur + step) % GZ_SCR_COUNT);
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (s_cur == GZ_SCR_STATUS || s_cur == GZ_SCR_LAB || s_suppress_click) return;
        s_suppress_click = true;       /* the release must not also count as a tap */
#if GZ_BOARD_HAS_RTC
        if (s_cur == GZ_SCR_WATCH)     gz_ui_watch_long_press(millis());
        else
#endif
        if (s_cur == GZ_SCR_HUNT)      gz_ui_hunt_long_press();      /* stop hunting */
        else if (gz_ui_radar_long_press()) go_to(GZ_SCR_HUNT);       /* card: hunt it */
    } else if (code == LV_EVENT_CLICKED) {
        if (s_suppress_click) return;
        s_touch_seen = true;               /* item J self-test: a tap has been seen */
        /* A tap dismisses the priority alert overlay first, and never also
         * reaches the screen beneath it. */
        if (gz_ui_alert_tap()) return;
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (s_cur == GZ_SCR_RADAR)      gz_ui_radar_tap(p.x, p.y, millis());
        else if (s_cur == GZ_SCR_LAB)   gz_ui_lab_tap(p.y, millis());
#if GZ_BOARD_HAS_RTC
        else if (s_cur == GZ_SCR_WATCH) gz_ui_watch_tap(p.y, millis());
#endif
        else if (s_cur == GZ_SCR_STATUS) selftest_open();   /* item J: open self-test */
    }
}

static void wire_screen(lv_obj_t *scr)
{
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_add_event_cb(scr, screen_event_cb, LV_EVENT_CLICKED, nullptr);
}

/* ---- task ---------------------------------------------------------------- */

static void ui_task(void *arg)
{
    (void)arg;
    for (int i = 0; i < GZ_SCR_COUNT; ++i) {
        s_scr[i] = lv_obj_create(nullptr);
        wire_screen(s_scr[i]);
    }
    gz_ui_radar_build(s_scr[GZ_SCR_RADAR]);
    gz_ui_hunt_build(s_scr[GZ_SCR_HUNT]);
    gz_ui_lab_build(s_scr[GZ_SCR_LAB]);
    {   /* Persisted face settings: the case finish (RTC boards) and the alert
         * overlay's per-family mask + always-on, all read once here. */
        gz_ui_settings_t fs;
        gz_face_store_load(&fs);                /* defaults + sanitised for this board */
        s_alert_mask      = fs.alert_mask;
        s_alert_always_on = fs.always_on != 0;
#if GZ_BOARD_HAS_RTC
        gz_ui_watch_set_finish(fs.finish);
#endif
        s_face_applied = fs;                    /* baseline for live re-apply (item I) */
    }
#if GZ_BOARD_HAS_RTC
    gz_ui_watch_build(s_scr[GZ_SCR_WATCH]);     /* first RTC read, on this task */
#endif
    build_status_screen(s_scr[GZ_SCR_STATUS]);
    /* Item J: the bench self-test page — a standalone screen (not in the swipe
     * ring), opened by a tap on Status and dismissed by a tap. */
    s_scr_selftest = lv_obj_create(nullptr);
    wire_screen(s_scr_selftest);
    build_selftest_screen(s_scr_selftest);
    gz_ui_alert_build();                        /* §4 item 6: overlay on the top layer */
    refresh_current(millis());
    lv_screen_load(s_scr[s_cur]);

    gz_wake_init(&s_wake, millis());
    bool     backlight_on = false;
    uint32_t last_refresh = 0;
#if GZ_SELFTEST_SERIAL
    /* Item J: one-line bench self-test summary over serial (off by default;
     * -DGZ_SELFTEST_SERIAL=1). Emitted HERE, on the task that owns the I2C bus,
     * so its RTC / IMU / gauge probes never interleave with another task's
     * register reads (a repeated-start read is two Wire calls). Read-only. */
    gz_ui_selftest_serial_emit();
#endif

    for (;;) {
        uint32_t wait = lv_timer_handler();

        if (!backlight_on && s_first_flush_done) {
            gz_display_backlight(GZ_LCD_BL_DEFAULT_PCT);
            backlight_on = true;
        }
        uint32_t now = millis();
        if (backlight_on) {
            gz_wake_action_t a = gz_wake_update(&s_wake, now, GZ_UI_DIM_AFTER_MS);
            if (a == GZ_WAKE_TO_DIM)    gz_display_backlight(GZ_UI_DIM_PCT);
            if (a == GZ_WAKE_TO_BRIGHT) gz_display_backlight(GZ_LCD_BL_DEFAULT_PCT);
        }
        if ((uint32_t)(now - last_refresh) >= GZ_UI_REFRESH_MS) {
            last_refresh = now;
#if GZ_BOARD_HAS_IMU && (GZ_UI_WAKE_ON_MOTION || GZ_UI_AUTO_ROTATE)
            /* One IMU sample per UI tick (4 Hz), only when the part answered
             * at boot: wake on motion and/or auto-rotate. */
            if (gz_imu_chip_id() == GZ_IMU_WHOAMI_VALUE) {
                gz_imu_sample_t m;
#if GZ_UI_ROTATE
                uint8_t prev = s_rot_imu;
#else
                uint8_t prev = 0;
#endif
                if (gz_imu_read(&m, prev)) {
#if GZ_UI_WAKE_ON_MOTION
                    if (m.moving) gz_wake_activity(&s_wake, now);
#endif
#if GZ_UI_ROTATE
                    s_rot_imu = m.rotation;
                    uint8_t want = gz_rotate_target(m.rotation, s_rot_cur, GZ_LCD_WIDTH,
                                                    GZ_LCD_HEIGHT, GZ_UI_ROT_REVERSE != 0);
                    if (s_rot_buf && want != s_rot_cur) {
                        s_rot_cur = want;
                        lv_display_set_rotation(s_disp, (lv_display_rotation_t)want);
                        lv_obj_invalidate(lv_screen_active());
                    }
#endif
                }
            }
#endif
#if GZ_BOARD_HAS_RTC
            /* Apply a time-set pushed by the Cardputer (CMD_TIMESET). Drained
             * here, on the RTC-owning UI task, whatever screen is showing, so a
             * sync sets the clock even when the watch face isn't visible. RTC
             * boards only — an RTC-less board has no clock to seed. */
            {
                uint32_t ts_epoch;
                int16_t  ts_tz;
                if (gz_link_take_timeset(&ts_epoch, &ts_tz))
                    gz_ui_watch_apply_timeset(ts_epoch, ts_tz, now);
            }
#endif
            /* Only the visible screen; the others catch up when shown. */
            if (s_in_selftest) refresh_selftest_screen();   /* item J page on top */
            else               refresh_current(now);

            /* Priority ALERT overlay (§4 item 6): polls the radar table on every
             * screen. A freshly raised alert counts as activity so the backlight
             * wakes. */
            bool was_alert = gz_ui_alert_active();
            gz_ui_alert_tick(s_alert_mask, s_alert_always_on, now);
            if (!was_alert && gz_ui_alert_active())
                gz_wake_activity(&s_wake, now);
        }
        if (wait < 5)  wait = 5;
        if (wait > 10) wait = 10;
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

/* ---- live settings apply (item I) ---------------------------------------- */
/* Called on the UI task after the customiser persists a change. Diffs the save
 * against what is currently applied (gz_ui_apply.h) and re-applies only the
 * consumers that moved: the alert overlay statics cheaply, and the active face
 * screen rebuilt once when the finish, accent, background or selected face
 * changed (item L) — so none of those need a reboot to take hold. */
void gz_ui_apply_settings(const gz_ui_settings_t *s)
{
    if (!s) return;
    gz_ui_consumers_t c = gz_ui_consumers_diff(&s_face_applied, s);
    if (!gz_ui_consumers_any(c)) { s_face_applied = *s; return; }

    if (c.alert) {
        /* The overlay tick reads these statics each pass (<=250 ms), so a new
         * mask / always-on takes hold on the next poll — no rebuild needed. */
        s_alert_mask      = s->alert_mask;
        s_alert_always_on = s->always_on != 0;
    }
#if GZ_BOARD_HAS_RTC
    /* The case finish (c.finish) and accent/background/selected-face (c.face)
     * both land on the one active face screen, so a save that moved either (or
     * both) costs exactly one rebuild, never two. */
    if (c.finish) gz_ui_watch_set_finish(s->finish);
    if (c.finish || c.face) {
        lv_obj_t *w = s_scr[GZ_SCR_WATCH];
        if (w) {
            /* Rebuild the dial so the new finish / accent / background / face
             * take hold live. lv_obj_clean deletes and frees the old children
             * (no LVGL leak); gz_ui_watch_build re-lays them, and the screen's
             * own event callbacks live on the screen, not its children, so they
             * survive. Resync if it is on screen. */
            lv_obj_clean(w);
            gz_ui_watch_build(w);
            if (s_cur == GZ_SCR_WATCH) gz_ui_watch_show(millis());
        }
        /* No screen yet (apply before the UI task built them): set_finish above
         * already makes it take effect on the first build. */
    }
#endif
    s_face_applied = *s;
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
#if GZ_UI_ROTATE
    /* Rotation target buffer (same size, DMA-capable). Not fatal if missing:
     * the UI just stays unrotated. */
    s_rot_buf = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_rot_buf) Serial.println("!! UI: no rotate buffer — auto-rotate off");
#endif
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
    Serial.printf("  UI: LVGL %d.%d.%d, 2 x %u B draw buffers, radar + hunt + BLE lab + "
#if GZ_BOARD_HAS_RTC
                  "watch + "
#endif
                  "status screens\n",
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                  (unsigned)buf_bytes);
    return true;
}
