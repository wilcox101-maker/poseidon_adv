/*
 * gz_ui_apply.h — pure decision core for applying a customiser save live
 * (watch-faces, unit 4 / item I). PURE.
 *
 * The customiser persists the WHOLE settings blob (gz_ui_settings_t) on every
 * tap. Most saves change one field, and only two of those fields have a live
 * consumer in the UI task today:
 *   - the Classic case finish  -> gz_ui_watch_set_finish + a watch rebuild
 *     (the costly one: it deletes and re-lays the whole dial);
 *   - the alert overlay's per-family mask and always-on -> two gz_ui.cpp
 *     statics the overlay tick reads.
 * This header turns "old settings vs just-saved settings" into exactly which of
 * those consumers must re-apply, so gz_ui.cpp runs the watch rebuild only when
 * the finish really changed and never on an unrelated save (an accent tap, a
 * pod swap). Accent / background / face re-apply is not wired as live here, so
 * they are intentionally not consumers of this diff.
 *
 * Pure: stdint/bool only; compiles as C and C++. Native-tested in
 * test_godzilla_ui_apply.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ui/faces/gz_faces.h"   /* gz_ui_settings_t */

/* Which live consumers a settings change touches. */
typedef struct {
    bool finish;   /* Classic case metal -> set_finish + rebuild (RTC boards) */
    bool alert;    /* alert mask or always-on -> gz_ui.cpp overlay statics     */
} gz_ui_consumers_t;

/* What changed between the last-applied settings `prev` and the just-saved
 * `next`, limited to the fields with a live consumer. */
static inline gz_ui_consumers_t gz_ui_consumers_diff(const gz_ui_settings_t *prev,
                                                     const gz_ui_settings_t *next)
{
    gz_ui_consumers_t c;
    c.finish = (prev->finish != next->finish);
    c.alert  = (prev->alert_mask != next->alert_mask) ||
               (prev->always_on  != next->always_on);
    return c;
}

/* True iff any live consumer must re-apply (lets the caller skip all work on a
 * save that touched only non-live fields). */
static inline bool gz_ui_consumers_any(gz_ui_consumers_t c)
{
    return c.finish || c.alert;
}
