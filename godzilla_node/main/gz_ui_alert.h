/*
 * gz_ui_alert.h — priority ALERT overlay (detection §4 item 6, LVGL 9). Called
 * only from the UI task in gz_ui.cpp, like every other LVGL surface.
 *
 * A full-screen overlay on the top layer that pops when a HIGH-confidence
 * surveillance / recording family, or a tracker seen repeatedly, appears in the
 * node's own radar table. The decision core (which family, when, priority,
 * rate-limit, 60 s auto-clear) is the pure, host-tested gz_alert.h; this file is
 * only the rendering and the per-device rate-limit bookkeeping.
 *
 * Passive: it reads the radar snapshot the scanners already fill — no new
 * capture path, nothing transmits.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

/* Build the (hidden) overlay on the top layer. Call once from the UI task. */
void gz_ui_alert_build(void);

/* Poll the radar table and raise / refresh / auto-clear the overlay.
 * `alert_mask` is the persisted per-family enable mask (gz_ui_settings_t);
 * `always_on` dims the overlay for the always-on screen. ~4 Hz from ui_task. */
void gz_ui_alert_tick(uint16_t alert_mask, bool always_on, uint32_t now_ms);

/* A screen tap arrived. If the overlay is showing, dismiss it and return true
 * (the tap is consumed — it must not also act on the screen beneath). */
bool gz_ui_alert_tap(void);

/* True while the overlay is visible. */
bool gz_ui_alert_active(void);
