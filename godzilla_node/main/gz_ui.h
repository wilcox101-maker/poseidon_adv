/*
 * gz_ui.h — LVGL 9 UI for the GODZILLA node (P2a: status screen).
 *
 * gz_ui_start() brings up the display (gz_display.h) and starts the UI in its
 * OWN FreeRTOS task on core 1. That task is the only code that ever calls
 * LVGL: it runs lv_timer_handler every ~5-10 ms and refreshes the status
 * screen from gz_stats_snapshot() a few times a second. The Arduino loop (BLE /
 * Wi-Fi publish ticks) is never blocked on rendering or SPI flushes.
 */
#pragma once

#include <stdbool.h>

#include "ui/faces/gz_faces.h"   /* gz_ui_settings_t */

/* Bring up the panel and start the UI task. Returns false if the display did
 * not come up (the radios still run — the node is useful headless). */
bool gz_ui_start(void);

/* Apply a just-saved customiser settings blob to its live consumers WITHOUT a
 * reboot (watch-faces item I): the alert overlay's per-family mask + always-on
 * update at once, and a changed Classic case finish rebuilds the watch face.
 * A no-op for fields with no live consumer, and for the finish on an RTC-less
 * board. The customiser calls this on the UI task after each gz_face_store_save
 * (which is also the only task that may touch LVGL). Safe to call before the UI
 * task has built its screens — the finish then simply applies on the next
 * build. */
void gz_ui_apply_settings(const gz_ui_settings_t *s);

/* Item J: print the one-line bench self-test summary to serial. Called once by
 * the UI task (which owns the I2C bus) behind GZ_SELFTEST_SERIAL (default off). Read-only — it
 * probes the on-board parts and prints "SELFTEST disp=ok touch=ok ..."; it never
 * writes a device or transmits. On a headless board (no UI) the readings that
 * depend on the panel/touch simply report absent. */
void gz_ui_selftest_serial_emit(void);
