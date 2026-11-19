/* Native Unity test for godzilla_node/main/gz_touch_ft.h — the pure FocalTech
 * report parser (FT3168 / FT6336G), built as the ES3C28P (240x320) so the
 * clamp runs against a real FocalTech board. The 2.06's 410x502 clamp is in
 * test_godzilla_board_amoled206. Wire / INT are on-target only. */
#define GZ_BOARD_ES3C28P 1
#include <unity.h>
#include "../../godzilla_node/main/gz_touch.h"   /* pulls in gz_touch_ft.h on FT boards */

void setUp(void) {}
void tearDown(void) {}

/* Build a 0x02..0x06 block: points, event, 12-bit x/y, touch id. */
static void block(uint8_t out[5], uint8_t points, uint8_t evt, uint16_t x, uint16_t y,
                  uint8_t id)
{
    out[0] = points;
    out[1] = (uint8_t)((evt << 6) | ((x >> 8) & 0x0F));
    out[2] = (uint8_t)(x & 0xFF);
    out[3] = (uint8_t)((id << 4) | ((y >> 8) & 0x0F));
    out[4] = (uint8_t)(y & 0xFF);
}

static void test_board_wiring(void)
{
    TEST_ASSERT_EQUAL_INT(1, GZ_BOARD_TOUCH_FT);
    TEST_ASSERT_EQUAL_HEX8(0x02, GZ_TOUCH_FT_REG_STATUS);
    TEST_ASSERT_EQUAL_INT(5, GZ_TOUCH_FT_REPORT_BYTES);
}

static void test_press_and_contact(void)
{
    uint8_t r[5]; gz_touch_point_t p;
    block(r, 1, GZ_TOUCH_FT_EVT_DOWN, 120, 200, 0);
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_TRUE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(1, p.fingers);
    TEST_ASSERT_EQUAL_UINT8(GZ_TOUCH_EVT_DOWN, p.event);
    TEST_ASSERT_EQUAL_UINT16(120, p.x);
    TEST_ASSERT_EQUAL_UINT16(200, p.y);
    TEST_ASSERT_EQUAL_UINT8(GZ_GEST_NONE, p.gesture);

    block(r, 1, GZ_TOUCH_FT_EVT_CONTACT, 5, 6, 0);
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_TRUE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(GZ_TOUCH_EVT_CONTACT, p.event);
}

static void test_release_cases(void)
{
    uint8_t r[5]; gz_touch_point_t p;
    block(r, 1, GZ_TOUCH_FT_EVT_UP, 50, 50, 0);           /* lift-up */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_FALSE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(GZ_TOUCH_EVT_UP, p.event);

    block(r, 1, GZ_TOUCH_FT_EVT_NONE, 50, 50, 0);         /* no event */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_FALSE(p.pressed);

    block(r, 0, GZ_TOUCH_FT_EVT_CONTACT, 50, 50, 0);      /* zero points */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_FALSE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(0, p.fingers);

    block(r, 0x0F, GZ_TOUCH_FT_EVT_CONTACT, 50, 50, 0);   /* idle 0x0F */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_FALSE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(0, p.fingers);

    block(r, 3, GZ_TOUCH_FT_EVT_CONTACT, 50, 50, 0);      /* > 2 points */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_FALSE(p.pressed);
}

static void test_two_points_uses_first(void)
{
    uint8_t r[5]; gz_touch_point_t p;
    block(r, 2, GZ_TOUCH_FT_EVT_CONTACT, 33, 44, 1);
    r[0] |= 0x30;                                          /* upper nibble ignored */
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_TRUE(p.pressed);
    TEST_ASSERT_EQUAL_UINT8(1, p.fingers);
    TEST_ASSERT_EQUAL_UINT16(33, p.x);
    TEST_ASSERT_EQUAL_UINT16(44, p.y);
}

static void test_12bit_and_clamp(void)
{
    uint8_t r[5]; gz_touch_point_t p;
    /* Touch id bits in P1_YH must not leak into Y. */
    block(r, 1, GZ_TOUCH_FT_EVT_CONTACT, 239, 319, 0x0F);
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_EQUAL_UINT16(239, p.x);
    TEST_ASSERT_EQUAL_UINT16(319, p.y);
    /* Off-panel coordinates clamp to the 240x320 panel. */
    block(r, 1, GZ_TOUCH_FT_EVT_CONTACT, 0x0FFF, 0x0400, 0);
    gz_touch_ft_parse(r, &p);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_WIDTH - 1, p.x);
    TEST_ASSERT_EQUAL_UINT16(GZ_LCD_HEIGHT - 1, p.y);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_board_wiring);
    RUN_TEST(test_press_and_contact);
    RUN_TEST(test_release_cases);
    RUN_TEST(test_two_points_uses_first);
    RUN_TEST(test_12bit_and_clamp);
    return UNITY_END();
}
