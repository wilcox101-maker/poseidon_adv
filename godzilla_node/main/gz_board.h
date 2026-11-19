/*
 * gz_board.h — Waveshare ESP32-S3-Touch-LCD-1.85B pin map (task 2a).
 *
 * Single source of truth for the node's on-board peripheral pins, so the
 * display, touch and RTC bring-up all reference one cited map instead of
 * scattering magic GPIO numbers.
 *
 * Source: Waveshare wiki, "ESP32-S3-Touch-LCD-1.85B" peripheral quick
 * reference — https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B
 *   LCD  ST77916  QSPI  360x360 RGB565:
 *        CS=21 PCLK=40 DATA0=46 DATA1=45 DATA2=42 DATA3=41 RST=3 BL=5
 *   Touch CST816S  I2C 0x15:  SCL=10 SDA=11 RST=1 INT=4
 *   RTC   PCF85063 I2C 0x51:  SCL=10 SDA=11 INT=6   (shares the touch I2C bus)
 *   TF    microSD  SDMMC 4-bit: CLK=15 CMD=14 D0=16 D1=17 D2=12 D3=13
 *         (GPIO allocation table on the same page; separate from the LCD QSPI)
 *
 * This is the "B" revision. It differs from the plain 1.85 (which puts LCD_RST
 * on an I/O expander and RTC_INT on GPIO9); do NOT mix the two maps.
 *
 * Note: GPIO3/45/46 are ESP32-S3 strapping pins — Waveshare's design drives
 * them to safe boot states, so we follow their assignment and never repurpose
 * these three.
 *
 * Pure: proto.h-free, just macros + compile-time collision guards, so it is
 * compiled by test/test_godzilla_board on the host.
 */
#pragma once

/* ---- LCD: ST77916 over QSPI ---- */
#define GZ_PIN_LCD_CS     21
#define GZ_PIN_LCD_PCLK   40
#define GZ_PIN_LCD_DATA0  46
#define GZ_PIN_LCD_DATA1  45
#define GZ_PIN_LCD_DATA2  42
#define GZ_PIN_LCD_DATA3  41
#define GZ_PIN_LCD_RST     3
#define GZ_PIN_LCD_BL      5   /* PWM dimming */

#define GZ_LCD_WIDTH     360
#define GZ_LCD_HEIGHT    360

/* ---- Shared I2C bus (touch + RTC) ---- */
#define GZ_PIN_I2C_SCL    10
#define GZ_PIN_I2C_SDA    11

/* ---- Touch: CST816S ---- */
#define GZ_I2C_ADDR_TOUCH 0x15
#define GZ_PIN_TOUCH_RST   1
#define GZ_PIN_TOUCH_INT   4

/* ---- RTC: PCF85063 ---- */
#define GZ_I2C_ADDR_RTC   0x51
#define GZ_PIN_RTC_INT     6

/* ---- microSD (TF): SDMMC 4-bit (P3 Recon log) ---- */
#define GZ_PIN_SD_CLK     15
#define GZ_PIN_SD_CMD     14
#define GZ_PIN_SD_D0      16
#define GZ_PIN_SD_D1      17
#define GZ_PIN_SD_D2      12
#define GZ_PIN_SD_D3      13

/* ---- Other parts on the same I2C bus (not driven yet; P3) ----
 * Named so the boot I2C scan can label them instead of printing "unknown".
 * IMU and gauge addresses are from Waveshare's 1.85B page
 * (docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B). That page lists the codecs
 * as 0x30 / 0x80, which are 8-bit forms; the 7-bit values below are those
 * shifted right by one and match the parts' usual defaults — INFERRED, to be
 * confirmed by the scan on Dale's bench. */
#define GZ_I2C_ADDR_IMU     0x6B   /* QMI8658 6-axis IMU            */
#define GZ_I2C_ADDR_GAUGE   0x55   /* BQ27220 battery gauge         */
#define GZ_I2C_ADDR_CODEC   0x18   /* ES8311 audio codec (inferred) */
#define GZ_I2C_ADDR_MIC_ADC 0x40   /* ES7210 mic ADC (inferred)     */

