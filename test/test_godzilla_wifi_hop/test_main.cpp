/* Native Unity tests for gz_next_channel() (godzilla_node/main/gz_wifi_scan.h),
 * the pure channel-hop step. It must walk 1..13 and wrap 13 -> 1, and fold any
 * out-of-range current back into the 1..13 band. */
#include <unity.h>
#include "../../godzilla_node/main/gz_wifi_scan.h"

void setUp(void) {}
void tearDown(void) {}

static void test_walks_1_to_13(void)
{
    uint8_t ch = GZ_WIFI_CH_MIN;
    for (int expect = 2; expect <= GZ_WIFI_CH_MAX; expect++) {
        ch = gz_next_channel(ch);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)expect, ch);
    }
}

static void test_wraps_13_to_1(void)
{
    TEST_ASSERT_EQUAL_UINT8(1, gz_next_channel(GZ_WIFI_CH_MAX));
}

/* A full cycle returns to the start and visits every channel exactly once. */
static void test_full_cycle_covers_band(void)
{
    bool seen[GZ_WIFI_CH_MAX + 1] = { false };
    uint8_t ch = GZ_WIFI_CH_MIN;
    seen[ch] = true;
    for (int i = 0; i < GZ_WIFI_CH_MAX - 1; i++) {
        ch = gz_next_channel(ch);
        TEST_ASSERT_TRUE(ch >= GZ_WIFI_CH_MIN && ch <= GZ_WIFI_CH_MAX);
        seen[ch] = true;
    }
    for (int c = GZ_WIFI_CH_MIN; c <= GZ_WIFI_CH_MAX; c++)
        TEST_ASSERT_TRUE(seen[c]);
    TEST_ASSERT_EQUAL_UINT8(1, gz_next_channel(ch));   /* back to start */
}

static void test_out_of_range_folds_back(void)
{
    TEST_ASSERT_EQUAL_UINT8(1, gz_next_channel(0));
    uint8_t ch = gz_next_channel(200);
    TEST_ASSERT_TRUE(ch >= GZ_WIFI_CH_MIN && ch <= GZ_WIFI_CH_MAX);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_walks_1_to_13);
    RUN_TEST(test_wraps_13_to_1);
    RUN_TEST(test_full_cycle_covers_band);
    RUN_TEST(test_out_of_range_folds_back);
    return UNITY_END();
}
