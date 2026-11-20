/*
 * gz_face_slot.h — shared pod/slot value helper for the slotted faces
 * (watch-faces orders, unit 5).
 *
 * The Modular and Minimal faces (and conceptually the Command pods) render a
 * complication's live value from the unit-1 subjects. This one place maps a
 * field id (gz_cmdface.h) to its lv_subject and formats its current value, so
 * the two faces read the node state identically and honestly.
 *
 * Firmware-only (LVGL + the subjects). Pure selection/formatting logic for the
 * fields themselves is covered by gz_cmdface.h's native test; this is the thin
 * LVGL-side binding.
 */
#pragma once

#include <lvgl.h>
#include <stddef.h>
#include "../../gz_cmdface.h"   /* gz_subj_field_t, gz_comp_subject */

/* The subject backing a field id, or nullptr for GZ_SUBJ_NONE. */
lv_subject_t *gz_face_subject(gz_subj_field_t f);

/* Short human text for a field's current value into out[n]. */
void gz_face_field_text(gz_subj_field_t f, char *out, size_t n);

/* Short title for a complication (e.g. "Contacts"). */
const char *gz_face_comp_title(gz_comp_id_t c);
