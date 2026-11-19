/*
 * gz_ui_radar.h — the P2b radar screen (LVGL 9). Called only from the UI task
 * in gz_ui.cpp, like everything else that touches LVGL.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

/* Build the radar into `scr` (an lv_obj_create(NULL) screen). */
void gz_ui_radar_build(lv_obj_t *scr);

/* Re-read the radar table and move the dots / sweep / card. ~4 Hz. */
void gz_ui_radar_refresh(uint32_t now_ms);

/* Long-press on the radar. With a detail card open: pick that contact for the
 * Hunt screen and return true (the caller shows it). Otherwise start a Recon
 * session on the SD card, or stop the running one, and return false; the REC
 * line above the footer shows the result. */
bool gz_ui_radar_long_press(void);

/* A tap at (x, y) on the radar screen: closes an open card, or opens the
 * card for the nearest dot within a finger's width. */
void gz_ui_radar_tap(int32_t x, int32_t y, uint32_t now_ms);
