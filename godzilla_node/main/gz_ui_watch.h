/*
 * gz_ui_watch.h — the P2c pocket-watch face (LVGL 9). Called only from the UI
 * task in gz_ui.cpp, like everything else that touches LVGL. The RTC reads and
 * the set-mode write also happen on that task.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

#include "gz_rtc.h"   /* gz_datetime_t */

/* Build the face into `scr` and take the first RTC reading. */
void gz_ui_watch_build(lv_obj_t *scr);

/* The watch screen became visible: resync from the RTC, redraw the hands. */
void gz_ui_watch_show(uint32_t now_ms);

/* The watch screen is being left: an unfinished set is dropped, not written. */
void gz_ui_watch_hide(void);

/* Called ~4 Hz while visible. Moves the hands only when the second changes
 * (or while a hand blinks in set mode). */
void gz_ui_watch_refresh(uint32_t now_ms);

/* A tap at screen row y (set mode: upper half hour +1, lower half minute +1). */
void gz_ui_watch_tap(int32_t y, uint32_t now_ms);

/* Long-press: enter set mode, or commit the set time to the RTC. */
void gz_ui_watch_long_press(uint32_t now_ms);

/* Current time from the software clock (for the status screen). False if the
 * clock has never had a time to run from. */
bool gz_ui_watch_time(uint32_t now_ms, gz_datetime_t *out);

/* Apply a CMD_TIMESET pushed by the Cardputer: convert the epoch (+ display-
 * only tz) to civil time, set the hardware RTC, restart the software clock from
 * it (so the UI and Recon read real time), and clear the "hold to set"
 * placeholder. On the UI task, which owns the RTC. RTC boards only — the whole
 * watch unit compiles only where GZ_BOARD_HAS_RTC. */
void gz_ui_watch_apply_timeset(uint32_t epoch, int16_t tz_offset_min, uint32_t now_ms);
