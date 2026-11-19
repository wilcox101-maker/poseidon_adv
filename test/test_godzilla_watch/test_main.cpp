/* Native Unity test for godzilla_node/main/gz_watch.h — the pure pocket-watch
 * model (P2c): hand angles, polar placement, calendar advance with rollover,
 * the millis()-driven software clock, the set-mode state machine and the
 * Roman numerals. The LVGL face and the RTC I2C are on-target only. */
#include <unity.h>
#include "../../godzilla_node/main/gz_watch.h"

void setUp(void) {}
void tearDown(void) {}

static gz_datetime_t dt(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
    gz_datetime_t t;
    t.year = y; t.month = mo; t.day = d; t.hour = h; t.minute = mi; t.second = s;
    t.weekday = gz_rtc_weekday(y, mo, d);
    t.osc_stopped = false;
    return t;
}

static void test_hand_angles(void)
{
    /* 00:00:00 — all at 12. */
    TEST_ASSERT_EQUAL_INT32(0, gz_watch_second_angle(0));
    TEST_ASSERT_EQUAL_INT32(0, gz_watch_minute_angle(0, 0));
    TEST_ASSERT_EQUAL_INT32(0, gz_watch_hour_angle(0, 0));
    /* 03:00 — hour hand at 90 deg. */
    TEST_ASSERT_EQUAL_INT32(900, gz_watch_hour_angle(3, 0));
    /* 06:30 — hour halfway 6->7 (195 deg), minute at 180 deg. */
    TEST_ASSERT_EQUAL_INT32(1950, gz_watch_hour_angle(6, 30));
    TEST_ASSERT_EQUAL_INT32(1800, gz_watch_minute_angle(30, 0));
    /* 11:59:59 — everything just short of 12. */
    TEST_ASSERT_EQUAL_INT32(3595, gz_watch_hour_angle(11, 59));
    TEST_ASSERT_EQUAL_INT32(3599, gz_watch_minute_angle(59, 59));
    TEST_ASSERT_EQUAL_INT32(3540, gz_watch_second_angle(59));
    /* 12:00 and 23:59 fold onto the 12-hour dial. */
    TEST_ASSERT_EQUAL_INT32(0, gz_watch_hour_angle(12, 0));
    TEST_ASSERT_EQUAL_INT32(3595, gz_watch_hour_angle(23, 59));
    TEST_ASSERT_EQUAL_INT32(gz_watch_hour_angle(3, 15), gz_watch_hour_angle(15, 15));
}

static void test_polar_and_hand(void)
{
    int x, y;
    gz_watch_polar(0, 100, 180, 188, &x, &y);
    TEST_ASSERT_EQUAL_INT(180, x); TEST_ASSERT_EQUAL_INT(88, y);
    gz_watch_polar(900, 100, 180, 188, &x, &y);
    TEST_ASSERT_EQUAL_INT(280, x); TEST_ASSERT_EQUAL_INT(188, y);
    gz_watch_polar(1800, 100, 180, 188, &x, &y);
    TEST_ASSERT_EQUAL_INT(180, x); TEST_ASSERT_EQUAL_INT(288, y);
    gz_watch_polar(2700, 100, 180, 188, &x, &y);
    TEST_ASSERT_EQUAL_INT(80, x); TEST_ASSERT_EQUAL_INT(188, y);

    /* A hand at 3 o'clock: tail to the left of the pivot, tip to the right. */
    int x0, y0, x1, y1;
    gz_watch_hand(900, 100, 20, 180, 188, &x0, &y0, &x1, &y1);
    TEST_ASSERT_EQUAL_INT(160, x0); TEST_ASSERT_EQUAL_INT(188, y0);
    TEST_ASSERT_EQUAL_INT(280, x1); TEST_ASSERT_EQUAL_INT(188, y1);
}

