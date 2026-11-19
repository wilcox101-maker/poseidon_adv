/*
 * gz_touch.h — CST816S capacitive-touch driver for the GODZILLA node (P2a).
 *
 * The Waveshare 1.85B carries a Hynitron CST816S touch controller on the
 * shared touch+RTC I2C bus (gz_board.h: 0x15, SCL 10 / SDA 11, RST 1, INT 4).
 * This is the P2a unit after the PCF85063 RTC (#72). It gives the LVGL task a
 * pointer input device now, and the P2b radar / P2c watch screens their
 * swipe-to-switch later.
 *
 * Register map (Hynitron CST816S register description; the same map is used
 * by Espressif's esp_lcd_touch_cst816s component):
 *   0x01 GestureID   0x00 none, 0x01 up, 0x02 down, 0x03 left, 0x04 right,
 *                    0x05 single click, 0x0B double click, 0x0C long press
 *   0x02 FingerNum   0 or 1
 *   0x03 XposH       bits7-6 = event (0 down, 1 up, 2 contact), bits3-0 = X[11:8]
 *   0x04 XposL       X[7:0]
 *   0x05 YposH       bits7-6 = touch id, bits3-0 = Y[11:8]
 *   0x06 YposL       Y[7:0]
 *   0xA7 ChipID      0xB4 CST816S, 0xB5 CST816T, 0xB6 CST816D, 0x20 CST716
 *   0xA9 FwVersion
 *
 * Power: the CST816S drops into auto-sleep a few seconds after the last touch
 * and then NACKs I2C until a finger wakes it (it pulls INT low when it does).
 * We therefore do NOT poll the bus blindly: a falling edge on INT arms a read,
 * and while a finger is down we keep reading each LVGL indev period until it
 * reports zero fingers. Auto-sleep stays enabled (lower power, no NACK spam).
 *
 * Orientation: the 1.85B demo maps touch 1:1 onto the 360x360 panel (no swap,
 * no mirror). GZ_TOUCH_SWAP_XY / _MIRROR_X / _MIRROR_Y default to 0 and are
 * the one-line fix if Dale's bench shows the axes flipped.
 *
 * Passive w.r.t. the project's RF rules: on-board I2C to a soldered-on
 * peripheral; no radio frames.
 *
 * Pure: stdint/stdbool only, no Arduino/Wire/LVGL, so test_godzilla_touch pins
 * the report parsing, orientation transform and name tables on the host. The
 * Wire transactions and the INT ISR are ARDUINO-only, behind #ifdef. The LVGL
 * indev glue lives in gz_ui.cpp (the only LVGL caller).
 *
 * Boards: this file is the touch entry point for every board. On the CST816S
 * board (1.85B, GZ_BOARD_TOUCH_FT 0) its own ARDUINO half below is the driver;
 * on FocalTech boards it includes gz_touch_ft.h, which provides the same API
 * (gz_touch_begin / _poll / _found / _name / _last_gesture).
 *
 * ONE TU ONLY: the ARDUINO half keeps its state in file-static variables, so
 * it must be used from exactly one translation unit — gz_ui.cpp, which calls
 * gz_touch_begin() from gz_ui_start() and gz_touch_poll() from the indev
 * callback. Other files may include this header for the pure helpers only.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_board.h"   /* GZ_I2C_ADDR_TOUCH / GZ_PIN_TOUCH_* / GZ_LCD_* */

/* ---- registers ----------------------------------------------------------- */
#define GZ_TOUCH_REG_GESTURE  0x01   /* first byte of the 6-byte report */
#define GZ_TOUCH_REPORT_BYTES 6      /* 0x01..0x06 */
#define GZ_TOUCH_REG_CHIP_ID  0xA7
#define GZ_TOUCH_REG_FW_VER   0xA9

/* ---- orientation (bench-tunable) ----------------------------------------- */
#ifndef GZ_TOUCH_SWAP_XY
#define GZ_TOUCH_SWAP_XY  0
#endif
#ifndef GZ_TOUCH_MIRROR_X
#define GZ_TOUCH_MIRROR_X 0
#endif
#ifndef GZ_TOUCH_MIRROR_Y
#define GZ_TOUCH_MIRROR_Y 0
#endif

