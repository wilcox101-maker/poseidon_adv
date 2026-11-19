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

/* Bring up the panel and start the UI task. Returns false if the display did
 * not come up (the radios still run — the node is useful headless). */
bool gz_ui_start(void);
