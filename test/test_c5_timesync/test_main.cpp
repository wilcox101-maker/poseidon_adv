/* Native Unity test for src/c5_timesync.h — the pure decision logic behind the
 * Cardputer → node time-sync (CMD_TIMESET). No Arduino/ESP-NOW, so the header
 * is included directly. Covers:
 *   1. clock validity sentinel (an unset boot clock is rejected, a real GPS
 *      epoch accepted);
 *   2. the per-peer "due a time-set?" rule: never without a valid clock, always
 *      on the first chance, then once per resync window — wrap-safe.
 */
#include <unity.h>
#include "../../src/c5_timesync.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- 1. clock validity --------------------------------------------------- */
static void test_time_validity_sentinel(void)
{
    TEST_ASSERT_FALSE(c5_time_is_valid(0));                       /* unset */
    TEST_ASSERT_FALSE(c5_time_is_valid(1000u));                   /* boot-relative */
    TEST_ASSERT_FALSE(c5_time_is_valid(C5_TIME_VALID_MIN - 1u));  /* just below */
    TEST_ASSERT_TRUE (c5_time_is_valid(C5_TIME_VALID_MIN));       /* exactly at */
    TEST_ASSERT_TRUE (c5_time_is_valid(1763000000u));             /* 2025 GPS fix */
}

/* ---- 2. due rule --------------------------------------------------------- */
static void test_never_due_without_valid_clock(void)
{
    /* No valid clock -> never send, even if never sent before. */
    TEST_ASSERT_FALSE(c5_timeset_due(false, 100000u, 0u, false, C5_TIMESET_RESYNC_MS));
    TEST_ASSERT_FALSE(c5_timeset_due(false, 100000u, 0u, true,  C5_TIMESET_RESYNC_MS));
}

static void test_first_send_always_due(void)
{
    /* Valid clock, never sent -> due regardless of the timers. */
    TEST_ASSERT_TRUE(c5_timeset_due(true, 0u,      0u,      false, C5_TIMESET_RESYNC_MS));
    TEST_ASSERT_TRUE(c5_timeset_due(true, 500u,    12345u,  false, C5_TIMESET_RESYNC_MS));
}

static void test_resync_window(void)
{
    const uint32_t last = 1000u;
    /* Within the window after a send -> not due. */
    TEST_ASSERT_FALSE(c5_timeset_due(true, last + 1u,                       last, true, C5_TIMESET_RESYNC_MS));
    TEST_ASSERT_FALSE(c5_timeset_due(true, last + C5_TIMESET_RESYNC_MS - 1u, last, true, C5_TIMESET_RESYNC_MS));
    /* Exactly at and past the window -> due again. */
    TEST_ASSERT_TRUE (c5_timeset_due(true, last + C5_TIMESET_RESYNC_MS,      last, true, C5_TIMESET_RESYNC_MS));
    TEST_ASSERT_TRUE (c5_timeset_due(true, last + C5_TIMESET_RESYNC_MS + 50u, last, true, C5_TIMESET_RESYNC_MS));
}

static void test_resync_wraps_millis(void)
{
    /* millis() wrapped past 0 after the last send near UINT32_MAX: the unsigned
     * elapsed must still read as a full window, not a huge negative. */
    const uint32_t last = 0xFFFFFF00u;          /* ~256 ms before wrap */
    uint32_t now = last + C5_TIMESET_RESYNC_MS;  /* wraps around 0 */
    TEST_ASSERT_TRUE(c5_timeset_due(true, now, last, true, C5_TIMESET_RESYNC_MS));
    /* A small post-wrap elapsed is still within the window -> not due. */
    TEST_ASSERT_FALSE(c5_timeset_due(true, last + 10u, last, true, C5_TIMESET_RESYNC_MS));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_time_validity_sentinel);
    RUN_TEST(test_never_due_without_valid_clock);
    RUN_TEST(test_first_send_always_due);
    RUN_TEST(test_resync_window);
    RUN_TEST(test_resync_wraps_millis);
    return UNITY_END();
}
