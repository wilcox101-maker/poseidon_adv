/* Native Unity test for godzilla_node/main/gz_timeset.h — the pure epoch ->
 * civil time conversion of the CMD_TIMESET consumer (time-sync node consumer,
 * step 3):
 *   1. known UTC epochs convert to the expected civil date/time and weekday;
 *   2. a leap day (2024-02-29) converts correctly;
 *   3. the display-only tz offset shifts local time, rolling the date across
 *      midnight in both directions;
 *   4. the validity floor accepts a real UTC epoch and rejects a bogus / unset
 *      (sub-floor, zero) one — the same floor the sender uses.
 * Pure host TU, same style as test_godzilla_rtc / test_godzilla_faceclock. */
#include <unity.h>
#include "../../godzilla_node/main/gz_timeset.h"

void setUp(void) {}
void tearDown(void) {}

static void check(uint32_t epoch, int16_t tz, int Y, int Mo, int D,
                  int h, int mi, int s, int wd)
{
    gz_datetime_t t;
    gz_timeset_to_datetime(epoch, tz, &t);
    TEST_ASSERT_EQUAL_INT(Y,  t.year);
    TEST_ASSERT_EQUAL_INT(Mo, t.month);
    TEST_ASSERT_EQUAL_INT(D,  t.day);
    TEST_ASSERT_EQUAL_INT(h,  t.hour);
    TEST_ASSERT_EQUAL_INT(mi, t.minute);
    TEST_ASSERT_EQUAL_INT(s,  t.second);
    TEST_ASSERT_EQUAL_INT(wd, t.weekday);
    TEST_ASSERT_FALSE(t.osc_stopped);
    TEST_ASSERT_TRUE(gz_rtc_valid(&t));       /* a real calendar date */
}

/* ---- 1. known epochs ----------------------------------------------------- */
static void test_known_epochs_utc(void)
{
    /* The floor epoch itself: 2023-11-14 22:13:20 UTC, a Tuesday (wd 2). */
    check(1700000000u, 0, 2023, 11, 14, 22, 13, 20, 2);
    /* 2024-01-01 00:00:00 UTC — a Monday (wd 1). */
    check(1704067200u, 0, 2024, 1, 1, 0, 0, 0, 1);
    /* 2025-01-01 00:00:00 UTC — a Wednesday (wd 3). */
    check(1735689600u, 0, 2025, 1, 1, 0, 0, 0, 3);
}

/* ---- 2. leap day --------------------------------------------------------- */
static void test_leap_day(void)
{
    /* 2024-02-29 00:00:00 UTC — 2024 is a leap year; a Thursday (wd 4). */
    check(1709164800u, 0, 2024, 2, 29, 0, 0, 0, 4);
    /* One second before -> 2024-02-28 23:59:59. */
    check(1709164799u, 0, 2024, 2, 28, 23, 59, 59, 3);
}

/* ---- 3. tz offset (display only) shifts local time ----------------------- */
static void test_tz_offset_applied(void)
{
    /* 2024-01-01 00:00:00 UTC at UTC-5 (US Eastern, -300) -> 2023-12-31 19:00. */
    check(1704067200u, -300, 2023, 12, 31, 19, 0, 0, 0);   /* Sunday (wd 0) */
    /* Same instant at UTC+5:30 (India, +330) -> 2024-01-01 05:30:00. */
    check(1704067200u, 330, 2024, 1, 1, 5, 30, 0, 1);      /* Monday (wd 1) */
    /* A positive offset that rolls forward across midnight: 23:00 UTC + 2h. */
    check(1704063600u, 120, 2024, 1, 1, 1, 0, 0, 1);       /* 2023-12-31 23:00 UTC +2h */
}

/* ---- 4. validity floor --------------------------------------------------- */
static void test_validity_floor(void)
{
    TEST_ASSERT_TRUE (gz_timeset_valid(GZ_TIMESET_EPOCH_MIN));
    TEST_ASSERT_TRUE (gz_timeset_valid(1704067200u));
    TEST_ASSERT_FALSE(gz_timeset_valid(GZ_TIMESET_EPOCH_MIN - 1u));
    TEST_ASSERT_FALSE(gz_timeset_valid(0u));
    TEST_ASSERT_FALSE(gz_timeset_valid(1000u));               /* boot-relative */
    /* The floor matches the sender's documented sentinel. */
    TEST_ASSERT_EQUAL_UINT32(1700000000u, GZ_TIMESET_EPOCH_MIN);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_epochs_utc);
    RUN_TEST(test_leap_day);
    RUN_TEST(test_tz_offset_applied);
    RUN_TEST(test_validity_floor);
    return UNITY_END();
}
