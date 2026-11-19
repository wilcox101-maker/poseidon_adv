/* Native Unity test for godzilla_node/main/gz_rotate.h — which display rotation
 * the IMU orientation maps to (#98 §1.3): all four on a square panel, 0/180
 * only on a non-square one (the layout is compile-time W x H), the reverse
 * option, and the disable defaults. The pixel rotation itself is LVGL's. */
#include <unity.h>
#include "../../godzilla_node/main/gz_rotate.h"

void setUp(void) {}
void tearDown(void) {}

static void test_square_takes_all_four(void)          /* 1.85B 360x360 */
{
    for (uint8_t r = 0; r < 4; ++r)
        TEST_ASSERT_EQUAL_UINT8(r, gz_rotate_target(r, 0, 360, 360, false));
}

static void test_non_square_flips_only(void)           /* 2.06 410x502 */
{
    TEST_ASSERT_EQUAL_UINT8(0, gz_rotate_target(0, 2, 410, 502, false));
    TEST_ASSERT_EQUAL_UINT8(2, gz_rotate_target(2, 0, 410, 502, false));
    /* A sideways tilt holds whatever is current. */
    TEST_ASSERT_EQUAL_UINT8(0, gz_rotate_target(1, 0, 410, 502, false));
    TEST_ASSERT_EQUAL_UINT8(2, gz_rotate_target(3, 2, 410, 502, false));
    TEST_ASSERT_EQUAL_UINT8(2, gz_rotate_target(1, 6, 410, 502, false));   /* current folded */
}

static void test_reverse(void)
{
    TEST_ASSERT_EQUAL_UINT8(3, gz_rotate_target(1, 0, 360, 360, true));
    TEST_ASSERT_EQUAL_UINT8(1, gz_rotate_target(3, 0, 360, 360, true));
    TEST_ASSERT_EQUAL_UINT8(2, gz_rotate_target(2, 0, 360, 360, true));    /* 180 unchanged */
    TEST_ASSERT_EQUAL_UINT8(0, gz_rotate_target(4, 1, 360, 360, true));    /* input folded */
}

static void test_defaults(void)
{
    TEST_ASSERT_EQUAL_INT(1, GZ_UI_AUTO_ROTATE);
    TEST_ASSERT_EQUAL_INT(0, GZ_UI_ROT_REVERSE);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_square_takes_all_four);
    RUN_TEST(test_non_square_flips_only);
    RUN_TEST(test_reverse);
    RUN_TEST(test_defaults);
    return UNITY_END();
}
