/* Native Unity tests for the status-screen model (godzilla_node/main/gz_status.h):
 * publish counters and the uptime / last-publish-age strings the P2a status
 * screen shows. The formatters must be bounded (never overrun the caller's
 * buffer) and correct across hour/day boundaries. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_status.h"

void setUp(void) {}
void tearDown(void) {}

static void test_counters_accumulate_per_radio(void)
{
    gz_status_t s;
    gz_status_init(&s);
    gz_status_note_publish(&s, GZ_RADIO_BLE, 5, 1000);
    gz_status_note_publish(&s, GZ_RADIO_BLE, 3, 2000);
    gz_status_note_publish(&s, GZ_RADIO_WIFI, 7, 2500);
    TEST_ASSERT_EQUAL_UINT32(2, s.ble_frames);
    TEST_ASSERT_EQUAL_UINT8(3, s.ble_last_entries);   /* entries in LAST batch */
    TEST_ASSERT_EQUAL_UINT32(1, s.wifi_frames);
    TEST_ASSERT_EQUAL_UINT8(7, s.wifi_last_entries);
    TEST_ASSERT_EQUAL_UINT32(2500, s.last_pub_ms);    /* newest of either radio */
    TEST_ASSERT_TRUE(s.has_published);
}

static void test_uptime_format(void)
{
    char b[16];
    gz_fmt_uptime(0, b, sizeof b);              TEST_ASSERT_EQUAL_STRING("0:00:00", b);
    gz_fmt_uptime(61000, b, sizeof b);          TEST_ASSERT_EQUAL_STRING("0:01:01", b);
    gz_fmt_uptime(3723000, b, sizeof b);        TEST_ASSERT_EQUAL_STRING("1:02:03", b);
    gz_fmt_uptime(90061000UL, b, sizeof b);     TEST_ASSERT_EQUAL_STRING("1d 1:01:01", b);
}

static void test_age_format(void)
{
    char b[16];
    gz_status_t s;
    gz_status_init(&s);
    gz_fmt_pub_age(&s, 5000, b, sizeof b);      TEST_ASSERT_EQUAL_STRING("never", b);
    gz_status_note_publish(&s, GZ_RADIO_BLE, 1, 5000);
    gz_fmt_pub_age(&s, 5400, b, sizeof b);      TEST_ASSERT_EQUAL_STRING("0s", b);
    gz_fmt_pub_age(&s, 17000, b, sizeof b);     TEST_ASSERT_EQUAL_STRING("12s", b);
    gz_fmt_pub_age(&s, 5000 + 125000, b, sizeof b); TEST_ASSERT_EQUAL_STRING("2m05s", b);
}

/* millis() wraps at 2^32: age must still be the small forward distance. */
static void test_age_survives_wrap(void)
{
    char b[16];
    gz_status_t s;
    gz_status_init(&s);
    gz_status_note_publish(&s, GZ_RADIO_WIFI, 1, 0xFFFFF000u);   /* ~4.1 s pre-wrap */
    gz_fmt_pub_age(&s, 0x00000400u, b, sizeof b);                /* 5.12 s later */
    TEST_ASSERT_EQUAL_STRING("5s", b);
}

/* A too-small buffer is truncated and still NUL-terminated, never overrun. */
static void test_formatters_bounded(void)
{
    char b[4];
    memset(b, 'X', sizeof b);
    gz_fmt_uptime(90061000UL, b, sizeof b);
    TEST_ASSERT_EQUAL_CHAR('\0', b[3]);
    TEST_ASSERT_TRUE(strlen(b) < sizeof b);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_counters_accumulate_per_radio);
    RUN_TEST(test_uptime_format);
    RUN_TEST(test_age_format);
    RUN_TEST(test_age_survives_wrap);
    RUN_TEST(test_formatters_bounded);
    return UNITY_END();
}