/* Gesture IDs (register 0x01). */
typedef enum {
    GZ_GEST_NONE         = 0x00,
    GZ_GEST_SWIPE_UP     = 0x01,
    GZ_GEST_SWIPE_DOWN   = 0x02,
    GZ_GEST_SWIPE_LEFT   = 0x03,
    GZ_GEST_SWIPE_RIGHT  = 0x04,
    GZ_GEST_CLICK        = 0x05,
    GZ_GEST_DOUBLE_CLICK = 0x0B,
    GZ_GEST_LONG_PRESS   = 0x0C,
} gz_gesture_t;

/* Touch event (XposH bits 7-6). */
#define GZ_TOUCH_EVT_DOWN    0
#define GZ_TOUCH_EVT_UP      1
#define GZ_TOUCH_EVT_CONTACT 2

/* One parsed report, already mapped to panel coordinates. */
typedef struct {
    uint8_t  gesture;   /* gz_gesture_t value as read (unknown IDs kept)  */
    uint8_t  fingers;   /* 0 or 1 (anything else is clamped to 1)         */
    uint8_t  event;     /* GZ_TOUCH_EVT_*                                 */
    uint16_t x;         /* 0..GZ_LCD_WIDTH-1                              */
    uint16_t y;         /* 0..GZ_LCD_HEIGHT-1                             */
    bool     pressed;   /* fingers > 0 and the event is not "up"          */
} gz_touch_point_t;

/* ---- pure helpers -------------------------------------------------------- */

static inline uint16_t gz_touch_clamp(uint16_t v, uint16_t max_excl)
{
    return (v >= max_excl) ? (uint16_t)(max_excl - 1u) : v;
}

/* Raw controller coords -> panel coords: swap, then mirror, then clamp. */
static inline void gz_touch_orient(uint16_t rx, uint16_t ry,
                                   bool swap_xy, bool mirror_x, bool mirror_y,
                                   uint16_t *ox, uint16_t *oy)
{
    uint16_t x = swap_xy ? ry : rx;
    uint16_t y = swap_xy ? rx : ry;
    x = gz_touch_clamp(x, GZ_LCD_WIDTH);
    y = gz_touch_clamp(y, GZ_LCD_HEIGHT);
    if (mirror_x) x = (uint16_t)(GZ_LCD_WIDTH  - 1u - x);
    if (mirror_y) y = (uint16_t)(GZ_LCD_HEIGHT - 1u - y);
    *ox = x;
    *oy = y;
}

/* Parse the 6-byte block read from 0x01 with the build's orientation. */
static inline void gz_touch_parse(const uint8_t r[GZ_TOUCH_REPORT_BYTES],
                                  gz_touch_point_t *p)
{
    uint16_t rx = (uint16_t)(((r[2] & 0x0Fu) << 8) | r[3]);
    uint16_t ry = (uint16_t)(((r[4] & 0x0Fu) << 8) | r[5]);
    p->gesture = r[0];
    p->fingers = (r[1] == 0) ? 0 : 1;
    p->event   = (uint8_t)(r[2] >> 6);
    gz_touch_orient(rx, ry, GZ_TOUCH_SWAP_XY != 0, GZ_TOUCH_MIRROR_X != 0,
                    GZ_TOUCH_MIRROR_Y != 0, &p->x, &p->y);
    p->pressed = (p->fingers > 0) && (p->event != GZ_TOUCH_EVT_UP);
}

static inline const char *gz_touch_chip_name(uint8_t id)
{
    switch (id) {
    case 0xB4: return "CST816S";
    case 0xB5: return "CST816T";
    case 0xB6: return "CST816D";
    case 0x20: return "CST716";
    default:   return "unknown";
    }
}

static inline const char *gz_touch_gesture_name(uint8_t g)
{
    switch (g) {
    case GZ_GEST_NONE:         return "none";
    case GZ_GEST_SWIPE_UP:     return "swipe up";
    case GZ_GEST_SWIPE_DOWN:   return "swipe down";
    case GZ_GEST_SWIPE_LEFT:   return "swipe left";
    case GZ_GEST_SWIPE_RIGHT:  return "swipe right";
    case GZ_GEST_CLICK:        return "tap";
    case GZ_GEST_DOUBLE_CLICK: return "double tap";
    case GZ_GEST_LONG_PRESS:   return "long press";
    default:                   return "?";
    }
}

#if defined(ARDUINO) && !GZ_BOARD_TOUCH_FT   /* CST816S boards (1.85B) */
#include <Arduino.h>
#include <Wire.h>

