/*
 * gz_timeset.h — pure epoch -> civil time conversion for the CMD_TIMESET
 * consumer (time-sync node consumer, step 3; design 2026-11-19-time-sync).
 *
 * The Cardputer pushes the node its wall clock as one additive downlink
 * (POSEI_TYPE_CMD_TIMESET, proto.h: a UTC epoch + a display-only tz offset).
 * This header is the PURE half the node consumer needs:
 *   - the epoch validity floor, identical to the sender's, so a bogus / unset
 *     clock is rejected the same way on both ends;
 *   - UNIX epoch (+ tz, minutes east of UTC, display only) -> civil
 *     gz_datetime_t (local Y/M/D h:m:s + weekday), by Howard Hinnant's integer
 *     days<->civil algorithm — no lookup tables, correct across and beyond the
 *     RTC's 2000..2099 range.
 *
 * Pure: stdint/stdbool + gz_rtc.h's pure calendar (gz_rtc_weekday / _valid), no
 * LVGL, no ESP-IDF, no Wire. The node's link callback validates with
 * gz_timeset_valid(); the UI (RTC-owning) task converts with
 * gz_timeset_to_datetime() and sets the hardware RTC. Native-tested in
 * test/test_godzilla_timeset. Compiles as C and C++.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_rtc.h"   /* gz_datetime_t, gz_rtc_weekday, gz_rtc_valid */

/* Epoch validity floor — the SAME value the sender uses (C5_TIME_VALID_MIN in
 * src/c5_timesync.h): 1700000000 = 2023-11-14 UTC, comfortably before this
 * project and after any plausible boot epoch. Below it, a clock is boot-
 * relative, not real UTC, so the frame is rejected. Kept in step with the
 * sender by this note; the wire struct (proto.h) is parity-pinned, this bound
 * is not, so the two definitions must match. */
#define GZ_TIMESET_EPOCH_MIN 1700000000u

/* True iff `epoch` is a real UTC wall clock (past the floor above). */
static inline bool gz_timeset_valid(uint32_t epoch)
{
    return epoch >= GZ_TIMESET_EPOCH_MIN;
}

/* Convert a UNIX epoch (UTC seconds) plus a display-only tz offset (minutes
 * east of UTC) to a civil gz_datetime_t: local Y/M/D h:m:s with the weekday
 * filled from gz_rtc_weekday(). osc_stopped is cleared. Pure 64-bit integer
 * arithmetic (Hinnant days<->civil); the tz shift is clamped into a valid
 * second-of-day so a negative offset rolls the date back cleanly. */
static inline void gz_timeset_to_datetime(uint32_t epoch, int16_t tz_offset_min,
                                          gz_datetime_t *out)
{
    int64_t local = (int64_t)epoch + (int64_t)tz_offset_min * 60;
    int64_t days  = local / 86400;
    int64_t secs  = local % 86400;
    if (secs < 0) { secs += 86400; days -= 1; }   /* floor-divide for negatives */

    /* days since 1970-01-01 -> civil date (Howard Hinnant, public domain). */
    int64_t z   = days + 719468;                   /* shift epoch to 0000-03-01 */
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;                             /* [0, 146096]  */
    int64_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365; /* [0, 399]  */
    int64_t y   = yoe + era * 400;
    int64_t doy = doe - (365*yoe + yoe/4 - yoe/100);            /* [0, 365]     */
    int64_t mp  = (5*doy + 2) / 153;                            /* [0, 11]      */
    int64_t d   = doy - (153*mp + 2)/5 + 1;                     /* [1, 31]      */
    int64_t m   = mp < 10 ? mp + 3 : mp - 9;                    /* [1, 12]      */
    y += (m <= 2);

    out->year    = (uint16_t)y;
    out->month   = (uint8_t)m;
    out->day     = (uint8_t)d;
    out->hour    = (uint8_t)(secs / 3600);
    out->minute  = (uint8_t)((secs % 3600) / 60);
    out->second  = (uint8_t)(secs % 60);
    out->weekday = gz_rtc_weekday(out->year, out->month, out->day);
    out->osc_stopped = false;
}
