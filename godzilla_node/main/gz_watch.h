/*
 * gz_watch.h — pocket-watch model for the GODZILLA node (P2c).
 *
 * The pure half of the watch face (docs/proposals/2026-11-18-godzilla-p2-orders.md
 * §P2c, advice in 2026-11-19-godzilla-p2c-watch-advice.md):
 *
 *   - hand angles in tenths of a degree (LVGL's unit), 0 = 12 o'clock, clockwise;
 *   - a software clock: the PCF85063 is read once and then advanced from
 *     millis() with wrap-safe unsigned deltas, so the face never reads I2C per
 *     frame. The UI re-syncs it from the RTC about once a minute;
 *   - calendar rollover through gz_rtc.h's one calendar (gz_rtc_days_in_month);
 *   - polar placement for hands, ticks and numerals;
 *   - the set-mode state machine: long-press enters, a tap on the upper half
 *     is hour +1, on the lower half minute +1 (seconds zeroed, no carry — each
 *     field is set on its own, like pulling a crown), long-press again commits,
 *     15 s without input leaves without writing;
 *   - Roman numerals, with the traditional dial "IIII" for four.
 *
 * Pure: stdint/math + gz_rtc.h's pure half. No Arduino, no LVGL. Compiles as C
 * and C++; pinned by test/test_godzilla_watch.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "gz_rtc.h"     /* gz_datetime_t, gz_rtc_days_in_month, gz_rtc_valid */

#define GZ_WATCH_SET_TIMEOUT_MS 15000u   /* set mode: leave after 15 s idle  */
#define GZ_WATCH_RESYNC_MS      60000u   /* re-read the RTC about once a min */

/* ---- hand angles (tenths of a degree) ------------------------------------ */

static inline int32_t gz_watch_second_angle(uint8_t s) { return (int32_t)s * 60; }

static inline int32_t gz_watch_minute_angle(uint8_t m, uint8_t s)
{
    return (int32_t)m * 60 + (int32_t)s;            /* 6 deg/min + 0.1 deg/s */
}

static inline int32_t gz_watch_hour_angle(uint8_t h, uint8_t m)
{
    return (int32_t)(h % 12u) * 300 + (int32_t)m * 5;   /* 30 deg/h + 0.5 deg/min */
}

/* ---- polar placement ----------------------------------------------------- */

/* Point at `radius` px along `angle_tenths` from (cx, cy). 0 = up, clockwise,
 * y grows downward. */
static inline void gz_watch_polar(int32_t angle_tenths, int radius, int cx, int cy,
                                  int *x, int *y)
{
    const float a = (float)angle_tenths * 0.0017453292519943f;
    *x = cx + (int)lroundf((float)radius * sinf(a));
    *y = cy - (int)lroundf((float)radius * cosf(a));
}

/* A hand: from a short counter-tail behind the pivot to the tip. */
static inline void gz_watch_hand(int32_t angle_tenths, int len, int tail,
                                 int cx, int cy,
                                 int *x0, int *y0, int *x1, int *y1)
{
    gz_watch_polar(angle_tenths + 1800, tail, cx, cy, x0, y0);
    gz_watch_polar(angle_tenths, len, cx, cy, x1, y1);
}

/* ---- calendar advance ---------------------------------------------------- */

/* *dt advanced by elapsed_ms (whole seconds; the remainder is dropped). Rolls
 * seconds -> minutes -> hours -> day -> month -> year and keeps the weekday in
 * step. The year saturates at 2099-12-31 23:59:59 (the RTC's range). */
static inline void gz_watch_advance(gz_datetime_t *dt, uint32_t elapsed_ms)
{
    uint32_t sod = (uint32_t)dt->hour * 3600u + (uint32_t)dt->minute * 60u +
                   (uint32_t)dt->second + elapsed_ms / 1000u;
    uint32_t days = sod / 86400u;
    sod %= 86400u;
    for (; days > 0; --days) {
        if (dt->day < gz_rtc_days_in_month(dt->year, dt->month)) {
            dt->day++;
        } else if (dt->month < 12) {
            dt->month++;
            dt->day = 1;
        } else if (dt->year < 2099) {
            dt->year++;
            dt->month = 1;
            dt->day = 1;
        } else {
            sod = 86399u;                           /* saturate */
            break;
        }
        dt->weekday = (uint8_t)((dt->weekday + 1u) % 7u);
    }
    dt->hour   = (uint8_t)(sod / 3600u);
    dt->minute = (uint8_t)((sod % 3600u) / 60u);
    dt->second = (uint8_t)(sod % 60u);
}

