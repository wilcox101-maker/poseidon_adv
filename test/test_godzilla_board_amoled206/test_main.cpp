/* Native Unity test for the GODZILLA AMOLED-2.06 board map (port unit 1):
 * godzilla_node/main/gz_board.h with GZ_BOARD_AMOLED206 defined, as
 * [env:godzilla-amoled206] builds it. Pins against the Waveshare wiki table,
 * the capability flags, the boot-scan labels and the HELLO name. The 1.85B
 * map keeps its own test (test_godzilla_board). */
#define GZ_BOARD_AMOLED206 1
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_board.h"
#include "../../godzilla_node/main/gz_i2c_scan.h"
#include "../../godzilla_node/main/gz_hello.h"

void setUp(void) {}
void tearDown(void) {}

static void test_selects_amoled206(void)
{
    TEST_ASSERT_EQUAL_STRING("ESP32-S3-Touch-AMOLED-2.06", GZ_BOARD_NAME);
    TEST_ASSERT_EQUAL_INT(0, GZ_LCD_ROUND);
    /* Unit 1 is headless: display / touch / TF are not driven yet. */
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_UI);
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_SD);
}

static void test_lcd_pins(void)
{
    TEST_ASSERT_EQUAL_INT(12, GZ_PIN_LCD_CS);
    TEST_ASSERT_EQUAL_INT(11, GZ_PIN_LCD_PCLK);
    TEST_ASSERT_EQUAL_INT(4,  GZ_PIN_LCD_DATA0);
    TEST_ASSERT_EQUAL_INT(5,  GZ_PIN_LCD_DATA1);
    TEST_ASSERT_EQUAL_INT(6,  GZ_PIN_LCD_DATA2);
    TEST_ASSERT_EQUAL_INT(7,  GZ_PIN_LCD_DATA3);
    TEST_ASSERT_EQUAL_INT(8,  GZ_PIN_LCD_RST);
    TEST_ASSERT_EQUAL_INT(13, GZ_PIN_LCD_TE);
    TEST_ASSERT_EQUAL_INT(410, GZ_LCD_WIDTH);
    TEST_ASSERT_EQUAL_INT(502, GZ_LCD_HEIGHT);
    TEST_ASSERT_EQUAL_INT(22,  GZ_LCD_COL_GAP);
#ifdef GZ_PIN_LCD_BL
    TEST_FAIL_MESSAGE("AMOLED has no backlight pin");
#endif
}

static void test_i2c_bus_and_parts(void)
{
    TEST_ASSERT_EQUAL_INT(14, GZ_PIN_I2C_SCL);
    TEST_ASSERT_EQUAL_INT(15, GZ_PIN_I2C_SDA);
    TEST_ASSERT_EQUAL_HEX8(0x38, GZ_I2C_ADDR_TOUCH);
    TEST_ASSERT_EQUAL_INT(9,  GZ_PIN_TOUCH_RST);
    TEST_ASSERT_EQUAL_INT(38, GZ_PIN_TOUCH_INT);
    TEST_ASSERT_EQUAL_HEX8(0x51, GZ_I2C_ADDR_RTC);
    TEST_ASSERT_EQUAL_INT(39, GZ_PIN_RTC_INT);
    TEST_ASSERT_EQUAL_HEX8(0x6B, GZ_I2C_ADDR_IMU);
    TEST_ASSERT_EQUAL_INT(21, GZ_PIN_IMU_INT);
    TEST_ASSERT_EQUAL_HEX8(0x34, GZ_I2C_ADDR_GAUGE);
}

static void test_sd_spi_and_keys(void)
{
    TEST_ASSERT_EQUAL_INT(1,  GZ_PIN_SDSPI_MOSI);
    TEST_ASSERT_EQUAL_INT(2,  GZ_PIN_SDSPI_SCK);
    TEST_ASSERT_EQUAL_INT(3,  GZ_PIN_SDSPI_MISO);
    TEST_ASSERT_EQUAL_INT(17, GZ_PIN_SDSPI_CS);
    TEST_ASSERT_EQUAL_INT(0,  GZ_PIN_KEY_BOOT);
    TEST_ASSERT_EQUAL_INT(10, GZ_PIN_KEY_PWR);
#ifdef GZ_PIN_SD_CLK
    TEST_FAIL_MESSAGE("SDMMC pins must not be defined on the SPI-TF board");
#endif
}

static void test_scan_labels(void)
{
    TEST_ASSERT_EQUAL_STRING("FT3168 touch",          gz_i2c_expected_device(0x38));
    TEST_ASSERT_EQUAL_STRING("PCF85063 RTC",          gz_i2c_expected_device(0x51));
    TEST_ASSERT_EQUAL_STRING("QMI8658 IMU (unused)",  gz_i2c_expected_device(0x6B));
    TEST_ASSERT_EQUAL_STRING("AXP2101 PMIC (unused)", gz_i2c_expected_device(0x34));
    TEST_ASSERT_EQUAL_STRING("ES8311 codec? (unused)",   gz_i2c_expected_device(0x18));
    TEST_ASSERT_EQUAL_STRING("ES7210 mic ADC? (unused)", gz_i2c_expected_device(0x40));
    /* The 1.85B's touch / gauge addresses mean nothing here. */
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x15));
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x55));
}

static void test_hello_name(void)
{
    posei_hello_t h;
    gz_build_hello(&h, 123);
    TEST_ASSERT_EQUAL_STRING("GODZILLA206", h.name);
    TEST_ASSERT_EQUAL_UINT8(GZ_ROLE, h.role);
    TEST_ASSERT_TRUE(strlen(h.name) < sizeof h.name);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_selects_amoled206);
    RUN_TEST(test_lcd_pins);
    RUN_TEST(test_i2c_bus_and_parts);
    RUN_TEST(test_sd_spi_and_keys);
    RUN_TEST(test_scan_labels);
    RUN_TEST(test_hello_name);
    return UNITY_END();
}
