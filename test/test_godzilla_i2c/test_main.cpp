/* Native Unity test for godzilla_node/main/gz_i2c_scan.h — the pure
 * address -> expected-device annotation used by the boot-time I2C probe.
 * The Wire probing itself is on-target (ARDUINO-only) and not exercised here;
 * this pins the lookup and that it stays single-sourced on gz_board.h. */
#include <unity.h>
#include "../../godzilla_node/main/gz_i2c_scan.h"

void setUp(void) {}
void tearDown(void) {}

static void test_known_addresses(void)
{
    TEST_ASSERT_EQUAL_STRING("CST816S touch", gz_i2c_expected_device(0x15));
    TEST_ASSERT_EQUAL_STRING("PCF85063 RTC",  gz_i2c_expected_device(0x51));
}

/* The annotation must follow the board map, not hard-coded literals, so a pin
 * map edit can never silently drift the boot log. */
static void test_tracks_board_macros(void)
{
    TEST_ASSERT_EQUAL_STRING("CST816S touch", gz_i2c_expected_device(GZ_I2C_ADDR_TOUCH));
    TEST_ASSERT_EQUAL_STRING("PCF85063 RTC",  gz_i2c_expected_device(GZ_I2C_ADDR_RTC));
}

/* The other parts on the 1.85B's bus are labelled (not driven) so the boot
 * log does not call them unknown. */
static void test_other_onboard_parts(void)
{
    TEST_ASSERT_EQUAL_STRING("QMI8658 IMU (unused)",           gz_i2c_expected_device(GZ_I2C_ADDR_IMU));
    TEST_ASSERT_EQUAL_STRING("BQ27220 battery gauge (unused)", gz_i2c_expected_device(GZ_I2C_ADDR_GAUGE));
    TEST_ASSERT_EQUAL_STRING("ES8311 codec? (unused)",         gz_i2c_expected_device(GZ_I2C_ADDR_CODEC));
    TEST_ASSERT_EQUAL_STRING("ES7210 mic ADC? (unused)",       gz_i2c_expected_device(GZ_I2C_ADDR_MIC_ADC));
}

static void test_unknown_addresses(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x00));
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x42));
    TEST_ASSERT_EQUAL_STRING("unknown", gz_i2c_expected_device(0x77));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_addresses);
    RUN_TEST(test_tracks_board_macros);
    RUN_TEST(test_other_onboard_parts);
    RUN_TEST(test_unknown_addresses);
    return UNITY_END();
}