/* ---- software clock ------------------------------------------------------ */

typedef struct {
    gz_datetime_t base;      /* wall time at base_ms          */
    uint32_t      base_ms;   /* millis() when base was taken  */
    bool          synced;    /* base came from somewhere real */
} gz_watch_clock_t;

static inline void gz_watch_sync(gz_watch_clock_t *c, const gz_datetime_t *dt,
                                 uint32_t now_ms)
{
    c->base    = *dt;
    c->base_ms = now_ms;
    c->synced  = true;
}

/* Current time from the software clock. Unsigned delta: right across the
 * millis() wrap as long as syncs are < 49 days apart. */
static inline void gz_watch_now(const gz_watch_clock_t *c, uint32_t now_ms,
                                gz_datetime_t *out)
{
    *out = c->base;
    gz_watch_advance(out, (uint32_t)(now_ms - c->base_ms));
}

static inline bool gz_watch_resync_due(const gz_watch_clock_t *c, uint32_t now_ms)
{
    return !c->synced || (uint32_t)(now_ms - c->base_ms) >= GZ_WATCH_RESYNC_MS;
}

/* ---- set mode ------------------------------------------------------------ */

enum { GZ_WATCH_FIELD_HOUR = 0, GZ_WATCH_FIELD_MINUTE = 1 };

typedef struct {
    bool          active;
    gz_datetime_t edit;          /* the time being set (seconds held at 0) */
    uint8_t       field;         /* GZ_WATCH_FIELD_*: last one changed      */
    uint32_t      last_input_ms;
} gz_watch_set_t;

/* Long-press while running: start editing from the current time. */
static inline void gz_watch_set_enter(gz_watch_set_t *s, const gz_datetime_t *now,
                                      uint32_t now_ms)
{
    s->active        = true;
    s->edit          = *now;
    s->field         = GZ_WATCH_FIELD_HOUR;
    s->last_input_ms = now_ms;
}

/* A tap at screen row `y` on a dial centred at row `cy`: upper half = hour +1,
 * lower half = minute +1. Each wraps on its own; seconds are zeroed. */
static inline void gz_watch_set_tap(gz_watch_set_t *s, int y, int cy, uint32_t now_ms)
{
    if (!s->active) return;
    if (y < cy) {
        s->edit.hour = (uint8_t)((s->edit.hour + 1u) % 24u);
        s->field = GZ_WATCH_FIELD_HOUR;
    } else {
        s->edit.minute = (uint8_t)((s->edit.minute + 1u) % 60u);
        s->field = GZ_WATCH_FIELD_MINUTE;
    }
    s->edit.second   = 0;
    s->last_input_ms = now_ms;
}

static inline bool gz_watch_set_timed_out(const gz_watch_set_t *s, uint32_t now_ms)
{
    return s->active && (uint32_t)(now_ms - s->last_input_ms) >= GZ_WATCH_SET_TIMEOUT_MS;
}

/* Long-press while setting: hand back the time to write and leave set mode. */
static inline void gz_watch_set_commit(gz_watch_set_t *s, gz_datetime_t *out)
{
    *out = s->edit;
    out->second = 0;
    out->osc_stopped = false;
    s->active = false;
}

static inline void gz_watch_set_cancel(gz_watch_set_t *s) { s->active = false; }

/* ---- numerals ------------------------------------------------------------ */

/* Roman numeral for dial hour 1..12 (traditional "IIII"); "" otherwise. */
static inline const char *gz_watch_roman(uint8_t hour)
{
    static const char *const r[13] = { "", "I", "II", "III", "IIII", "V", "VI",
                                       "VII", "VIII", "IX", "X", "XI", "XII" };
    return hour <= 12 ? r[hour] : "";
}
