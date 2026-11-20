/*
 * gz_face_customiser.h — the on-device UI customiser screen
 * (watch-faces orders, unit 4).
 *
 * The settings screen a long-press on a face opens: a column of rows laid
 * inside the shape safe-area box (gz_safearea.h) so nothing clips on a round or
 * rectangular panel — face picker, accent (x9), background (black / texture),
 * always-on, and a complication per pod. A tap on a row steps that setting with
 * the pure cyclers in gz_customiser.h (the pod picker offers only board-
 * sourceable complications) and persists the result through the unit-1 NVS
 * writer (gz_face_store.h). No face/complication a board can't source is ever
 * offered.
 *
 * Firmware-only (LVGL). The pure model (options, cyclers, safe-area box) is
 * native-tested in test_godzilla_customiser; this file is the [render] — the
 * three-board render pass is Claudius's. Routing the long-press from a face
 * into this screen joins with the face dispatcher.
 */
#pragma once

#include <lvgl.h>
#include "gz_faces.h"           /* gz_ui_settings_t (same dir: ui/faces/) */

/* Build the customiser onto `parent`, loading the saved settings. */
void gz_face_customiser_build(lv_obj_t *parent);

/* Reload the saved settings and redraw every row (call when the screen opens). */
void gz_face_customiser_show(void);

/* Route a tap at (x, y): the row it hits steps that setting and is persisted. */
void gz_face_customiser_tap(int x, int y);

/* The settings currently shown/edited (for the face dispatcher to apply). */
const gz_ui_settings_t *gz_face_customiser_settings(void);
