/*
 * gz_board.h — board selector for the GODZILLA node.
 *
 * One node tree, two boards, chosen at BUILD time (never probed at run time:
 * the boards share no display or touch pins, so a wrong guess would drive the
 * wrong ones):
 *   default              Waveshare ESP32-S3-Touch-LCD-1.85B   (gz_board_185b.h)
 *   -DGZ_BOARD_AMOLED206 Waveshare ESP32-S3-Touch-AMOLED-2.06 (gz_board_amoled206.h)
 *
 * Every board header defines the same identity / capability macros:
 *   GZ_BOARD_NAME      human name for the boot log
 *   GZ_BOARD_HAS_UI    1 = display + touch are driven (gz_ui_start is called)
 *   GZ_BOARD_HAS_SD    1 = the TF slot is driven (gz_recon_start is called)
 *   GZ_LCD_ROUND       1 = round glass; 0 = rectangular panel
 *   GZ_LCD_WIDTH / GZ_LCD_HEIGHT
 *   GZ_PIN_I2C_SCL / GZ_PIN_I2C_SDA, GZ_I2C_ADDR_TOUCH / _RTC / _IMU / _GAUGE /
 *   _CODEC / _MIC_ADC, and GZ_I2C_NAME_TOUCH / GZ_I2C_NAME_GAUGE (scan labels)
 * plus its own pins and compile-time collision guards.
 *
 * Design: docs/proposals/2026-11-19-godzilla-amoled206-port.md.
 */
#pragma once

#if defined(GZ_BOARD_AMOLED206)
#include "gz_board_amoled206.h"
#else
#include "gz_board_185b.h"
#endif
