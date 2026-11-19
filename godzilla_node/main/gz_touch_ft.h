/*
 * gz_touch_ft.h — FocalTech capacitive-touch parser + driver (three-board port,
 * completion orders #88 §3.1). One driver for the two FocalTech parts:
 *   FT3168  — Waveshare ESP32-S3-Touch-AMOLED-2.06 (gz_board_amoled206.h)
 *   FT6336G — 2.8" ESP32-S3 Display ES3C28P      (gz_board_es3c28p.h)
 * The 1.85B's Hynitron CST816S keeps its own driver in gz_touch.h.
 *
 * Report block, read in one transaction from 0x02 (FocalTech layout; the
 * register addresses match Waveshare's Arduino_FT3x68 driver for the 2.06 —
 * facts cited, no code taken):
 *   0x02 TD_STATUS   bits3-0 = touch points (FT6336G: up to 2)
 *   0x03 P1_XH       bits7-6 = event flag, bits3-0 = X[11:8]
 *   0x04 P1_XL       X[7:0]
 *   0x05 P1_YH       bits7-4 = touch id,   bits3-0 = Y[11:8]
 *   0x06 P1_YL       Y[7:0]
 * Event flag: 0 press down, 1 lift up, 2 contact, 3 no event. Only point 1 is
 * used: the UI is single-pointer (LVGL does its own swipe detection).
 *
 * Address: 0x38 on both boards is FocalTech's usual default and the Waveshare
 * driver's value for the FT3168; for the FT6336G it is INFERRED (lcdwiki and
 * the datasheet give none). Per #88 §4 the boot I2C scan confirms it, and a
 * part that does not answer is logged and skipped, never fatal: the indev stays
 * registered and every INT edge retries the read.
 *
 * Read-only: the driver writes no controller register (the controller's own
 * power-on mode is used; reset is the RST pin). Passive w.r.t. the RF rules.
 *
 * Pure half: stdint/stdbool + gz_touch.h's shared types and orientation, so
 * test_godzilla_touch_ft runs it on the host. The Wire / INT half is ARDUINO-
 * only and, like gz_touch.h's, keeps file-static state: ONE TU ONLY (gz_ui.cpp).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_touch.h"   /* gz_touch_point_t, gz_touch_orient, GZ_TOUCH_* */

#define GZ_TOUCH_FT_REG_STATUS    0x02   /* first byte of the 5-byte report */
#define GZ_TOUCH_FT_REPORT_BYTES  5      /* 0x02..0x06                      */
#define GZ_TOUCH_FT_MAX_POINTS    2

/* Event flag (P1_XH bits 7-6). */
#define GZ_TOUCH_FT_EVT_DOWN      0
#define GZ_TOUCH_FT_EVT_UP        1
#define GZ_TOUCH_FT_EVT_CONTACT   2
#define GZ_TOUCH_FT_EVT_NONE      3

/* Parse the 5-byte block read from 0x02 into panel coordinates, with the
 * build's orientation. A point count of 0 or above the part's maximum (an idle
 * controller can report 0x0F) is "no finger"; so is a lift-up or a no-event
 * flag. FocalTech has no gesture register in this mode: gesture is NONE. */
static inline void gz_touch_ft_parse(const uint8_t r[GZ_TOUCH_FT_REPORT_BYTES],
                                     gz_touch_point_t *p)
{
    uint8_t  points = (uint8_t)(r[0] & 0x0Fu);
    uint8_t  evt    = (uint8_t)(r[1] >> 6);
    uint16_t rx     = (uint16_t)(((r[1] & 0x0Fu) << 8) | r[2]);
    uint16_t ry     = (uint16_t)(((r[3] & 0x0Fu) << 8) | r[4]);
    bool valid = points >= 1 && points <= GZ_TOUCH_FT_MAX_POINTS;

    p->gesture = GZ_GEST_NONE;
    p->fingers = valid ? 1 : 0;
    /* Map FocalTech events onto the CST-shaped event field. */
    p->event   = (evt == GZ_TOUCH_FT_EVT_DOWN)    ? GZ_TOUCH_EVT_DOWN
               : (evt == GZ_TOUCH_FT_EVT_CONTACT) ? GZ_TOUCH_EVT_CONTACT
                                                  : GZ_TOUCH_EVT_UP;
    gz_touch_orient(rx, ry, GZ_TOUCH_SWAP_XY != 0, GZ_TOUCH_MIRROR_X != 0,
                    GZ_TOUCH_MIRROR_Y != 0, &p->x, &p->y);
    p->pressed = valid && (evt == GZ_TOUCH_FT_EVT_DOWN || evt == GZ_TOUCH_FT_EVT_CONTACT);
}

