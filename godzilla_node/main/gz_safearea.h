/*
 * gz_safearea.h — the shape-aware safe-area box for full-screen layouts
 * (watch-faces orders, unit 4).
 *
 * A screen-filling layout (the on-device UI customiser's list) must not run its
 * rows off a round panel's glass or under a rounded-rect's corner radius. This
 * gives the largest axis-aligned rectangle that stays inside the safe area:
 *   - round panel: the square inscribed in the safe circle (side = 2r/sqrt(2)),
 *     centred;
 *   - rectangular panel: the panel inset by a margin each side.
 * Lay a column of controls inside this box and nothing clips on the round
 * 1.85B or the rectangular 2.06 / ES3C28P.
 *
 * Geometry from the one source, gz_geom.h (#88 §3.2). Pure: plain int functions
 * of (w, h, round), so the host test checks all three boards without a board
 * header, like test_godzilla_geom. No LVGL, no Arduino. C and C++.
 */
#pragma once

#include <stdbool.h>
#include "gz_geom.h"   /* gz_geom_safe_r / gz_geom_min — the one geometry source */

typedef struct { int x, y, w, h; } gz_rect_t;

/* The safe-area box for a `w` x `h` panel. `margin` is the edge keep-out used on
 * a rectangular panel (and as a minimum inset on a round one). sqrt(2) ~
 * 1.41421356 is approximated by 4142/2929 (same ratio as gz_cmdface.h). */
static inline gz_rect_t gz_safe_box(int w, int h, bool round, int margin)
{
    gz_rect_t r;
    if (round) {
        int sr = gz_geom_safe_r(w, h);          /* min(w,h)/2 */
        int side = (sr * 2929) / 4142 * 2;      /* 2 * r/sqrt(2), inscribed square */
        int maxside = gz_geom_min(w, h) - 2 * margin;
        if (side > maxside) side = maxside;     /* never exceed the margin inset */
        r.w = side;
        r.h = side;
    } else {
        r.w = w - 2 * margin;
        r.h = h - 2 * margin;
    }
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    r.x = (w - r.w) / 2;                          /* centred */
    r.y = (h - r.h) / 2;
    return r;
}

/* True iff the rect [x, x+w) x [y, y+h) lies wholly inside the safe area —
 * within the safe circle on a round panel, within the margin inset otherwise.
 * Used by the native test to prove the box (and anything laid inside it) never
 * clips. */
static inline bool gz_safe_rect_inside(gz_rect_t b, int w, int h, bool round,
                                       int margin)
{
    if (b.w < 0 || b.h < 0) return false;
    if (round) {
        int cx = w / 2, cy = h / 2;
        long sr = gz_geom_safe_r(w, h);
        /* All four corners within the safe circle. */
        const long xs[2] = { b.x - cx, b.x + b.w - cx };
        const long ys[2] = { b.y - cy, b.y + b.h - cy };
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                if (xs[i] * xs[i] + ys[j] * ys[j] > sr * sr) return false;
        return true;
    }
    return b.x >= margin && b.y >= margin &&
           b.x + b.w <= w - margin && b.y + b.h <= h - margin;
}
