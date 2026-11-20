/*
 * gz_slot_render.h — the shared slot renderer for the slotted watch faces
 * (watch-faces orders, unit 5).
 *
 * The Command pods, the Modular grid tiles and the Minimal status line all show
 * the same thing: a complication's short title and its live value read from an
 * lv_subject. This file is that one renderer's firmware helpers — the title
 * string, the subject lookup, and the value formatter — so Modular and Minimal
 * draw their tiles exactly as the Command pods do, from the one pure model in
 * gz_slotface.h / gz_cmdface.h. Honest values only: a field with no live subject
 * formats as "--".
 *
 * Firmware-only (LVGL). Pure layout/binding maths stays in the native-tested
 * headers; nothing here is testable on the host, so there is nothing to test.
 */
#pragma once

#include <lvgl.h>
#include <stddef.h>
#include "../../gz_cmdface.h"   /* gz_comp_id_t, gz_subj_field_t */

/* Short upper-case title for a complication (e.g. "NODE", "CHAN"); "" for an
 * empty slot. */
const char *gz_slot_comp_title(gz_comp_id_t c);

/* The live subject a field reads, or nullptr for GZ_SUBJ_NONE. */
lv_subject_t *gz_slot_field_subject(gz_subj_field_t f);

/* Format a field's current value into out[n] (e.g. "128k", "7", "42%", "--"). */
void gz_slot_field_text(gz_subj_field_t f, char *out, size_t n);
