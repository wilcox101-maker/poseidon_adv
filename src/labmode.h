/*
 * labmode — guided educational exercises ("lab mode").
 *
 * PUIE owns the surface (src/features/lab_mode.cpp); this model holds
 * lesson/step state. A lesson is an ordered list of steps; each step is
 * keyed to a lab_event_t so features can self-complete it. PSPE's seam is
 * labmode_notify(ev): a feature calls it (on loopTask) when the real thing
 * happens — e.g. the handshake-capture path calls
 * labmode_notify(LAB_EV_HANDSHAKE_CAPTURED). Until those events are wired
 * (and as a fallback) the user marks a step done manually in the UI.
 *
 * CONTENT NOTE: the bundled lessons are PLACEHOLDERS to exercise the UI —
 * the real curriculum is a product decision, still TBD.
 *
 * State is session-scoped (RAM only); loopTask access.
 */
#pragma once

#include <Arduino.h>

#define LAB_MAX_STEPS 8

/* Events a feature can raise to satisfy a step (the PSPE seam). Extend as
 * features gain reporting; each lesson step is keyed to one of these. */
typedef enum {
    LAB_EV_NONE = 0,
    LAB_EV_SCAN_STARTED,
    LAB_EV_AP_SELECTED,
    LAB_EV_HANDSHAKE_CAPTURED,
    LAB_EV_PMKID_CAPTURED,
    LAB_EV_SAVED_TO_SD,
    LAB_EV__COUNT
} lab_event_t;

/* Lessons. */
int         labmode_lesson_count(void);
void        labmode_select(int lesson);        /* clamps; resets step progress */
int         labmode_current(void);
const char *labmode_lesson_title(int lesson);

/* Active-lesson steps. */
int         labmode_step_count(void);
const char *labmode_step_text(int i);
bool        labmode_step_done(int i);
int         labmode_steps_done(void);          /* for the progress bar */

/* Mark a step satisfied — manually (UI) or via a feature event. */
void        labmode_mark(int i, bool done);
/* Satisfy the first not-yet-done step whose trigger matches ev. Call from a
 * feature (loopTask) when the real event happens. Returns true if a step
 * flipped. Safe to call when lab mode is not open — it just updates state. */
bool        labmode_notify(lab_event_t ev);
void        labmode_reset(void);               /* clear all step progress */
