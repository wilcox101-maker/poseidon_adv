/*
 * gz_accent.h — shared accent palette + polish primitives for the watch faces
 * (watch-faces orders, unit 6).
 *
 * The pure, one-source polish toolkit every face draws from, so the accent a
 * wearer picks (unit-4 customiser, index 0..8) looks the same on every face,
 * the always-on variant dims consistently, and a face change fades in with one
 * timing curve:
 *   - gz_accent_rgb(idx): the nine accent colours (the single source — the
 *     customiser's swatches and each face's accent elements read this);
 *   - gz_dim_rgb(rgb, pct): scale a colour's brightness to `pct` percent, for
 *     the always-on dimmed variant (0 -> black, 100 -> unchanged);
 *   - gz_fade_opa(elapsed, dur): a 0..255 opacity ramp over a face-change
 *     transition, clamped at both ends.
 *
 * Pure stdint, no LVGL/Arduino (colours are 0xRRGGBB ints the faces pass to
 * lv_color_hex), C and C++. Native-tested in test_godzilla_accent.
 */
#pragma once

#include <stdint.h>

#define GZ_ACCENT_N 9   /* matches GZ_ACCENT_COUNT in ui/faces/gz_faces.h */

/* The nine accent colours, 0xRRGGBB. idx is taken modulo the count, so a
 * sanitised-but-stray value never indexes out of range. */
static inline uint32_t gz_accent_rgb(int idx)
{
    static const uint32_t ACCENT[GZ_ACCENT_N] = {
        0x2EC4B6, 0xFFB020, 0xFF4D5A, 0x8A6DF0, 0x3FA7FF,
        0x58D68D, 0xF06292, 0xC9A227, 0xBDEBEB,
    };
    idx %= GZ_ACCENT_N;
    if (idx < 0) idx += GZ_ACCENT_N;
    return ACCENT[idx];
}

/* Scale each channel of 0xRRGGBB to `pct` percent brightness (clamped 0..100).
 * The always-on dimmed variant uses this so every face dims the same way. */
static inline uint32_t gz_dim_rgb(uint32_t rgb, int pct)
{
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    r = r * (uint32_t)pct / 100u;
    g = g * (uint32_t)pct / 100u;
    b = b * (uint32_t)pct / 100u;
    return (r << 16) | (g << 8) | b;
}

/* A 0..255 opacity ramp across a transition of `dur_ms`: 0 at the start, 255 at
 * and beyond `dur_ms` (and if dur is 0). A light face-change fade uses this. */
static inline uint8_t gz_fade_opa(uint32_t elapsed_ms, uint32_t dur_ms)
{
    if (dur_ms == 0u || elapsed_ms >= dur_ms) return 255u;
    return (uint8_t)((uint32_t)255u * elapsed_ms / dur_ms);
}
