/* Native Unity tests for the bench self-test model (godzilla_node/main/gz_selftest.h):
 * the per-line pass/fail/skip verdict, the value and serial-line text, and the
 * capture-rate helper. All pure — no board header, no LVGL, no Arduino. Pins
 * item J's host-testable core: the formatters must be bounded and the verdicts
 * must track the capability flags (a part absent on a board is SKIP, never a
 * failure). */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_selftest.h"

void setUp(void) {}
void tearDown(void) {}

/* A snapshot with every part present and healthy. */
static gz_selftest_t all_good(void)
{
    gz_selftest_t s;
    memset(&s, 0, sizeof s);
    s.cap_rtc = s.cap_imu = s.cap_gauge = s.cap_mic = s.cap_sd = true;
    s.disp_ok = true;
    s.touch_found = true; s.touch_seen = true; s.touch_x = 127; s.touch_y = 44;
    s.rtc_ok = true; s.rtc_osc_stopped = false;
    s.imu_ok = true; s.imu_moving = false;
    s.gauge_ok = true; s.gauge_mv = 4050;
    s.mic_ok = true;
    s.sd_mounted = true;
    s.wifi_any = s.ble_any = true;
    s.wifi_delta = 40; s.ble_delta = 24; s.rate_window_ms = 2000;
    s.link_up = true; s.has_published = true; s.pub_age_ms = 3000;
    return s;
}

/* ---- rate helper --------------------------------------------------------- */

static void test_rate_x10_rounds(void)
{
    /* 40 frames over 2000 ms = 20.0/s -> 200 */
    TEST_ASSERT_EQUAL_UINT32(200u, gz_st_rate_x10(40u, 2000u));
    /* 25 over 2000 ms = 12.5/s -> 125 */
    TEST_ASSERT_EQUAL_UINT32(125u, gz_st_rate_x10(25u, 2000u));
    /* 1 over 3000 ms = 0.333/s -> rounds to 0.3 -> 3 */
    TEST_ASSERT_EQUAL_UINT32(3u, gz_st_rate_x10(1u, 3000u));
    /* zero window never divides by zero */
    TEST_ASSERT_EQUAL_UINT32(0u, gz_st_rate_x10(100u, 0u));
    /* zero traffic */
    TEST_ASSERT_EQUAL_UINT32(0u, gz_st_rate_x10(0u, 1000u));
}

static void test_rate_x10_clamps(void)
{
    /* A pathological delta is clamped so delta*10000 cannot overflow uint32. */
    uint32_t r = gz_st_rate_x10(0xFFFFFFFFu, 1000u);
    TEST_ASSERT_EQUAL_UINT32(4000000u, r);   /* clamp 400000 -> 4000.0/s */
}

static void test_rate_string(void)
{
    char b[16];
    gz_st_fmt_rate(40u, 2000u, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("20.0/s", b);
    gz_st_fmt_rate(25u, 2000u, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("12.5/s", b);
    gz_st_fmt_rate(0u, 1000u, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("0.0/s", b);
}

/* ---- verdicts ------------------------------------------------------------ */

static void test_all_good_all_pass(void)
{
    gz_selftest_t s = all_good();
    for (int i = 0; i < GZ_ST_LINE_COUNT; ++i)
        TEST_ASSERT_EQUAL_INT_MESSAGE(GZ_ST_PASS, gz_selftest_result(&s, i),
                                      gz_selftest_label(i));
}

static void test_absent_parts_skip_not_fail(void)
{
    /* A board with no RTC / IMU / gauge / mic / SD (e.g. the ES3C28P has no RTC):
     * those lines are SKIP, and the always-present lines still PASS. */
    gz_selftest_t s = all_good();
    s.cap_rtc = s.cap_imu = s.cap_gauge = s.cap_mic = s.cap_sd = false;
    TEST_ASSERT_EQUAL_INT(GZ_ST_SKIP, gz_selftest_result(&s, GZ_ST_RTC));
    TEST_ASSERT_EQUAL_INT(GZ_ST_SKIP, gz_selftest_result(&s, GZ_ST_IMU));
    TEST_ASSERT_EQUAL_INT(GZ_ST_SKIP, gz_selftest_result(&s, GZ_ST_GAUGE));
    TEST_ASSERT_EQUAL_INT(GZ_ST_SKIP, gz_selftest_result(&s, GZ_ST_MIC));
    TEST_ASSERT_EQUAL_INT(GZ_ST_SKIP, gz_selftest_result(&s, GZ_ST_SD));
    TEST_ASSERT_EQUAL_INT(GZ_ST_PASS, gz_selftest_result(&s, GZ_ST_DISPLAY));
    TEST_ASSERT_EQUAL_INT(GZ_ST_PASS, gz_selftest_result(&s, GZ_ST_WIFI));
    TEST_ASSERT_EQUAL_INT(GZ_ST_PASS, gz_selftest_result(&s, GZ_ST_LINK));
}

static void test_failures(void)
{
    gz_selftest_t s = all_good();
    s.disp_ok = false;          TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_DISPLAY));
    s = all_good();
    s.touch_found = false;      TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_TOUCH));
    s = all_good();
    s.rtc_ok = false;           TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_RTC));
    s = all_good();
    /* answered but the oscillator stopped: a real finding, so FAIL. */
    s.rtc_osc_stopped = true;   TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_RTC));
    s = all_good();
    s.imu_ok = false;           TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_IMU));
    s = all_good();
    s.sd_mounted = false;       TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_SD));
    s = all_good();
    s.wifi_any = false;         TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_WIFI));
    s = all_good();
    s.ble_any = false;          TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_BLE));
    s = all_good();
    s.has_published = false;    TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_LINK));
    s = all_good();
    s.link_up = false;          TEST_ASSERT_EQUAL_INT(GZ_ST_FAIL, gz_selftest_result(&s, GZ_ST_LINK));
}

