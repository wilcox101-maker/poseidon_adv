/*
 * gz_slotface.h — Modular + Minimal face model (watch-faces orders, unit 5).
 *
 * The PURE half of the two slotted faces that share the Command face's slot
 * renderer:
 *   - MODULAR: a muki-style 2x2 complication-slot grid under a digital time;
 *   - MINIMAL: a big digital time with one status line.
 *
 * This header owns only the layout maths and the honest slot->complication
 * resolution — no LVGL, no Arduino — so all three boards are exercised on the
 * host (test/test_godzilla_slotface), exactly as gz_cmdface.h / gz_geom.h are.
 * The LVGL renders (ui/faces/gz_face_modular.*, gz_face_minimal.*) consume it.
 *
 * Geometry comes from the one source gz_geom.h via gz_cmdface.h, whose
 * gz_cmd_safe_half already defines the inscribed-square half-side of a round
 * panel; this header reuses it so round/rect safe-area handling stays identical
 * to the Command pods. The slot->field binding reuses gz_comp_subject from the
 * same header — one slot renderer, not two. Honest data only: a complication a
 * board can't source is dropped to GZ_COMP_NONE, never faked.
 *
 * Pure int functions of (w, h, round, margin, ...); compiles as C and C++.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gz_cmdface.h"   /* gz_comp_subject, gz_geom_safe_r, the slot model */

/* ---- content box --------------------------------------------------------- */
/* The largest axis-aligned rectangle that sits inside the panel's safe area:
 * the inscribed square (inset by `margin`) of a round panel, or the panel less
 * `margin` each side otherwise. Every slotted-face element lays out inside this
 * box, so it can never clip the glass on any board. */
static inline void gz_slot_box(int w, int h, bool round, int margin,
                               int *bx, int *by, int *bw, int *bh)
{
    if (round) {
        int r    = gz_geom_safe_r(w, h);        /* min(w,h)/2 */
        int half = (r * 2929) / 4142 - margin;  /* inscribed-square half-side */
        if (half < 0) half = 0;
        *bw = half * 2; *bh = half * 2;
        *bx = w / 2 - half; *by = h / 2 - half;
    } else {
        int mx = margin, my = margin;
        if (mx * 2 > w) mx = w / 2;
        if (my * 2 > h) my = h / 2;
        *bx = mx; *by = my;
        *bw = w - 2 * mx; *bh = h - 2 * my;
    }
}

/* True iff the rect (x, y, rw, rh) lies wholly inside the safe area: within the
 * safe circle (every corner) on a round panel, within the inset rect otherwise.
 * Mirrors gz_cmd_pod_inside for a rectangle; used by the native test to prove no
 * tile clips on any board. */
static inline bool gz_slot_rect_inside(int x, int y, int rw, int rh,
                                       int w, int h, bool round, int margin)
{
    if (round) {
        int  cx = w / 2, cy = h / 2;
        long r  = gz_geom_safe_r(w, h);         /* the glass; the box keeps clear */
        int  xs[2] = { x, x + rw }, ys[2] = { y, y + rh };
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 2; ++b) {
                long dx = xs[a] - cx, dy = ys[b] - cy;
                if (dx * dx + dy * dy > r * r) return false;
            }
        return true;
    }
    return x >= margin && y >= margin &&
           x + rw <= w - margin && y + rh <= h - margin;
}

/* ---- Modular: digital time + 2x2 slot grid ------------------------------- */
/* Fraction (percent) of the content-box height the digital time band takes; the
 * 2x2 grid fills the rest. */
#define GZ_MOD_TIME_PCT 26

/* The digital-time band: the top GZ_MOD_TIME_PCT of the content box. */
static inline void gz_mod_time_band(int w, int h, bool round, int margin,
                                    int *tx, int *ty, int *tw, int *th)
{
    int bx, by, bw, bh;
    gz_slot_box(w, h, round, margin, &bx, &by, &bw, &bh);
    *tx = bx; *ty = by; *tw = bw; *th = (bh * GZ_MOD_TIME_PCT) / 100;
}

