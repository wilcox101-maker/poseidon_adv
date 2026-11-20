/*
 * gz_moon.h — moon phase + printed sub-scale math for the Classic face
 * (watch-faces orders, unit 2).
 *
 * The PURE half of the antique dial's four printed sub-scales:
 *   - the twin-aperture moon-phase: a date -> synodic age in days -> an
 *     eight-bucket phase, an illuminated fraction (0 new .. 1 full) and the
 *     waxing/waning sense, plus the aperture travel fraction a twin-moon wheel
 *     turns through in one lunation;
 *   - the month, day-of-week and date rings: the angle (LVGL tenths of a
 *     degree, 0 = 12 o'clock, clockwise) of a value printed on an N-division
 *     ring, so each scale's numerals and the pointer that reads them agree.
 *
 * Moon age is computed from an integer Julian Day Number (Fliegel-Van Flandern)
 * measured from the 2000-01-06 new moon, reduced modulo the synodic month. Date
 * granularity (midnight) is plenty for a printed phase window; known new and
 * full moons land in buckets 0 and 4 (pinned by test/test_godzilla_moon).
 *
 * Pure: <stdint.h>/<math.h> only, no Arduino, no LVGL, no RTC driver — it takes
 * y/m/d integers, so the host test checks every date without a clock. The
 * Classic face (gz_ui_watch.cpp) feeds it the gz_datetime_t it already holds.
 * Compiles as C and C++. The whole moon/date display is gated behind
 * GZ_BOARD_HAS_RTC at the face; a board without an RTC shows none of it.
 */
#pragma once

#include <stdint.h>
#include <math.h>

/* Mean synodic month (new moon to new moon), days. */
#define GZ_MOON_SYNODIC    29.530588853
/* JDN of the 2000-01-06 new moon reference (Fliegel-Van Flandern, midnight). */
#define GZ_MOON_REF_JDN    2451550L
/* Eight printed phase buckets: 0 new .. 4 full .. 7 waning crescent. */
#define GZ_MOON_BUCKETS    8

/* ---- Julian Day Number --------------------------------------------------- */
/* Integer JDN of a proleptic-Gregorian calendar date at midnight. Pure integer
 * arithmetic, valid across the RTC's 2000-2099 range (and well beyond). */
static inline long gz_jdn(int year, int month, int day)
{
    int a = (14 - month) / 12;
    long y = (long)year + 4800 - a;
    int m = month + 12 * a - 3;
    return (long)day + (153L * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045L;
}

/* ---- moon age ------------------------------------------------------------ */
/* Synodic age in days, 0 .. <29.53, for the given date. 0 is new moon. */
static inline double gz_moon_age(int year, int month, int day)
{
    double age = fmod((double)(gz_jdn(year, month, day) - GZ_MOON_REF_JDN), GZ_MOON_SYNODIC);
    if (age < 0.0) age += GZ_MOON_SYNODIC;
    return age;
}

/* Phase bucket 0..7 for an age: round(age / synodic * 8) wrapped, so new moon
 * (age ~0 or ~29.5) is bucket 0 and full moon (~14.77) is bucket 4. */
static inline int gz_moon_bucket_age(double age)
{
    int b = (int)floor(age / GZ_MOON_SYNODIC * (double)GZ_MOON_BUCKETS + 0.5);
    b %= GZ_MOON_BUCKETS;
    if (b < 0) b += GZ_MOON_BUCKETS;
    return b;
}

static inline int gz_moon_bucket(int year, int month, int day)
{
    return gz_moon_bucket_age(gz_moon_age(year, month, day));
}

/* Illuminated fraction 0 (new) .. 1 (full) for an age. */
static inline double gz_moon_illum_age(double age)
{
    return (1.0 - cos(2.0 * M_PI * age / GZ_MOON_SYNODIC)) / 2.0;
}

static inline double gz_moon_illum(int year, int month, int day)
{
    return gz_moon_illum_age(gz_moon_age(year, month, day));
}

/* True while the moon is waxing (age before full): the lit limb is on the
 * right. False from full to the next new moon (lit limb on the left). */
static inline bool gz_moon_waxing_age(double age)
{
    return age < (GZ_MOON_SYNODIC / 2.0);
}

/* Fraction 0 .. <1 of the lunation the twin-moon aperture wheel has turned
 * through: one full turn carries two moons across the window per synodic month,
 * so a moon is centred in the aperture at new and at full. */
static inline double gz_moon_aperture_frac(double age)
{
    return age / GZ_MOON_SYNODIC;
}

/* ---- printed sub-scale rings --------------------------------------------- */
/* Angle (tenths of a degree, 0 = 12 o'clock, clockwise) of division `idx` of a
 * ring split into `count` equal divisions. Shares the LVGL angle convention of
 * gz_watch.h so gz_watch_polar() places the numeral and gz_watch_hand() the
 * pointer with no second convention. idx is taken modulo count. */
static inline int32_t gz_ring_angle_tenths(int idx, int count)
{
    if (count <= 0) return 0;
    idx %= count;
    if (idx < 0) idx += count;
    return (int32_t)((long)idx * 3600L / count);
}

/* Month 1..12 on a twelve-division ring (January at the top). */
static inline int32_t gz_month_angle_tenths(int month)
{
    return gz_ring_angle_tenths(month - 1, 12);
}

/* Day-of-week 0..6 (0 = Sunday, matching gz_rtc.h) on a seven-division ring. */
static inline int32_t gz_weekday_angle_tenths(int weekday)
{
    return gz_ring_angle_tenths(weekday, 7);
}

/* Day-of-month 1..dim on a ring of `dim` divisions (the month's real length,
 * so the 1st sits at the top whatever the month). */
static inline int32_t gz_date_angle_tenths(int day, int days_in_month)
{
    return gz_ring_angle_tenths(day - 1, days_in_month);
}

/* ---- printed sub-scale captions (unit 2 render, B2) ---------------------- */
/* The month and day-of-week rings print the division's name; the date ring
 * prints a numeral every fifth day plus the month's last day. These give the
 * printed text the render stamps on each sub-dial, so the scale and its blued
 * pointer agree. Pure string/predicate helpers — native-tested in
 * test_godzilla_moon. */

/* Three-letter month name for month 1..12 (uppercase); "" out of range. */
static inline const char *gz_month_abbrev(int month)
{
    static const char *const m[13] = { "", "JAN", "FEB", "MAR", "APR", "MAY",
                                       "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    return (month >= 1 && month <= 12) ? m[month] : "";
}

/* Three-letter weekday name for weekday 0..6 (0 = Sunday, matching gz_rtc.h);
 * "" out of range. */
static inline const char *gz_weekday_abbrev(int weekday)
{
    static const char *const d[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    return (weekday >= 0 && weekday <= 6) ? d[weekday] : "";
}

/* The date ring is too fine to number every day, so it prints a numeral every
 * fifth day (5, 10, 15, 20, 25) and always on the month's last day (its length),
 * dropping a fifth-day numeral that would crowd the last-day one. True iff day
 * `d` of a `dim`-day month gets a printed numeral:
 *   - 31-day month: 5, 10, 15, 20, 25, 31  (30 is dropped, abutting 31);
 *   - 30-day month: 5, 10, 15, 20, 25, 30;
 *   - 28-day month: 5, 10, 15, 20, 25, 28. */
static inline bool gz_date_label_shown(int day, int days_in_month)
{
    if (day < 1 || day > days_in_month) return false;
    if (day == days_in_month) return true;                 /* always the last day */
    return (day % 5) == 0 && day <= days_in_month - 2;      /* 5s, clear of the last */
}
