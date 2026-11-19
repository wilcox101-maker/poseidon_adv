/*
 * gz_face_command.h — the Command watch face (watch-faces orders, unit 3).
 *
 * A self-contained face unit over the common face interface: a central
 * detection-radar dial with analog hour/minute hands and a slow sweep, four
 * complication pods bound to the live node subjects (gz_face_subjects.h), a
 * feature-shortcut quadrant tap map, and a bottom status ticker.
 *
 * Pure model (pod bindings, round-safe pod geometry, the quadrant->feature map)
 * is gz_cmdface.h, native-tested in test/test_godzilla_cmdface. This file is the
 * LVGL render that consumes it; it draws no device dots itself — the live radar
 * dots come from the existing radar renderer when Claudius integrates the face
 * onto the screen ring. Honest bindings only: no GPS, no bearing/DOA.
 *
 * Firmware-only (LVGL). Compiled on every godzilla env; the hands show only once
 * the clock has a time (gz_clock), so a board without an RTC shows the radar and
 * pods and no hands. [render] — display/geometry; Claudius re-renders.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>
#include "../../gz_cmdface.h"   /* gz_cmd_feature_t and the pure model */

/* Build the Command face onto `parent` (a screen or container). */
void gz_face_command_build(lv_obj_t *parent);

/* Refresh the sweep, hands and pod values from the live subjects. Cheap when
 * nothing changed. */
void gz_face_command_refresh(uint32_t now_ms);

/* Map a tap to the feature its quadrant opens (radar / hunt / BLE-lab / status),
 * for the caller to route to the screen ring. */
gz_cmd_feature_t gz_face_command_tap(int x, int y);
