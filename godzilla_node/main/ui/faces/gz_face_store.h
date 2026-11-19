/*
 * gz_face_store.h — NVS persistence for the watch-faces settings
 * (watch-faces orders, unit 1).
 *
 * Load reads the `gzui` namespace, decodes the blob with the pure codec in
 * gz_faces.h (an absent, short or corrupt blob falls back to factory
 * defaults), then sanitises the result against THIS board's real capability
 * mask so a slot the board can't source is dropped rather than faked. Save
 * writes the current settings back. Thin on purpose: all the logic that can be
 * host-tested lives in gz_faces.h; this file only touches NVS.
 */
#pragma once

#include "gz_faces.h"

/* Fill *s from NVS (or defaults), already sanitised for this board. */
void gz_face_store_load(gz_ui_settings_t *s);

/* Persist *s to NVS. Best-effort: a begin() failure leaves NVS unchanged. */
void gz_face_store_save(const gz_ui_settings_t *s);