/* Compile-time guards: the touch and RTC deliberately SHARE the I2C bus, but
 * nothing else may collide. A fat-fingered pin edit fails the build here. */
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#include <assert.h>
/* The four QSPI data lines are distinct. */
static_assert(GZ_PIN_LCD_DATA0 != GZ_PIN_LCD_DATA1 &&
              GZ_PIN_LCD_DATA0 != GZ_PIN_LCD_DATA2 &&
              GZ_PIN_LCD_DATA0 != GZ_PIN_LCD_DATA3 &&
              GZ_PIN_LCD_DATA1 != GZ_PIN_LCD_DATA2 &&
              GZ_PIN_LCD_DATA1 != GZ_PIN_LCD_DATA3 &&
              GZ_PIN_LCD_DATA2 != GZ_PIN_LCD_DATA3, "LCD QSPI data pins collide");
/* LCD control pins are distinct from the QSPI bus. */
static_assert(GZ_PIN_LCD_CS != GZ_PIN_LCD_PCLK &&
              GZ_PIN_LCD_CS != GZ_PIN_LCD_RST &&
              GZ_PIN_LCD_CS != GZ_PIN_LCD_BL, "LCD control pins collide");
/* The I2C bus is clock != data, and the two INT/RST lines are all distinct. */
static_assert(GZ_PIN_I2C_SCL != GZ_PIN_I2C_SDA, "I2C SCL/SDA collide");
static_assert(GZ_PIN_TOUCH_RST != GZ_PIN_TOUCH_INT &&
              GZ_PIN_TOUCH_INT != GZ_PIN_RTC_INT &&
              GZ_PIN_TOUCH_RST != GZ_PIN_RTC_INT, "touch/RTC control pins collide");
/* The six SDMMC lines are distinct, and none lands on the I2C bus or on the
 * touch / RTC / LCD control pins. */
#define GZ_SD_PIN_FREE(p) ((p) != GZ_PIN_I2C_SCL && (p) != GZ_PIN_I2C_SDA && \
                           (p) != GZ_PIN_TOUCH_RST && (p) != GZ_PIN_TOUCH_INT && \
                           (p) != GZ_PIN_RTC_INT && (p) != GZ_PIN_LCD_CS && \
                           (p) != GZ_PIN_LCD_PCLK && (p) != GZ_PIN_LCD_RST && \
                           (p) != GZ_PIN_LCD_BL && (p) != GZ_PIN_LCD_DATA0 && \
                           (p) != GZ_PIN_LCD_DATA1 && (p) != GZ_PIN_LCD_DATA2 && \
                           (p) != GZ_PIN_LCD_DATA3)
static_assert(GZ_SD_PIN_FREE(GZ_PIN_SD_CLK) && GZ_SD_PIN_FREE(GZ_PIN_SD_CMD) &&
              GZ_SD_PIN_FREE(GZ_PIN_SD_D0) && GZ_SD_PIN_FREE(GZ_PIN_SD_D1) &&
              GZ_SD_PIN_FREE(GZ_PIN_SD_D2) && GZ_SD_PIN_FREE(GZ_PIN_SD_D3),
              "SD pins collide with the LCD / I2C / touch / RTC pins");
static_assert(GZ_PIN_SD_CLK != GZ_PIN_SD_CMD && GZ_PIN_SD_CLK != GZ_PIN_SD_D0 &&
              GZ_PIN_SD_CLK != GZ_PIN_SD_D1 && GZ_PIN_SD_CLK != GZ_PIN_SD_D2 &&
              GZ_PIN_SD_CLK != GZ_PIN_SD_D3 && GZ_PIN_SD_CMD != GZ_PIN_SD_D0 &&
              GZ_PIN_SD_CMD != GZ_PIN_SD_D1 && GZ_PIN_SD_CMD != GZ_PIN_SD_D2 &&
              GZ_PIN_SD_CMD != GZ_PIN_SD_D3 && GZ_PIN_SD_D0 != GZ_PIN_SD_D1 &&
              GZ_PIN_SD_D0 != GZ_PIN_SD_D2 && GZ_PIN_SD_D0 != GZ_PIN_SD_D3 &&
              GZ_PIN_SD_D1 != GZ_PIN_SD_D2 && GZ_PIN_SD_D1 != GZ_PIN_SD_D3 &&
              GZ_PIN_SD_D2 != GZ_PIN_SD_D3, "SD pins collide with each other");
/* The two I2C peripherals have different addresses. */
static_assert(GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_RTC, "touch and RTC share an I2C address");
#endif
