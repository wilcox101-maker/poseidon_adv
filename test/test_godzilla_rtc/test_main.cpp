/* Native Unity test for godzilla_node/main/gz_rtc.h — the PURE PCF85063 time
 * logic (BCD, register decode/encode, validation, weekday, build-time parse and
 * the oscillator-stopped seed policy). The Wire I2C transactions are on-target
 * (ARDUINO-only) and not exercised here. Pins the behaviour Dale's clock and the
 * later pocket-watch face depend on, so a bit-mask or BCD slip fails CI, not the
 * bench. */
#include <unity.h>
#include "../../godzilla_node/main/gz_rtc.h"

void setUp(void) {}
void tearDown(void) {}

static void test_bcd_roundtrip(void)
{
    for (uint8_t v = 0; v <= 99; ++v) {
        TEST_ASSERT_EQUAL_UINT8(v, gz_bcd2bin(gz_bin2bcd(v)));
    }
    /* Known encodings. */
    TEST_ASSERT_EQUAL_HEX8(0x59, gz_bin2bcd(59));
    TEST_ASSERT_EQUAL_UINT8(45, gz_bcd2bin(0x45));
}

static void test_decode_masks_os_and_reserved(void)
{
    /* 2026-11-19 (Thu) 14:32:07, with the OS flag set in Seconds and high
     * reserved bits dirtied in several fields. Masking must strip them all. */
    uint8_t regs[GZ_RTC_TIME_BYTES] = {
        (uint8_t)(0x80 | 0x07), /* sec 07 + OS flag                */
        0x32,                   /* min 32                          */
        (uint8_t)(0xC0 | 0x14), /* hour 14 + dirty bits 7,6        */
        0x19,                   /* day 19                          */
        (uint8_t)(0xF8 | 0x04), /* weekday 4 (Thu) + dirty high    */
        0x11,                   /* month 11                        */
        0x26,                   /* year 26 -> 2026                 */
    };
    gz_datetime_t dt;
    gz_rtc_decode(regs, &dt);
    TEST_ASSERT_TRUE(dt.osc_stopped);
    TEST_ASSERT_EQUAL_UINT16(2026, dt.year);
    TEST_ASSERT_EQUAL_UINT8(11, dt.month);
    TEST_ASSERT_EQUAL_UINT8(19, dt.day);
    TEST_ASSERT_EQUAL_UINT8(4,  dt.weekday);
    TEST_ASSERT_EQUAL_UINT8(14, dt.hour);
    TEST_ASSERT_EQUAL_UINT8(32, dt.minute);
    TEST_ASSERT_EQUAL_UINT8(7,  dt.second);
}

static void test_encode_clears_os_and_roundtrips(void)
{
    gz_datetime_t dt = {2026, 11, 19, 4, 23, 5, 9, false};
    uint8_t regs[GZ_RTC_TIME_BYTES];
    gz_rtc_encode(&dt, regs);
    TEST_ASSERT_EQUAL_HEX8(0x00, regs[0] & GZ_RTC_OS_FLAG); /* OS cleared */
    gz_datetime_t back;
    gz_rtc_decode(regs, &back);
    TEST_ASSERT_FALSE(back.osc_stopped);
    TEST_ASSERT_EQUAL_UINT16(dt.year, back.year);
    TEST_ASSERT_EQUAL_UINT8(dt.month, back.month);
    TEST_ASSERT_EQUAL_UINT8(dt.day, back.day);
    TEST_ASSERT_EQUAL_UINT8(dt.weekday, back.weekday);
    TEST_ASSERT_EQUAL_UINT8(dt.hour, back.hour);
    TEST_ASSERT_EQUAL_UINT8(dt.minute, back.minute);
    TEST_ASSERT_EQUAL_UINT8(dt.second, back.second);
}

