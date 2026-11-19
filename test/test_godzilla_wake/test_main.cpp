/* Native Unity test for godzilla_node/main/gz_wake.h — the idle-dim / wake
 * policy (#88 §3.7): dims once after the idle time, brightens once on the next
 * activity, never repeats a transition, can be disabled, and is safe across
 * the millis() wrap. Built for the AMOLED to check its lower dim level. */
#define GZ_BOARD_AMOLED206 1
#include <unity.h>
#include "../../godzilla_node/main/gz_wake.h"

void setUp(void) {}
void tearDown(void) {}

static void test_dim_then_wake(void)
{
    gz_wake_t w; gz_wake_init(&w, 1000);
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, 1000 + 59999, 60000));
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_TO_DIM,    gz_wake_update(&w, 1000 + 60000, 60000));
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, 1000 + 90000, 60000));  /* once */
    gz_wake_activity(&w, 95000);
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_TO_BRIGHT, gz_wake_update(&w, 95000, 60000));
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, 96000, 60000));          /* once */
}

static void test_activity_keeps_it_bright(void)
{
    gz_wake_t w; gz_wake_init(&w, 0);
    for (uint32_t t = 0; t < 300000; t += 30000) {   /* activity every 30 s */
        gz_wake_activity(&w, t);
        TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, t + 29999, 60000));
    }
}

static void test_disabled(void)
{
    gz_wake_t w; gz_wake_init(&w, 0);
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, 10u * 60u * 1000u, 0));
    TEST_ASSERT_FALSE(w.dimmed);
}

static void test_millis_wrap(void)
{
    gz_wake_t w; gz_wake_init(&w, 0xFFFFFF00u);
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_NO_CHANGE, gz_wake_update(&w, 0x00000100u, 60000));     /* 512 ms */
    TEST_ASSERT_EQUAL_INT(GZ_WAKE_TO_DIM,    gz_wake_update(&w, 0xFFFFFF00u + 60000u, 60000));
}

static void test_levels_and_defaults(void)
{
    TEST_ASSERT_EQUAL_UINT32(60000u, GZ_UI_DIM_AFTER_MS);
    TEST_ASSERT_EQUAL_INT(1, GZ_UI_WAKE_ON_MOTION);
    TEST_ASSERT_EQUAL_UINT32(8u, GZ_UI_DIM_PCT);        /* AMOLED: lower, for burn-in */
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_dim_then_wake);
    RUN_TEST(test_activity_keeps_it_bright);
    RUN_TEST(test_disabled);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_levels_and_defaults);
    return UNITY_END();
}
