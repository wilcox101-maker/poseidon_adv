/*
 * gz_geom.h — the ONE source of screen geometry for the GODZILLA UI
 * (three-board port, completion orders #88 §3.2).
 *
 * Every UI module sizes itself from here instead of hard-coding 360 / 180, so
 * the same screens fit the 1.85B (360x360 round), the AMOLED-2.06 (410x502)
 * and the ES3C28P (240x320). A fourth board is just a board header.
 *
 * The screens were designed on the 1.85B: a 360 px canvas whose safe circle
 * has radius 180. Two rules carry that design to other panels:
 *   - radial parts (radar dial, watch face) scale with the SAFE RADIUS,
 *     min(W, H) / 2, so a dial always inscribes in the shorter side;
 *   - top-anchored layouts (Hunt) keep their 360-canvas y positions, shifted
 *     to sit centred on a taller or shorter panel (content_dy), with widths
 *     clamped to the panel.
 * On the 1.85B every function below returns exactly the constant it replaces
 * (pinned by test/test_godzilla_geom), so nothing moves on the reference board.
 *
 * Fonts are not scaled: Montserrat sizes stay as they are (#88 asks the UI to
 * adapt, not to shrink text below legibility).
 *
 * Pure: plain int functions of (w, h, round) so the host test checks all three
 * boards in one TU; the GZ_UI_* macros bind them to the build's board.
 */
#pragma once

#include <stdbool.h>
#include "gz_board.h"   /* GZ_LCD_WIDTH / GZ_LCD_HEIGHT / GZ_LCD_ROUND */

#define GZ_GEOM_DESIGN_R       180   /* safe radius of the 360 px design canvas */
#define GZ_GEOM_DESIGN_SIZE    360

static inline int gz_geom_min(int a, int b) { return a < b ? a : b; }

/* Largest circle centred on the panel. */
static inline int gz_geom_safe_r(int w, int h) { return gz_geom_min(w, h) / 2; }

/* Scale a design-canvas length (px at safe radius 180) to this panel,
 * rounded to nearest. Identity on the 1.85B. */
static inline int gz_geom_scale(int v, int w, int h)
{
    return (v * gz_geom_safe_r(w, h) + GZ_GEOM_DESIGN_R / 2) / GZ_GEOM_DESIGN_R;
}

/* y offset that centres a 360-tall design layout on an h-tall panel. */
static inline int gz_geom_content_dy(int h) { return (h - GZ_GEOM_DESIGN_SIZE) / 2; }

/* Width available to a centred block: the design width, or the panel less a
 * margin each side, whichever is smaller. */
static inline int gz_geom_fit_w(int design_w, int w, int margin)
{
    return gz_geom_min(design_w, w - 2 * margin);
}

/* ---- radar --------------------------------------------------------------- */
#define GZ_GEOM_RADAR_R_MIN   22   /* clear of the trident glyph (fixed size) */

/* Outer dial radius: the design's 22 px rim inset, on any panel (158 on the
 * 1.85B). */
static inline int gz_geom_radar_r_max(int w, int h) { return gz_geom_safe_r(w, h) - 22; }

/* Detail card: 236 px on the design, never wider than the panel less 4 px a
 * side, so its 2 px border stays off the glass edge (232 on the 240 px
 * ES3C28P). */
static inline int gz_geom_card_w(int w) { return gz_geom_fit_w(236, w, 4); }

/* ---- Hunt ---------------------------------------------------------------- */
/* Trace plot: 240 px inside the round glass (its chord limit); on rectangular
 * panels it takes the width less a 16 px margin a side (378 on the 2.06,
 * 208 on the ES3C28P). */
static inline int gz_geom_hunt_plot_w(int w, bool round)
{
    return round ? gz_geom_fit_w(240, w, 16) : w - 32;
}
static inline int gz_geom_hunt_plot_x(int w, bool round)
{
    return (w - gz_geom_hunt_plot_w(w, round)) / 2;
}

/* ---- text columns (Hunt labels, status screen) ---------------------------- */
static inline int gz_geom_col_w(int design_w, int w) { return gz_geom_fit_w(design_w, w, 8); }

/* ---- board bindings ------------------------------------------------------- */
#define GZ_UI_W            GZ_LCD_WIDTH
#define GZ_UI_H            GZ_LCD_HEIGHT
#define GZ_UI_ROUND        (GZ_LCD_ROUND != 0)
#define GZ_UI_CX           (GZ_LCD_WIDTH / 2)
#define GZ_UI_CY           (GZ_LCD_HEIGHT / 2)
#define GZ_UI_SAFE_R       gz_geom_safe_r(GZ_LCD_WIDTH, GZ_LCD_HEIGHT)
#define GZ_UI_S(v)         gz_geom_scale((v), GZ_LCD_WIDTH, GZ_LCD_HEIGHT)
#define GZ_UI_DY           gz_geom_content_dy(GZ_LCD_HEIGHT)
