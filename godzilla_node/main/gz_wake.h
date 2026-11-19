/*
 * gz_wake.h — screen idle-dim / wake policy for every board (#88 §3.7 "wake").
 *
 * The panel dims after GZ_UI_DIM_AFTER_MS without activity and returns to full
 * brightness on the next one. Activity is a touch on every board, and IMU
 * motion where the board has an IMU (GZ_BOARD_HAS_IMU) and GZ_UI_WAKE_ON_MOTION
 * is 1 — pick the node up and the screen comes back. On the AMOLED this is
 * also the burn-in care from the port design (#84 §4), so it dims lower there.
 *
 * One constant each to disable (#88 §3.7): GZ_UI_DIM_AFTER_MS 0 never dims;
 * GZ_UI_WAKE_ON_MOTION 0 wakes on touch only. Both can be set from the build
 * flags. Auto-rotate is deliberately not done (see the PR): rotating LVGL's
 * partial buffers on the round panel is a render-path change that needs a
 * bench, and the round dial reads the same in any orientation.
 *
 * Pure: the policy is stdint-only and host-tested (test/test_godzilla_wake);
 * gz_ui.cpp feeds it events and applies the brightness.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gz_board.h"

#ifndef GZ_UI_DIM_AFTER_MS
#define GZ_UI_DIM_AFTER_MS    60000u   /* 0 = never dim */
#endif
#ifndef GZ_UI_WAKE_ON_MOTION
#define GZ_UI_WAKE_ON_MOTION  1        /* 0 = wake on touch only */
#endif
/* Dimmed level: lower on the AMOLED (burn-in), still readable on the LCDs. */
#define GZ_UI_DIM_PCT         ((GZ_BOARD_DISPLAY == 2) ? 8u : 15u)

typedef enum { GZ_WAKE_NO_CHANGE = 0, GZ_WAKE_TO_DIM, GZ_WAKE_TO_BRIGHT } gz_wake_action_t;

typedef struct {
    uint32_t last_ms;    /* time of the last activity */
    bool     dimmed;
} gz_wake_t;

static inline void gz_wake_init(gz_wake_t *w, uint32_t now_ms)
{
    w->last_ms = now_ms;
    w->dimmed  = false;
}

/* Note an activity (touch, motion). Brightens at the next update. */
static inline void gz_wake_activity(gz_wake_t *w, uint32_t now_ms) { w->last_ms = now_ms; }

/* Call once per UI tick. Returns what the caller should do with the panel
 * brightness; after_ms 0 never dims. Unsigned age, so millis() wrap is safe. */
static inline gz_wake_action_t gz_wake_update(gz_wake_t *w, uint32_t now_ms, uint32_t after_ms)
{
    bool idle = after_ms != 0 && (uint32_t)(now_ms - w->last_ms) >= after_ms;
    if (idle == w->dimmed) return GZ_WAKE_NO_CHANGE;
    w->dimmed = idle;
    return idle ? GZ_WAKE_TO_DIM : GZ_WAKE_TO_BRIGHT;
}