static void test_advance_rollovers(void)
{
    /* Seconds only; sub-second remainder dropped. */
    gz_datetime_t t = dt(2026, 11, 19, 14, 32, 7);
    gz_watch_advance(&t, 2999);
    TEST_ASSERT_EQUAL_UINT8(9, t.second);
    TEST_ASSERT_EQUAL_UINT8(32, t.minute);

    /* Across midnight: date and weekday move (Thu -> Fri). */
    t = dt(2026, 11, 19, 23, 59, 58);
    TEST_ASSERT_EQUAL_UINT8(4, t.weekday);
    gz_watch_advance(&t, 3000);
    TEST_ASSERT_EQUAL_UINT8(0, t.hour);
    TEST_ASSERT_EQUAL_UINT8(0, t.minute);
    TEST_ASSERT_EQUAL_UINT8(1, t.second);
    TEST_ASSERT_EQUAL_UINT8(20, t.day);
    TEST_ASSERT_EQUAL_UINT8(5, t.weekday);

    /* Across a 30-day month end. */
    t = dt(2026, 11, 30, 23, 59, 59);
    gz_watch_advance(&t, 1000);
    TEST_ASSERT_EQUAL_UINT8(12, t.month);
    TEST_ASSERT_EQUAL_UINT8(1, t.day);

    /* February: 2028 is a leap year, 2027 is not. */
    t = dt(2028, 2, 28, 23, 59, 59);
    gz_watch_advance(&t, 1000);
    TEST_ASSERT_EQUAL_UINT8(2, t.month);
    TEST_ASSERT_EQUAL_UINT8(29, t.day);
    t = dt(2027, 2, 28, 23, 59, 59);
    gz_watch_advance(&t, 1000);
    TEST_ASSERT_EQUAL_UINT8(3, t.month);
    TEST_ASSERT_EQUAL_UINT8(1, t.day);

    /* Across a year end. */
    t = dt(2026, 12, 31, 23, 59, 59);
    gz_watch_advance(&t, 1000);
    TEST_ASSERT_EQUAL_UINT16(2027, t.year);
    TEST_ASSERT_EQUAL_UINT8(1, t.month);
    TEST_ASSERT_EQUAL_UINT8(1, t.day);
    TEST_ASSERT_TRUE(gz_rtc_valid(&t));
    TEST_ASSERT_EQUAL_UINT8(gz_rtc_weekday(2027, 1, 1), t.weekday);

    /* Many days at once stays a valid date with the right weekday. */
    t = dt(2026, 11, 19, 12, 0, 0);
    gz_watch_advance(&t, 40u * 86400u * 1000u);          /* 40 days */
    TEST_ASSERT_EQUAL_UINT8(12, t.month);
    TEST_ASSERT_EQUAL_UINT8(29, t.day);
    TEST_ASSERT_EQUAL_UINT8(12, t.hour);
    TEST_ASSERT_EQUAL_UINT8(gz_rtc_weekday(2026, 12, 29), t.weekday);

    /* The RTC's range ends at 2099: saturate, don't wrap. */
    t = dt(2099, 12, 31, 23, 59, 50);
    gz_watch_advance(&t, 60000);
    TEST_ASSERT_EQUAL_UINT16(2099, t.year);
    TEST_ASSERT_EQUAL_UINT8(31, t.day);
    TEST_ASSERT_EQUAL_UINT8(23, t.hour);
    TEST_ASSERT_EQUAL_UINT8(59, t.second);
}

static void test_software_clock_and_millis_wrap(void)
{
    gz_watch_clock_t c = {};
    TEST_ASSERT_TRUE(gz_watch_resync_due(&c, 0));        /* never synced */

    gz_datetime_t base = dt(2026, 11, 19, 3, 22, 10);
    gz_watch_sync(&c, &base, 0xFFFFF000u);               /* just before the wrap */
    TEST_ASSERT_FALSE(gz_watch_resync_due(&c, 0xFFFFF000u + 59999u));
    TEST_ASSERT_TRUE(gz_watch_resync_due(&c, 0xFFFFF000u + 60000u));  /* wrapped */

    gz_datetime_t now;
    gz_watch_now(&c, 0xFFFFF000u + 5000u, &now);         /* wraps past 0 */
    TEST_ASSERT_EQUAL_UINT8(15, now.second);
    TEST_ASSERT_EQUAL_UINT8(22, now.minute);
    /* The base itself is not consumed by reading. */
    gz_watch_now(&c, 0xFFFFF000u + 5000u, &now);
    TEST_ASSERT_EQUAL_UINT8(15, now.second);
}

