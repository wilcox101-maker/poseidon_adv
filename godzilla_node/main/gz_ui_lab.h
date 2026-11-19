/*
 * gz_ui_lab.h — the BLE Lab screen (LVGL 9, #98 §2.4b): recent BLE adverts,
 * strongest first; tap a row for its decoded detail. Passive: it shows only
 * what the scan already receives. Called only from the UI task in gz_ui.cpp.
 */
#pragma once

#include <stdint.h>
#include <lvgl.h>

/* Build the BLE Lab screen into `scr` (an lv_obj_create(NULL) screen). */
void gz_ui_lab_build(lv_obj_t *scr);

/* Re-read the table and redraw. Called ~4 Hz; the list re-sorts at 1 Hz so
 * rows do not jump under a finger. */
void gz_ui_lab_refresh(uint32_t now_ms);

/* Tap at screen y: on the list, open that row's detail; on the detail, go
 * back to the list. */
void gz_ui_lab_tap(int y, uint32_t now_ms);

/* Leaving the screen: back to the list, so it always opens there. */
void gz_ui_lab_hide(void);
