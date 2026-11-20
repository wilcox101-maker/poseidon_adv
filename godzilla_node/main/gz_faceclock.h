/*
 * gz_faceclock.h — digital-time formatting + 2x2 slot grid for the Modular and
 * Minimal faces (watch-faces orders, unit 5).
 *
 * The pure, host-testable parts the two slotted digital faces share:
 *   - HH:MM formatting (24-hour, zero-padded) and the 12-hour split, for the
 *     Modular grid's centre time and the Minimal face's big time;
 *   - the once-a-second colon blink;
 *   - the muki-style 2x2 slot grid: four equal cells (with a gap) tiling a
 *     safe-area box, so the Modular face's pods sit on a clean grid that never
 *     clips (the box itself is the shape-safe one from gz_safearea.h).
 *
 * Pure stdint/bool, no LVGL/Arduino, C and C++. The faces feed it the
 * gz_datetime_t they already hold (gz_clock) and the safe box from
 * gz_safe_box(); native-tested in test_godzilla_faceclock.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gz_safearea.h"   /* gz_rect_t + the shape-safe box */

/* "HH:MM" (24-hour, zero-padded) into out[6] (5 chars + NUL). Hours/minutes are
 * clamped to their valid range so a glitched value never overruns. */
static inline void gz_fmt_hhmm(uint8_t hour, uint8_t minute, char out[6])
{
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    out[0] = (char)('0' + hour / 10);
    out[1] = (char)('0' + hour % 10);
    out[2] = ':';
    out[3] = (char)('0' + minute / 10);
    out[4] = (char)('0' + minute % 10);
    out[5] = '\0';
}

/* 24-hour hour -> 12-hour clock hour 1..12, with *pm set for 12:00..23:59. */
static inline uint8_t gz_hour12(uint8_t hour24, bool *pm)
{
    if (pm) *pm = (hour24 % 24) >= 12;
    uint8_t h = (uint8_t)(hour24 % 12);
    return h == 0 ? 12 : h;
}

/* The colon shows on even seconds, blinks off on odd — a 1 Hz tick. */
static inline bool gz_colon_on(uint8_t second) { return (second & 1u) == 0u; }

/* ---- 2x2 slot grid ------------------------------------------------------- */
/* Cell `i` of a 2x2 grid tiling `box`, with `gap` px between cells (and no
 * outer gap). i: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right. The
 * four cells partition the box (minus the gaps) and never exceed it, so laid
 * inside a shape-safe box nothing clips. */
static inline void gz_grid2x2_cell(gz_rect_t box, int gap, int i, gz_rect_t *cell)
{
    int cw = (box.w - gap) / 2;
    int ch = (box.h - gap) / 2;
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    int right = (i & 1) ? 1 : 0;
    int bottom = (i & 2) ? 1 : 0;
    cell->w = cw;
    cell->h = ch;
    cell->x = box.x + (right ? cw + gap : 0);
    cell->y = box.y + (bottom ? ch + gap : 0);
}