/* Cell `i` of the 2x2 slot grid (0 top-left, 1 top-right, 2 bottom-left,
 * 3 bottom-right), laid out in the content box below the time band with `gap`
 * between tiles. Sizes are clamped non-negative; every cell lies inside the safe
 * area (proven by the native test). */
static inline void gz_mod_cell(int i, int w, int h, bool round, int margin,
                               int gap, int *cx, int *cy, int *cw, int *ch)
{
    int bx, by, bw, bh;
    gz_slot_box(w, h, round, margin, &bx, &by, &bw, &bh);
    int th = (bh * GZ_MOD_TIME_PCT) / 100;
    int gy = by + th;
    int gh = bh - th;
    int cwi = (bw - gap) / 2;
    int chi = (gh - gap) / 2;
    if (cwi < 0) cwi = 0;
    if (chi < 0) chi = 0;
    int col = i & 1, row = (i >> 1) & 1;
    *cw = cwi; *ch = chi;
    *cx = bx + col * (cwi + gap);
    *cy = gy + row * (chi + gap);
}

/* The complication Modular cell `i` shows, read from the persisted settings and
 * dropped to NONE if this board can't source it (never faked). */
static inline gz_comp_id_t gz_mod_cell_comp(const gz_ui_settings_t *s, int i,
                                            uint8_t board_caps)
{
    if (i < 0 || i >= GZ_FACE_SLOTS) return GZ_COMP_NONE;
    gz_comp_id_t c = (gz_comp_id_t)s->slots[i];
    return gz_comp_available(c, board_caps) ? c : GZ_COMP_NONE;
}

/* The field Modular cell `i` reads (binding-table shortcut, reused from the
 * Command pods). */
static inline gz_subj_field_t gz_mod_cell_field(const gz_ui_settings_t *s, int i,
                                                uint8_t board_caps)
{
    return gz_comp_subject(gz_mod_cell_comp(s, i, board_caps));
}

/* ---- Minimal: big time + one status line --------------------------------- */
#define GZ_MIN_TIME_PCT   40
#define GZ_MIN_STATUS_PCT 14
#define GZ_MIN_GAP_PCT     5

/* Lay out the Minimal face: a big digital-time rect and a single status line
 * below it, vertically centred as a block in the content box. Both rects lie
 * inside the safe area (proven by the native test). */
static inline void gz_min_layout(int w, int h, bool round, int margin,
                                 int *time_x, int *time_y, int *time_w, int *time_h,
                                 int *st_x, int *st_y, int *st_w, int *st_h)
{
    int bx, by, bw, bh;
    gz_slot_box(w, h, round, margin, &bx, &by, &bw, &bh);
    int th    = (bh * GZ_MIN_TIME_PCT)   / 100;
    int sh    = (bh * GZ_MIN_STATUS_PCT) / 100;
    int gap   = (bh * GZ_MIN_GAP_PCT)    / 100;
    int total = th + gap + sh;
    int top   = by + (bh - total) / 2;
    if (top < by) top = by;
    *time_x = bx; *time_w = bw; *time_h = th; *time_y = top;
    *st_x   = bx; *st_w   = bw; *st_h   = sh; *st_y   = top + th + gap;
}

/* The Minimal face's single status complication: the first slot this board can
 * source, else node status (GZ_COMP_STATUS, available on every board). Always
 * returns a sourceable complication — never faked. */
static inline gz_comp_id_t gz_min_status_comp(const gz_ui_settings_t *s,
                                              uint8_t board_caps)
{
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) {
        gz_comp_id_t c = (gz_comp_id_t)s->slots[i];
        if (c != GZ_COMP_NONE && gz_comp_available(c, board_caps)) return c;
    }
    return GZ_COMP_STATUS;
}
