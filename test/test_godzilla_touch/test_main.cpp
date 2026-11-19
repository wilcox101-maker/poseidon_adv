/* Native Unity test for godzilla_node/main/gz_touch.h — the PURE CST816S logic
 * (6-byte report parse, event/pressed rules, orientation transform + clamping,
 * chip-ID and gesture name tables). The Wire reads and the INT ISR are
 * on-target (ARDUINO-only) and not exercised here. A bit-mask or axis slip
 * fails CI, not the bench. */
#include <unity.h>
#include "../../godzilla_node/main/gz_touch.h"

void setUp(void) {}
void tearDown(void) {}

static void test_parse_contact_point(void)
{
    /* Finger held at (300, 45): event=contact(2), touch id 0, no gesture.
     * XposH/YposH carry dirty high bits that must not leak into X/Y. */
    const uint8_t r[GZ_TOUCH_REPORT_BYTES] = {
        0x00,                         /* gesture none         */
        0x01,                         /* one finger           */
        (uint8_t)(0x80 | 0x01),       /* event 2, X[11:8]=1   */
        0x2C,                         /* X = 0x12C = 300      */
        (uint8_t)(0x40 | 0x00),       /* touch id 1, Y[11:8]=0*/
        0x2D,                         /* Y = 45               */
    };
    gz_touch_point_t p;
    gz_touch_parse(r, &p);
    TEST_ASSERT_EQUAL_UINT16(300, p.x);
    TEST_ASSERT_EQUAL_UINT16(45, p.y);
    TEST_ASSERT_EQUAL_UINT8(GZ_TOUCH_EVT_CONTACT, p.event);
    TEST_ASSERT_EQUAL_UINT8(1, p.fingers);
    TEST_ASSERT_EQUAL_UINT8(GZ_GEST_NONE, p.gesture);
    TEST_ASSERT_TRUE(p.pressed);
}

static void test_parse_release_and_zero_fingers(void)
{
    /* Lift event with the finger count still 1: not pressed. */
    const uint8_t up[GZ_TOUCH_REPORT_BYTES] = { 0x05, 0x01, 0x40, 0x10, 0x00, 0x10 };
    gz_touch_point_t p;
    gz_touch_parse(up, &p);
    TEST_ASSERT_EQUAL_UINT8(GZ_TOUCH_EVT_UP, p.event);
    TEST_ASSERT_FALSE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(GZ_GEST_CLICK, p.gesture);

    /* Zero fingers with a stale "down" event: not pressed. */
    const uint8_t none[GZ_TOUCH_REPORT_BYTES] = { 0x00, 0x00, 0x00, 0x10, 0x00, 0x10 };
    gz_touch_parse(none, &p);
    TEST_ASSERT_EQUAL_UINT8(0, p.fingers);
    TEST_ASSERT_FALSE(p.pressed);

    /* A bogus finger count is clamped to 1. */
    const uint8_t two[GZ_TOUCH_REPORT_BYTES] = { 0x00, 0x02, 0x00, 0x10, 0x00, 0x10 };
    gz_touch_parse(two, &p);
    TEST_ASSERT_EQUAL_UINT8(1, p.fingers);
    TEST_ASSERT_TRUE(p.pressed);
}

static void test_parse_clamps_out_of_range(void)
{
    /* 12-bit max (4095, 4095) must clamp onto the 360x360 panel. */
    const uint8_t r[GZ_TOUCH_REPORT_BYTES] = { 0x00, 0x01, 0x8F, 0xFF, 0x0F, 0xFF };
    gz_touch_point_t p;
    gz_touch_parse(r, &p);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_WIDTH - 1, p.x);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_HEIGHT - 1, p.y);
}

static void test_orient_swap_and_mirror(void)
{
    uint16_t x, y;
    gz_touch_orient(10, 20, false, false, false, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(10, x);
    TEST_ASSERT_EQUAL_UINT16(20, y);

    gz_touch_orient(10, 20, true, false, false, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(20, x);
    TEST_ASSERT_EQUAL_UINT16(10, y);

    gz_touch_orient(10, 20, false, true, true, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_WIDTH - 1 - 10, x);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_HEIGHT - 1 - 20, y);

    /* Clamp happens before mirroring: an over-range X mirrors to 0. */
    gz_touch_orient(1000, 0, false, true, false, &x, &y);
    TEST_ASSERT_EQUAL_UINT16(0, x);
}

static void test_default_build_is_identity(void)
{
    /* The 1.85B demo maps touch 1:1; the shipped defaults must match it. */
    TEST_ASSERT_EQUAL_INT(0, GZ_TOUCH_SWAP_XY);
    TEST_ASSERT_EQUAL_INT(0, GZ_TOUCH_MIRROR_X);
    TEST_ASSERT_EQUAL_INT(0, GZ_TOUCH_MIRROR_Y);
}

static void test_name_tables(void)
{
    TEST_ASSERT_EQUAL_STRING("CST816S", gz_touch_chip_name(0xB4));
    TEST_ASSERT_EQUAL_STRING("CST816T", gz_touch_chip_name(0xB5));
    TEST_ASSERT_EQUAL_STRING("CST816D", gz_touch_chip_name(0xB6));
    TEST_ASSERT_EQUAL_STRING("CST716",  gz_touch_chip_name(0x20));
    TEST_ASSERT_EQUAL_STRING("unknown", gz_touch_chip_name(0x00));

    TEST_ASSERT_EQUAL_STRING("swipe left", gz_touch_gesture_name(GZ_GEST_SWIPE_LEFT));
    TEST_ASSERT_EQUAL_STRING("tap",        gz_touch_gesture_name(GZ_GEST_CLICK));
    TEST_ASSERT_EQUAL_STRING("long press", gz_touch_gesture_name(GZ_GEST_LONG_PRESS));
    TEST_ASSERT_EQUAL_STRING("?",          gz_touch_gesture_name(0x7E));
}

static void test_board_addresses(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x15, GZ_I2C_ADDR_TOUCH);
    TEST_ASSERT_EQUAL_INT(1, GZ_PIN_TOUCH_RST);
    TEST_ASSERT_EQUAL_INT(4, GZ_PIN_TOUCH_INT);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_contact_point);
    RUN_TEST(test_parse_release_and_zero_fingers);
    RUN_TEST(test_parse_clamps_out_of_range);
    RUN_TEST(test_orient_swap_and_mirror);
    RUN_TEST(test_default_build_is_identity);
    RUN_TEST(test_name_tables);
    RUN_TEST(test_board_addresses);
    return UNITY_END();
}
