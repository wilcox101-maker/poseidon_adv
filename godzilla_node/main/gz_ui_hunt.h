/*
 * gz_ui_hunt.h — the P3 Hunt screen (LVGL 9): an RSSI trace for the one
 * contact picked on the radar. Called only from the UI task in gz_ui.cpp.
 */
#pragma once

#include <stdint.h>
#include <lvgl.h>

/* Build the Hunt screen into `scr` (an lv_obj_create(NULL) screen). */
void gz_ui_hunt_build(lv_obj_t *scr);

/* Re-read the hunt state and redraw the trace / readout. ~4 Hz. */
void gz_ui_hunt_refresh(uint32_t now_ms);

/* Long-press on the Hunt screen: stop hunting (back to the empty state). */
void gz_ui_hunt_long_press(void);
