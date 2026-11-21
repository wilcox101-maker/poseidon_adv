/*
 * gz_cust_layout.h — pure list geometry for the customiser screens (item M). PURE.
 *
 * Both the customiser's main page (gz_face_customiser.cpp) and its Alerts subpage
 * lay a column of equal-height rows inside the shape-aware safe box
 * (gz_safearea.h), with a title line at the top and — on the Alerts subpage — a
 * footer line at the bottom. The row maths was duplicated and inlined in the
 * firmware; this header is its one host-tested source:
 *   - the title / footer band height from a font line height plus a gap;
 *   - the row origin and the per-row height that divides the remaining box
 *     evenly, so every row fits and nothing is scrolled off (item M removed the
 *     dead Alerts scroll — its parents were non-clickable, so a drag could not
 *     scroll it, and all rows fit every board anyway);
 *   - the vertically-centred text y for a row;
 *   - the tap y -> row mapping, and a row's centre y (so a tap on a drawn row
 *     round-trips back to that row).
 *
 * Geometry from the one source (gz_geom.h via gz_safearea.h). The font line
 * heights are passed in (the firmware reads lv_font_get_line_height; the host
 * test passes the real Montserrat values), so this stays LVGL-free and pure:
 * plain int maths, C and C++. Native-tested in test/test_godzilla_cust_layout.
 */
#pragma once

#include "gz_safearea.h"   /* gz_rect_t + the safe box (one geometry source) */

/* A title / footer band is one font line plus a gap. */
static inline int gz_cust_band_h(int line_h, int gap)
{
    int h = line_h + gap;
    return h > 0 ? h : 0;
}

/* The laid-out list: a title band at box.y, n_rows of equal height below it, and
 * (when footer_h > 0) a footer band at the bottom of the box. */
typedef struct {
    int title_y;    /* y of the title line (top of the box)            */
    int rows_y;     /* y of the first row, below the title band        */
    int row_h;      /* per-row height (0 if nothing fits)              */
    int footer_y;   /* y of the footer band (top of it); == end of rows */
} gz_cust_list_t;

/* Lay n_rows evenly in `box` under a title band of title_h, above a footer band
 * of footer_h (footer_h = 0 for a page with no footer). The rows divide what is
 * left so they always fit — no scrolling. */
static inline gz_cust_list_t gz_cust_list(gz_rect_t box, int title_h,
                                          int footer_h, int n_rows)
{
    gz_cust_list_t L;
    L.title_y = box.y;
    L.rows_y  = box.y + title_h;
    int avail = box.h - title_h - footer_h;
    if (avail < 0) avail = 0;
    L.row_h   = (n_rows > 0) ? avail / n_rows : 0;
    L.footer_y = L.rows_y + L.row_h * n_rows;
    return L;
}

/* The y at which to place a `text_h`-tall label so it is vertically centred in
 * row `row`. */
static inline int gz_cust_row_text_y(const gz_cust_list_t *L, int row, int text_h)
{
    return L->rows_y + row * L->row_h + (L->row_h - text_h) / 2;
}

/* The centre y of row `row` (the point a tap on that row maps back from). */
static inline int gz_cust_row_center_y(const gz_cust_list_t *L, int row)
{
    return L->rows_y + row * L->row_h + L->row_h / 2;
}

/* The row a tap at (x, y) hits, or -1 if it is outside the row band or the box's
 * x-range. No scroll offset: every row is on screen. */
static inline int gz_cust_row_at(const gz_cust_list_t *L, gz_rect_t box,
                                 int x, int y, int n_rows)
{
    if (L->row_h <= 0) return -1;
    if (x < box.x || x >= box.x + box.w) return -1;
    if (y < L->rows_y || y >= L->rows_y + L->row_h * n_rows) return -1;
    int row = (y - L->rows_y) / L->row_h;
    return (row >= 0 && row < n_rows) ? row : -1;
}
