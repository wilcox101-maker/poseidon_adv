/*
 * gz_face_minimal.h — the Minimal watch face (watch-faces orders, unit 5).
 *
 * A self-contained face unit over the common face interface: a big digital time
 * centred on the panel with one status line beneath it. The status line is the
 * first complication the board can source among the persisted slots (else node
 * status), drawn with the same slot renderer the Command pods use
 * (gz_slot_render.*); the layout maths is the native-tested pure model
 * gz_slotface.h. Honest data only — the status line never shows a field the
 * board can't source.
 *
 * Firmware-only (LVGL). The time shows "--:--" until the clock has a time
 * (gz_clock). [render] — display/geometry; Claudius re-renders on the boards.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

/* Build the Minimal face onto `parent` (a screen or container). */
void gz_face_minimal_build(lv_obj_t *parent);

/* Refresh the time and the status line from the live subjects. */
void gz_face_minimal_refresh(uint32_t now_ms);