/* ---- value text ---------------------------------------------------------- */

static void test_value_text(void)
{
    char b[24];
    gz_selftest_t s = all_good();
    gz_selftest_fmt_value(&s, GZ_ST_TOUCH, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("127,44", b);
    gz_selftest_fmt_value(&s, GZ_ST_RTC, b, sizeof b);    TEST_ASSERT_EQUAL_STRING("running", b);
    gz_selftest_fmt_value(&s, GZ_ST_GAUGE, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("4.05 V", b);
    gz_selftest_fmt_value(&s, GZ_ST_IMU, b, sizeof b);    TEST_ASSERT_EQUAL_STRING("still", b);
    gz_selftest_fmt_value(&s, GZ_ST_LINK, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("pub 3s", b);
    gz_selftest_fmt_value(&s, GZ_ST_WIFI, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("20.0/s", b);

    s.touch_seen = false;
    gz_selftest_fmt_value(&s, GZ_ST_TOUCH, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("no tap yet", b);
    s.touch_found = false;
    gz_selftest_fmt_value(&s, GZ_ST_TOUCH, b, sizeof b);  TEST_ASSERT_EQUAL_STRING("not found", b);

    s = all_good();
    s.rtc_osc_stopped = true;
    gz_selftest_fmt_value(&s, GZ_ST_RTC, b, sizeof b);    TEST_ASSERT_EQUAL_STRING("osc stopped", b);
    s.cap_rtc = false;
    gz_selftest_fmt_value(&s, GZ_ST_RTC, b, sizeof b);    TEST_ASSERT_EQUAL_STRING("n/a", b);

    s = all_good();
    s.pub_age_ms = 125000;   /* 2m05s */
    gz_selftest_fmt_value(&s, GZ_ST_LINK, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("pub 2m05s", b);
    s.has_published = false;
    gz_selftest_fmt_value(&s, GZ_ST_LINK, b, sizeof b);   TEST_ASSERT_EQUAL_STRING("up, no publish", b);
}

/* The value text carries no non-ASCII (the LVGL build has ASCII fonts only). */
static void test_value_text_ascii_only(void)
{
    gz_selftest_t s = all_good();
    char b[24];
    for (int i = 0; i < GZ_ST_LINE_COUNT; ++i) {
        gz_selftest_fmt_value(&s, i, b, sizeof b);
        for (size_t k = 0; b[k]; ++k)
            TEST_ASSERT_TRUE_MESSAGE((unsigned char)b[k] < 0x80u, gz_selftest_label(i));
    }
}

/* ---- serial summary ------------------------------------------------------ */

static void test_serial_line(void)
{
    gz_selftest_t s = all_good();
    s.cap_mic = false;          /* one skipped line reads "--" */
    char b[128];
    gz_selftest_serial(&s, b, sizeof b);
    TEST_ASSERT_EQUAL_STRING(
        "SELFTEST disp=ok touch=ok rtc=ok imu=ok gauge=ok mic=-- sd=ok wifi=ok ble=ok link=ok",
        b);
}

/* ---- bounded buffers ----------------------------------------------------- */

static void test_formatters_bounded(void)
{
    gz_selftest_t s = all_good();
    char b[5];
    memset(b, 'X', sizeof b);
    gz_selftest_fmt_line(&s, GZ_ST_GAUGE, b, sizeof b);
    TEST_ASSERT_EQUAL_CHAR('\0', b[4]);
    TEST_ASSERT_TRUE(strlen(b) < sizeof b);

    char sb[10];
    memset(sb, 'X', sizeof sb);
    gz_selftest_serial(&s, sb, sizeof sb);
    TEST_ASSERT_EQUAL_CHAR('\0', sb[9]);
    TEST_ASSERT_TRUE(strlen(sb) < sizeof sb);

    char rb[3];
    memset(rb, 'X', sizeof rb);
    gz_st_fmt_rate(40u, 2000u, rb, sizeof rb);
    TEST_ASSERT_EQUAL_CHAR('\0', rb[2]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rate_x10_rounds);
    RUN_TEST(test_rate_x10_clamps);
    RUN_TEST(test_rate_string);
    RUN_TEST(test_all_good_all_pass);
    RUN_TEST(test_absent_parts_skip_not_fail);
    RUN_TEST(test_failures);
    RUN_TEST(test_value_text);
    RUN_TEST(test_value_text_ascii_only);
    RUN_TEST(test_serial_line);
    RUN_TEST(test_formatters_bounded);
    return UNITY_END();
}
