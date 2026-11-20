/*
 * gz_face_modular.h — the Modular watch face (watch-faces orders, unit 5).
 *
 * A self-contained face unit over the common face interface: a digital time
 * band across the top and a muki-style 2x2 grid of complication tiles below it.
 * The four tiles read the persisted per-slot complication choices (gz_face_store
 * / gz_faces.h), each dropped to an empty tile if this board can't source it —
 * never faked. The tiles are the same slot renderer the Command pods use
 * (gz_slot_render.*); the layout maths is the native-tested pure model
 * gz_slotface.h.
 *
 * Firmware-only (LVGL). The digital time shows only once the clock has a time
 * (gz_clock); a board without an RTC shows "--:--". [render] — display/geometry;
 * Claudius re-renders on the three boards.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

/* Build the Modular face onto `parent` (a screen or container). */
void gz_face_modular_build(lv_obj_t *parent);

/* Refresh the digital time and the tile values from the live subjects. */
void gz_face_modular_refresh(uint32_t now_ms);
