/* Native Unity test for godzilla_node/main/gz_board.h — the Waveshare 1.85B
 * pin map. The header's static_asserts already guard pin collisions at compile
 * time; this pins the documented values so an accidental edit to the map is
 * caught, and confirms the touch and RTC deliberately share one I2C bus. */
#include <unity.h>
#include "../../godzilla_node/main/gz_board.h"

void setUp(void) {}
void tearDown(void) {}

static void test_documented_lcd_pins(void)
{
    TEST_ASSERT_EQUAL_INT(21, GZ_PIN_LCD_CS);
    TEST_ASSERT_EQUAL_INT(40, GZ_PIN_LCD_PCLK);
    TEST_ASSERT_EQUAL_INT(46, GZ_PIN_LCD_DATA0);
    TEST_ASSERT_EQUAL_INT(45, GZ_PIN_LCD_DATA1);
    TEST_ASSERT_EQUAL_INT(42, GZ_PIN_LCD_DATA2);
    TEST_ASSERT_EQUAL_INT(41, GZ_PIN_LCD_DATA3);
    TEST_ASSERT_EQUAL_INT(3,  GZ_PIN_LCD_RST);
    TEST_ASSERT_EQUAL_INT(5,  GZ_PIN_LCD_BL);
    TEST_ASSERT_EQUAL_INT(360, GZ_LCD_WIDTH);
    TEST_ASSERT_EQUAL_INT(360, GZ_LCD_HEIGHT);
}

static void test_touch_and_rtc_share_i2c(void)
{
    TEST_ASSERT_EQUAL_INT(10, GZ_PIN_I2C_SCL);
    TEST_ASSERT_EQUAL_INT(11, GZ_PIN_I2C_SDA);
    TEST_ASSERT_EQUAL_HEX8(0x15, GZ_I2C_ADDR_TOUCH);
    TEST_ASSERT_EQUAL_HEX8(0x51, GZ_I2C_ADDR_RTC);
    TEST_ASSERT_NOT_EQUAL(GZ_I2C_ADDR_TOUCH, GZ_I2C_ADDR_RTC);
}

static void test_control_lines(void)
{
    TEST_ASSERT_EQUAL_INT(1, GZ_PIN_TOUCH_RST);
    TEST_ASSERT_EQUAL_INT(4, GZ_PIN_TOUCH_INT);
    TEST_ASSERT_EQUAL_INT(6, GZ_PIN_RTC_INT);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_documented_lcd_pins);
    RUN_TEST(test_touch_and_rtc_share_i2c);
    RUN_TEST(test_control_lines);
    return UNITY_END();
}
