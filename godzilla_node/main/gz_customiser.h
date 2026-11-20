/*
 * gz_customiser.h — pure model for the on-device UI customiser
 * (watch-faces orders, unit 4).
 *
 * The long-press settings screen lets the wearer pick the face, an accent
 * (x9), the background, the always-on toggle, and a complication per pod. The
 * pure, host-testable part:
 *   - the slot-availability filter: the per-slot complication picker offers
 *     only complications THIS board can source (plus the empty slot), so a
 *     wearer can never assign a faked pod;
 *   - the face picker's option list: the faces that fit this board;
 *   - small cyclers that step a setting with wrap, used by the touch handler.
 * The safe-area layout box the screen lays its rows in is gz_safearea.h.
 *
 * Builds on the unit-1 capability model (ui/faces/gz_faces.h) and persists
 * through the unit-1 NVS writer (gz_face_store.h); nothing here touches NVS or
 * LVGL. Pure stdint/bool, C and C++; native-tested in test_godzilla_customiser.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/faces/gz_faces.h"   /* gz_comp/gz_face enums + availability */
#include "gz_safearea.h"         /* the layout box (re-exported for the screen) */

/* ---- option lists -------------------------------------------------------- */
/* Fill `out` with the complications the slot picker may offer on this board:
 * the empty slot first, then every complication the board can source, in id
 * order. Returns the count. `out` must hold GZ_COMP_COUNT entries. */
static inline int gz_cust_comp_options(uint8_t board_caps, gz_comp_id_t *out)
{
    int n = 0;
    out[n++] = GZ_COMP_NONE;
    for (int c = GZ_COMP_NONE + 1; c < GZ_COMP_COUNT; ++c)
        if (gz_comp_available((gz_comp_id_t)c, board_caps))
            out[n++] = (gz_comp_id_t)c;
    return n;
}

/* Fill `out` with the faces that fit this board, in id order. Returns the
 * count. `out` must hold GZ_FACE_COUNT entries. */
static inline int gz_cust_face_options(uint8_t board_caps, gz_face_id_t *out)
{
    int n = 0;
    for (int f = 0; f < GZ_FACE_COUNT; ++f)
        if (gz_face_available((gz_face_id_t)f, board_caps))
            out[n++] = (gz_face_id_t)f;
    return n;
}

/* True iff the slot picker may offer complication `c` on this board. */
static inline bool gz_cust_comp_offered(gz_comp_id_t c, uint8_t board_caps)
{
    return gz_comp_available(c, board_caps);   /* NONE is always available */
}

/* ---- cyclers (wrap) ------------------------------------------------------ */
/* Step `cur` by `dir` (+1 / -1) within [0, count), wrapping. For the accent
 * (count GZ_ACCENT_COUNT), background (GZ_BG_COUNT) and any fixed ring. */
static inline int gz_cust_cycle(int cur, int dir, int count)
{
    if (count <= 0) return 0;
    int v = (cur + dir) % count;
    if (v < 0) v += count;
    return v;
}

/* Step a pod to the next/prev complication the board can source, over the
 * option list (so an unavailable complication is skipped, never landed on). */
static inline gz_comp_id_t gz_cust_cycle_comp(gz_comp_id_t cur, int dir,
                                              uint8_t board_caps)
{
    gz_comp_id_t opt[GZ_COMP_COUNT];
    int n = gz_cust_comp_options(board_caps, opt);
    int at = 0;
    for (int i = 0; i < n; ++i) if (opt[i] == cur) { at = i; break; }
    return opt[gz_cust_cycle(at, dir, n)];
}

/* Step the face to the next/prev one that fits the board. */
static inline gz_face_id_t gz_cust_cycle_face(gz_face_id_t cur, int dir,
                                              uint8_t board_caps)
{
    gz_face_id_t opt[GZ_FACE_COUNT];
    int n = gz_cust_face_options(board_caps, opt);
    int at = 0;
    for (int i = 0; i < n; ++i) if (opt[i] == cur) { at = i; break; }
    return opt[gz_cust_cycle(at, dir, n)];
}