static void test_set_mode(void)
{
    const int CY = 188;
    gz_watch_set_t s = {};
    gz_datetime_t now = dt(2026, 11, 19, 23, 59, 41);

    /* Taps before entering do nothing. */
    gz_watch_set_tap(&s, 10, CY, 100);
    TEST_ASSERT_FALSE(s.active);

    gz_watch_set_enter(&s, &now, 1000);
    TEST_ASSERT_TRUE(s.active);
    TEST_ASSERT_EQUAL_UINT8(GZ_WATCH_FIELD_HOUR, s.field);
    TEST_ASSERT_EQUAL_UINT8(41, s.edit.second);

    /* Upper half: hour +1, wraps 23 -> 0, seconds zeroed, date untouched. */
    gz_watch_set_tap(&s, CY - 1, CY, 2000);
    TEST_ASSERT_EQUAL_UINT8(0, s.edit.hour);
    TEST_ASSERT_EQUAL_UINT8(0, s.edit.second);
    TEST_ASSERT_EQUAL_UINT8(19, s.edit.day);
    TEST_ASSERT_EQUAL_UINT8(GZ_WATCH_FIELD_HOUR, s.field);

    /* Lower half (the centre row counts as lower): minute +1, wraps 59 -> 0
     * without carrying into the hour. */
    gz_watch_set_tap(&s, CY, CY, 3000);
    TEST_ASSERT_EQUAL_UINT8(0, s.edit.minute);
    TEST_ASSERT_EQUAL_UINT8(0, s.edit.hour);
    TEST_ASSERT_EQUAL_UINT8(GZ_WATCH_FIELD_MINUTE, s.field);

    /* Idle timeout counts from the last input, across the millis() wrap. */
    TEST_ASSERT_FALSE(gz_watch_set_timed_out(&s, 3000 + GZ_WATCH_SET_TIMEOUT_MS - 1));
    TEST_ASSERT_TRUE(gz_watch_set_timed_out(&s, 3000 + GZ_WATCH_SET_TIMEOUT_MS));
    s.last_input_ms = 0xFFFFFF00u;
    TEST_ASSERT_FALSE(gz_watch_set_timed_out(&s, 0x00000100u));

    /* Commit hands back a valid time with seconds 0 and leaves set mode. */
    gz_datetime_t out;
    gz_watch_set_commit(&s, &out);
    TEST_ASSERT_FALSE(s.active);
    TEST_ASSERT_EQUAL_UINT8(0, out.hour);
    TEST_ASSERT_EQUAL_UINT8(0, out.minute);
    TEST_ASSERT_EQUAL_UINT8(0, out.second);
    TEST_ASSERT_TRUE(gz_rtc_valid(&out));
    TEST_ASSERT_FALSE(gz_watch_set_timed_out(&s, 0x7FFFFFFFu));   /* inactive */

    /* Cancel leaves without a result. */
    gz_watch_set_enter(&s, &now, 5000);
    gz_watch_set_cancel(&s);
    TEST_ASSERT_FALSE(s.active);
}

static void test_roman_numerals(void)
{
    TEST_ASSERT_EQUAL_STRING("I", gz_watch_roman(1));
    TEST_ASSERT_EQUAL_STRING("IIII", gz_watch_roman(4));     /* dial convention */
    TEST_ASSERT_EQUAL_STRING("IX", gz_watch_roman(9));
    TEST_ASSERT_EQUAL_STRING("XII", gz_watch_roman(12));
    TEST_ASSERT_EQUAL_STRING("", gz_watch_roman(0));
    TEST_ASSERT_EQUAL_STRING("", gz_watch_roman(13));
}

static void test_days_in_month(void)
{
    TEST_ASSERT_EQUAL_UINT8(31, gz_rtc_days_in_month(2026, 1));
    TEST_ASSERT_EQUAL_UINT8(28, gz_rtc_days_in_month(2026, 2));
    TEST_ASSERT_EQUAL_UINT8(29, gz_rtc_days_in_month(2028, 2));
    TEST_ASSERT_EQUAL_UINT8(29, gz_rtc_days_in_month(2000, 2));
    TEST_ASSERT_EQUAL_UINT8(30, gz_rtc_days_in_month(2026, 11));
    TEST_ASSERT_EQUAL_UINT8(0, gz_rtc_days_in_month(2026, 0));
    TEST_ASSERT_EQUAL_UINT8(0, gz_rtc_days_in_month(2026, 13));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hand_angles);
    RUN_TEST(test_polar_and_hand);
    RUN_TEST(test_advance_rollovers);
    RUN_TEST(test_software_clock_and_millis_wrap);
    RUN_TEST(test_set_mode);
    RUN_TEST(test_roman_numerals);
    RUN_TEST(test_days_in_month);
    return UNITY_END();
}