static void test_valid_bounds(void)
{
    gz_datetime_t ok = {2026, 11, 19, 4, 14, 32, 7, false};
    TEST_ASSERT_TRUE(gz_rtc_valid(&ok));

    gz_datetime_t m0 = ok; m0.month = 0;  TEST_ASSERT_FALSE(gz_rtc_valid(&m0));
    gz_datetime_t h24 = ok; h24.hour = 24; TEST_ASSERT_FALSE(gz_rtc_valid(&h24));
    gz_datetime_t d0 = ok; d0.day = 0;    TEST_ASSERT_FALSE(gz_rtc_valid(&d0));

    /* Feb 29 is valid in a leap year, invalid otherwise. */
    gz_datetime_t leap = {2024, 2, 29, 4, 0, 0, 0, false};
    TEST_ASSERT_TRUE(gz_rtc_valid(&leap));
    gz_datetime_t noleap = {2026, 2, 29, 0, 0, 0, 0, false};
    TEST_ASSERT_FALSE(gz_rtc_valid(&noleap));
    /* 2100 is NOT a leap year (century rule). */
    gz_datetime_t c2100 = {2099, 2, 29, 0, 0, 0, 0, false};
    TEST_ASSERT_FALSE(gz_rtc_valid(&c2100));
}

static void test_weekday_sakamoto(void)
{
    /* 2026-11-19 is a Thursday (4); 2000-01-01 a Saturday (6);
     * 2024-02-29 a Thursday (4). */
    TEST_ASSERT_EQUAL_UINT8(4, gz_rtc_weekday(2026, 11, 19));
    TEST_ASSERT_EQUAL_UINT8(6, gz_rtc_weekday(2000, 1, 1));
    TEST_ASSERT_EQUAL_UINT8(0, gz_rtc_weekday(2026, 11, 22)); /* Sunday */
    TEST_ASSERT_EQUAL_UINT8(4, gz_rtc_weekday(2024, 2, 29));
}

static void test_parse_build_strings(void)
{
    gz_datetime_t dt;
    TEST_ASSERT_TRUE(gz_rtc_parse_build("Nov 19 2026", "14:32:07", &dt));
    TEST_ASSERT_EQUAL_UINT16(2026, dt.year);
    TEST_ASSERT_EQUAL_UINT8(11, dt.month);
    TEST_ASSERT_EQUAL_UINT8(19, dt.day);
    TEST_ASSERT_EQUAL_UINT8(14, dt.hour);
    TEST_ASSERT_EQUAL_UINT8(32, dt.minute);
    TEST_ASSERT_EQUAL_UINT8(7,  dt.second);
    TEST_ASSERT_EQUAL_UINT8(4,  dt.weekday);   /* Thursday */
    TEST_ASSERT_FALSE(dt.osc_stopped);

    /* Single-digit day is space-padded by __DATE__. */
    gz_datetime_t sp;
    TEST_ASSERT_TRUE(gz_rtc_parse_build("Jan  5 2027", "09:00:00", &sp));
    TEST_ASSERT_EQUAL_UINT8(1, sp.month);
    TEST_ASSERT_EQUAL_UINT8(5, sp.day);

    /* A garbled month fails rather than producing month 0. */
    gz_datetime_t bad;
    TEST_ASSERT_FALSE(gz_rtc_parse_build("Xyz 05 2027", "09:00:00", &bad));
}

static void test_seed_policy(void)
{
    gz_datetime_t build = {2026, 11, 19, 4, 14, 32, 7, false};

    /* Stopped oscillator -> seed from build time, OS flag cleared. */
    gz_datetime_t stopped = {2000, 1, 1, 6, 0, 0, 0, true};
    TEST_ASSERT_TRUE(gz_rtc_seed_if_stopped(&stopped, &build));
    TEST_ASSERT_EQUAL_UINT16(2026, stopped.year);
    TEST_ASSERT_FALSE(stopped.osc_stopped);

    /* Garbage (out-of-range) but OS clear -> still seeded. */
    gz_datetime_t garbage = {2026, 13, 40, 0, 25, 99, 99, false};
    TEST_ASSERT_TRUE(gz_rtc_seed_if_stopped(&garbage, &build));
    TEST_ASSERT_EQUAL_UINT8(11, garbage.month);

    /* Running clock with a sane time -> left untouched. */
    gz_datetime_t running = {2026, 6, 15, 1, 8, 5, 30, false};
    TEST_ASSERT_FALSE(gz_rtc_seed_if_stopped(&running, &build));
    TEST_ASSERT_EQUAL_UINT8(6, running.month);
    TEST_ASSERT_EQUAL_UINT8(15, running.day);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bcd_roundtrip);
    RUN_TEST(test_decode_masks_os_and_reserved);
    RUN_TEST(test_encode_clears_os_and_roundtrips);
    RUN_TEST(test_valid_bounds);
    RUN_TEST(test_weekday_sakamoto);
    RUN_TEST(test_parse_build_strings);
    RUN_TEST(test_seed_policy);
    return UNITY_END();
}
