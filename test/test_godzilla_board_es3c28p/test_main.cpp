/* Native Unity test for the GODZILLA ES3C28P board map (2.8" ESP32-S3
 * Display): godzilla_node/main/gz_board.h with GZ_BOARD_ES3C28P defined, as
 * [env:godzilla-es3c28p] builds it. Pins against the lcdwiki table, the
 * capability flags (no RTC / IMU / gauge / mic ADC — and no address macros for
 * them, so code can't reach an absent part), the scan labels, the amp-off
 * level and the HELLO name. */
#define GZ_BOARD_ES3C28P 1
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_board.h"
#include "../../godzilla_node/main/gz_i2c_scan.h"
#include "../../godzilla_node/main/gz_hello.h"

void setUp(void) {}
void tearDown(void) {}

static void test_selects_es3c28p(void)
{
    TEST_ASSERT_EQUAL_STRING("2.8in ESP32-S3 Display ES3C28P", GZ_BOARD_NAME);
    TEST_ASSERT_EQUAL_INT(0, GZ_LCD_ROUND);
    TEST_ASSERT_EQUAL_INT(1, GZ_BOARD_HAS_UI);    /* ILI9341V + FT6336G driven */
    TEST_ASSERT_EQUAL_INT(1, GZ_BOARD_HAS_SD);    /* SDMMC + Recon on now    */
}

static void test_absent_parts_are_compiled_out(void)
{
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_RTC);
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_IMU);
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_GAUGE);
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_MIC_ADC);
#if defined(GZ_I2C_ADDR_RTC) || defined(GZ_I2C_ADDR_IMU) || \
    defined(GZ_I2C_ADDR_GAUGE) || defined(GZ_I2C_ADDR_MIC_ADC)
    TEST_FAIL_MESSAGE("an absent part has an I2C address macro");
#endif
}

static void test_lcd_pins(void)
{
    TEST_ASSERT_EQUAL_INT(10, GZ_PIN_LCD_CS);
    TEST_ASSERT_EQUAL_INT(46, GZ_PIN_LCD_DC);
    TEST_ASSERT_EQUAL_INT(12, GZ_PIN_LCD_SCK);
    TEST_ASSERT_EQUAL_INT(11, GZ_PIN_LCD_MOSI);
    TEST_ASSERT_EQUAL_INT(13, GZ_PIN_LCD_MISO);
    TEST_ASSERT_EQUAL_INT(45, GZ_PIN_LCD_BL);
    TEST_ASSERT_EQUAL_INT(240, GZ_LCD_WIDTH);
    TEST_ASSERT_EQUAL_INT(320, GZ_LCD_HEIGHT);
#ifdef GZ_PIN_LCD_RST
    TEST_FAIL_MESSAGE("LCD reset is on EN on this board; no GPIO");
#endif
}

static void test_touch_sd_audio(void)
{
    TEST_ASSERT_EQUAL_INT(15, GZ_PIN_I2C_SCL);
    TEST_ASSERT_EQUAL_INT(16, GZ_PIN_I2C_SDA);
    TEST_ASSERT_EQUAL_HEX8(0x38, GZ_I2C_ADDR_TOUCH);
    TEST_ASSERT_EQUAL_INT(18, GZ_PIN_TOUCH_RST);
    TEST_ASSERT_EQUAL_INT(17, GZ_PIN_TOUCH_INT);

    TEST_ASSERT_EQUAL_INT(38, GZ_PIN_SD_CLK);
    TEST_ASSERT_EQUAL_INT(40, GZ_PIN_SD_CMD);
    TEST_ASSERT_EQUAL_INT(39, GZ_PIN_SD_D0);
    TEST_ASSERT_EQUAL_INT(41, GZ_PIN_SD_D1);
    TEST_ASSERT_EQUAL_INT(48, GZ_PIN_SD_D2);
    TEST_ASSERT_EQUAL_INT(47, GZ_PIN_SD_D3);

    /* FM8002E: low = enabled, so the boot drive that holds it off is high. */
    TEST_ASSERT_EQUAL_INT(1, GZ_PIN_AMP_EN);
    TEST_ASSERT_EQUAL_INT(1, GZ_AMP_OFF_LEVEL);
    TEST_ASSERT_EQUAL_INT(9, GZ_PIN_BATT_ADC);
}

static void test_scan_labels(void)
{
    TEST_ASSERT_EQUAL_STRING("FT6336G touch",          gz_i2c_expected_device(0x38));
    TEST_ASSERT_EQUAL_STRING("ES8311 codec? (unused)", gz_i2c_expected_device(0x18));
    /* Other boards' parts are not expected here. */
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x51));   /* RTC   */
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x6B));   /* IMU   */
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x34));   /* PMIC  */
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x40));   /* mic   */
}

static void test_hello_name(void)
{
    posei_hello_t h;
    gz_build_hello(&h, 1);
    TEST_ASSERT_EQUAL_STRING("GODZILLA28", h.name);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_selects_es3c28p);
    RUN_TEST(test_absent_parts_are_compiled_out);
    RUN_TEST(test_lcd_pins);
    RUN_TEST(test_touch_sd_audio);
    RUN_TEST(test_scan_labels);
    RUN_TEST(test_hello_name);
    return UNITY_END();
}
