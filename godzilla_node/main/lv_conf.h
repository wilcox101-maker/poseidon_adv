/*
 * lv_conf.h — LVGL 9.2.2 config for the GODZILLA node (P2a).
 *
 * Minimal on purpose: LVGL 9 fills every option not set here from
 * lv_conf_internal.h defaults. Found via -DLV_CONF_INCLUDE_SIMPLE and
 * -Igodzilla_node/main in [env:godzilla].
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* 360x360 RGB565 panel. */
#define LV_COLOR_DEPTH 16

/* C library heap: on the S3 with PSRAM, the Arduino core routes large mallocs
 * to PSRAM and keeps small object allocations internal. The two partial draw
 * buffers are allocated separately in internal DMA RAM by gz_ui.cpp. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Only the UI task touches LVGL, so no LVGL-side OS locking. */
#define LV_USE_OS LV_OS_NONE

/* Release: no LVGL logging, no asserts that print. */
#define LV_USE_LOG 0

/* Fonts the status + radar screens use — nothing else. 12/20 are the radar
 * detail card (P2b). */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Widgets: label, line, base obj (all on by default). Keep examples/demos out
 * of the image. */
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
