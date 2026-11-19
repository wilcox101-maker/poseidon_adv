/*
 * gz_fmt.h — small pure text helpers for the GODZILLA UI.
 *
 * gz_plural(): the singular or plural noun for a count, so labels read
 * "1 hit" / "2 hits", "1 sample", "saved 1 row" (post-port orders #98 §1.1).
 * English only, regular plurals passed in by the caller: 0 and anything other
 * than 1 take the plural ("0 hits").
 *
 * Pure: no Arduino / LVGL; host-tested by test/test_godzilla_fmt.
 */
#pragma once

static inline const char *gz_plural(unsigned long n, const char *one, const char *many)
{
    return (n == 1ul) ? one : many;
}
