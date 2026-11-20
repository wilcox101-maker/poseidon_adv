/*
 * gz_face_minimal.h — Minimal watch face (watch-faces orders, unit 5).
 *
 * A big-time minimal face: the HH:MM digital time centred large, and one status
 * line beneath it bound to the wearer's first pod complication (unit-1
 * settings). Reuses the slot value renderer (gz_face_slot) and the pure time
 * format (gz_faceclock). Firmware-only (LVGL); [render] — Claudius re-renders.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>

void gz_face_minimal_build(lv_obj_t *parent);
void gz_face_minimal_refresh(uint32_t now_ms);