/* State shared by the INT ISR and the reader. Reads happen only on the LVGL
 * task (via gz_ui.cpp's indev callback), so only the ISR flag is cross-context.
 * Wire itself is mutex-guarded in arduino-esp32, so the RTC (main task) and the
 * touch (LVGL task) may share the bus. */
static volatile bool s_gz_touch_irq     = false;
static bool          s_gz_touch_down    = false;
static uint8_t       s_gz_touch_chip_id = 0;
static uint8_t       s_gz_touch_last_gesture = GZ_GEST_NONE;

static void IRAM_ATTR gz_touch_isr(void) { s_gz_touch_irq = true; }

static inline bool gz_touch_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    Wire.beginTransmission(GZ_I2C_ADDR_TOUCH);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;          /* repeated start */
    if (Wire.requestFrom((int)GZ_I2C_ADDR_TOUCH, (int)n) != (int)n) return false;
    for (uint8_t i = 0; i < n; ++i) buf[i] = (uint8_t)Wire.read();
    return true;
}

/* Hardware reset, arm the INT edge, and log the chip ID. Assumes Wire.begin()
 * already ran (gz_i2c_scan_report() does it). Returns true if the controller
 * answered after reset; the indev is registered either way (a later touch
 * wakes it). */
static inline bool gz_touch_begin(void)
{
    pinMode(GZ_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(GZ_PIN_TOUCH_RST, LOW);
    delay(10);
    digitalWrite(GZ_PIN_TOUCH_RST, HIGH);
    delay(50);                                   /* controller boot */

    pinMode(GZ_PIN_TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GZ_PIN_TOUCH_INT), gz_touch_isr, FALLING);

    uint8_t id_fw[3];
    if (!gz_touch_read_regs(GZ_TOUCH_REG_CHIP_ID, id_fw, 3)) {   /* 0xA7..0xA9 */
        Serial.printf("!! touch: no ACK at 0x%02X after reset (RST=%u INT=%u)\n",
                      (unsigned)GZ_I2C_ADDR_TOUCH, (unsigned)GZ_PIN_TOUCH_RST,
                      (unsigned)GZ_PIN_TOUCH_INT);
        return false;
    }
    s_gz_touch_chip_id = id_fw[0];
    Serial.printf("  touch: %s (id 0x%02X, fw 0x%02X) at 0x%02X, INT=%u\n",
                  gz_touch_chip_name(id_fw[0]), (unsigned)id_fw[0],
                  (unsigned)id_fw[2], (unsigned)GZ_I2C_ADDR_TOUCH,
                  (unsigned)GZ_PIN_TOUCH_INT);
    return true;
}

/* Call from the LVGL indev read callback. Reads the bus only when INT fired or
 * a finger is already down; otherwise returns the last (released) state. A
 * failed read while down counts as a release, so a NACK can never leave LVGL
 * stuck "pressed". Returns true when *p holds a fresh report. */
static inline bool gz_touch_poll(gz_touch_point_t *p)
{
    if (!s_gz_touch_irq && !s_gz_touch_down) return false;
    s_gz_touch_irq = false;

    uint8_t r[GZ_TOUCH_REPORT_BYTES];
    if (!gz_touch_read_regs(GZ_TOUCH_REG_GESTURE, r, GZ_TOUCH_REPORT_BYTES)) {
        s_gz_touch_down = false;
        p->pressed = false;
        return false;
    }
    gz_touch_parse(r, p);
    if (p->gesture != GZ_GEST_NONE) s_gz_touch_last_gesture = p->gesture;
    s_gz_touch_down = p->pressed;
    return true;
}

static inline uint8_t gz_touch_chip_id(void)      { return s_gz_touch_chip_id; }
static inline uint8_t gz_touch_last_gesture(void) { return s_gz_touch_last_gesture; }
/* Board-neutral API, shared with the FocalTech driver (gz_touch_ft.h). */
static inline bool        gz_touch_found(void) { return s_gz_touch_chip_id != 0; }
static inline const char *gz_touch_name(void)  { return gz_touch_chip_name(s_gz_touch_chip_id); }
#endif /* ARDUINO && !GZ_BOARD_TOUCH_FT */

/* FocalTech boards (AMOLED-2.06, ES3C28P): parser + driver with the same API. */
#if GZ_BOARD_TOUCH_FT
#include "gz_touch_ft.h"
#endif