/* ---- hardware half (FocalTech boards only) -------------------------------- */
#if defined(ARDUINO) && GZ_BOARD_TOUCH_FT
#include <Arduino.h>
#include <Wire.h>

static volatile bool s_gz_ft_irq   = false;
static bool          s_gz_ft_down  = false;
static bool          s_gz_ft_found = false;
static uint8_t       s_gz_ft_id    = 0;     /* raw byte at GZ_TOUCH_FT_ID_REG */

static void IRAM_ATTR gz_touch_ft_isr(void) { s_gz_ft_irq = true; }

static inline bool gz_touch_ft_read(uint8_t reg, uint8_t *buf, uint8_t n)
{
    Wire.beginTransmission(GZ_I2C_ADDR_TOUCH);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;          /* repeated start */
    if (Wire.requestFrom((int)GZ_I2C_ADDR_TOUCH, (int)n) != (int)n) return false;
    for (uint8_t i = 0; i < n; ++i) buf[i] = (uint8_t)Wire.read();
    return true;
}

/* Same API as the CST816S half, so gz_ui.cpp is driver-agnostic. Assumes
 * Wire.begin() already ran (gz_i2c_scan_report()). Never fatal. */
static inline bool gz_touch_begin(void)
{
    pinMode(GZ_PIN_TOUCH_RST, OUTPUT);
    digitalWrite(GZ_PIN_TOUCH_RST, LOW);        /* low = reset on both parts */
    delay(10);
    digitalWrite(GZ_PIN_TOUCH_RST, HIGH);
    delay(GZ_TOUCH_FT_BOOT_MS);                 /* controller boot after reset */

    pinMode(GZ_PIN_TOUCH_INT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GZ_PIN_TOUCH_INT), gz_touch_ft_isr, FALLING);

    uint8_t st;
    if (!gz_touch_ft_read(GZ_TOUCH_FT_REG_STATUS, &st, 1)) {
        Serial.printf("!! touch: %s not answering at 0x%02X (RST=%u INT=%u) — "
                      "check the boot I2C scan; continuing without touch\n",
                      GZ_I2C_NAME_TOUCH, (unsigned)GZ_I2C_ADDR_TOUCH,
                      (unsigned)GZ_PIN_TOUCH_RST, (unsigned)GZ_PIN_TOUCH_INT);
        return false;
    }
    s_gz_ft_found = true;
    if (!gz_touch_ft_read(GZ_TOUCH_FT_ID_REG, &s_gz_ft_id, 1)) s_gz_ft_id = 0;
    Serial.printf("  touch: %s at 0x%02X, id reg 0x%02X = 0x%02X, INT=%u\n",
                  GZ_I2C_NAME_TOUCH, (unsigned)GZ_I2C_ADDR_TOUCH,
                  (unsigned)GZ_TOUCH_FT_ID_REG, (unsigned)s_gz_ft_id,
                  (unsigned)GZ_PIN_TOUCH_INT);
    return true;
}

/* Reads only after an INT edge or while a finger is down; a failed read while
 * down counts as a release, so LVGL can never stick "pressed". A controller
 * that was missing at boot is picked up on its first successful read. */
static inline bool gz_touch_poll(gz_touch_point_t *p)
{
    if (!s_gz_ft_irq && !s_gz_ft_down) return false;
    s_gz_ft_irq = false;

    uint8_t r[GZ_TOUCH_FT_REPORT_BYTES];
    if (!gz_touch_ft_read(GZ_TOUCH_FT_REG_STATUS, r, GZ_TOUCH_FT_REPORT_BYTES)) {
        s_gz_ft_down = false;
        p->pressed = false;
        return false;
    }
    s_gz_ft_found = true;
    gz_touch_ft_parse(r, p);
    s_gz_ft_down = p->pressed;
    return true;
}

static inline bool        gz_touch_found(void)        { return s_gz_ft_found; }
static inline const char *gz_touch_name(void)         { return GZ_I2C_NAME_TOUCH; }
static inline uint8_t     gz_touch_last_gesture(void) { return GZ_GEST_NONE; }
#endif /* ARDUINO && GZ_BOARD_TOUCH_FT */
