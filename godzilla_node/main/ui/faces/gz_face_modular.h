/*
 * gz_face_modular.h — Modular watch face (watch-faces orders, unit 5).
 *
 * A muki-style 2x2 slot grid over a large digital time: the four pods render
 * the wearer's per-slot complication choices (unit-1 settings, set in the
 * unit-4 customiser), each bound to its live subject; the centre/top shows the
 * HH:MM time with a 1 Hz colon blink. Grid cells tile the shape-safe box
 * (gz_faceclock.h / gz_safearea.h) so nothing clips on a round or rect panel.
 *
 * Firmware-only (LVGL). Pure grid/time logic is native-tested in
 * test_godzilla_faceclock; honest slot bindings reuse gz_face_slot. The hands-
 * free digital time shows only once the clock has a time (gz_clock). [render]
 * — Claudius re-renders on the three boards.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

void gz_face_modular_build(lv_obj_t *parent);
void gz_face_modular_refresh(uint32_t now_ms);
