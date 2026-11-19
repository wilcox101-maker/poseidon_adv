/*
 * gz_board.h — board selector for the GODZILLA node.
 *
 * One node tree, two boards, chosen at BUILD time (never probed at run time:
 * the boards share no display or touch pins, so a wrong guess would drive the
 * wrong ones):
 *   default              Waveshare ESP32-S3-Touch-LCD-1.85B   (gz_board_185b.h)
 *   -DGZ_BOARD_AMOLED206 Waveshare ESP32-S3-Touch-AMOLED-2.06 (gz_board_amoled206.h)
 *   -DGZ_BOARD_ES3C28P   2.8" ESP32-S3 Display, ES3C28P       (gz_board_es3c28p.h)
 *
 * Every board header defines the same identity / capability macros:
 *   GZ_BOARD_NAME      human name for the boot log
 *   GZ_BOARD_HAS_UI    1 = display + touch are driven (gz_ui_start is called)
 *   GZ_BOARD_HAS_SD    1 = the TF slot is driven (gz_recon_start is called)
 *   GZ_BOARD_HAS_RTC / _IMU / _GAUGE / _MIC_ADC
 *                      1 = that I2C part is fitted; 0 = absent, and its
 *                      GZ_I2C_ADDR_* macro is not defined at all
 *   GZ_LCD_ROUND       1 = round glass; 0 = rectangular panel
 *   GZ_BOARD_TOUCH_FT  1 = FocalTech touch (gz_touch_ft.h); 0 = CST816S
 *   GZ_LCD_WIDTH / GZ_LCD_HEIGHT
 *   GZ_PIN_I2C_SCL / GZ_PIN_I2C_SDA, GZ_I2C_ADDR_TOUCH / _RTC / _IMU / _GAUGE /
 *   _CODEC / _MIC_ADC (each only when fitted), GZ_I2C_NAME_TOUCH and, with a
 *   gauge, GZ_I2C_NAME_GAUGE (scan labels)
 * plus its own pins and compile-time collision guards.
 *
 * Designs: docs/proposals/2026-11-19-godzilla-amoled206-port.md and
 * docs/proposals/2026-11-19-godzilla-es3c28p-port.md.
 */
#pragma once

#if defined(GZ_BOARD_AMOLED206) && defined(GZ_BOARD_ES3C28P)
#error "Define at most one GZ_BOARD_* flag"
#endif

#if defined(GZ_BOARD_AMOLED206)
#include "gz_board_amoled206.h"
#elif defined(GZ_BOARD_ES3C28P)
#include "gz_board_es3c28p.h"
#else
#include "gz_board_185b.h"
#endif
