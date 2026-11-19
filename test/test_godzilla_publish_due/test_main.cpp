/* Native Unity tests for gz_publish_due() (godzilla_node/main/gz_ble_scan.h),
 * the pure predicate that decides when the node packs and broadcasts its
 * staging batch. It must:
 *   - stay false before the period elapses and go true at/after it,
 *   - go true immediately on a flush request (batch filled early),
 *   - remain correct across the millis() wraparound (unsigned arithmetic). */
#include <unity.h>
#include "../../godzilla_node/main/gz_ble_scan.h"

void setUp(void) {}
void tearDown(void) {}

static void test_not_due_before_period(void)
{
    TEST_ASSERT_FALSE(gz_publish_due(0, 500, 1000, false));
    TEST_ASSERT_FALSE(gz_publish_due(0, 999, 1000, false));
}

static void test_due_at_and_after_period(void)
{
    TEST_ASSERT_TRUE(gz_publish_due(0, 1000, 1000, false));
    TEST_ASSERT_TRUE(gz_publish_due(0, 5000, 1000, false));
}

static void test_flush_forces_publish(void)
{
    TEST_ASSERT_TRUE(gz_publish_due(0, 0, 1000, true));     /* even at t=0 */
    TEST_ASSERT_TRUE(gz_publish_due(500, 600, 1000, true)); /* mid-window */
}

/* millis() wraps at 2^32. Unsigned (now - last) must still measure elapsed. */
static void test_wraparound(void)
{
    uint32_t last = 0xFFFFFF00u;          /* ~256 ms before wrap */
    TEST_ASSERT_FALSE(gz_publish_due(last, 0x00000100u, 1000, false)); /* 512 ms */
    TEST_ASSERT_TRUE (gz_publish_due(last, 0x00000400u, 1000, false)); /* 1280 ms */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_not_due_before_period);
    RUN_TEST(test_due_at_and_after_period);
    RUN_TEST(test_flush_forces_publish);
    RUN_TEST(test_wraparound);
    return UNITY_END();
}
