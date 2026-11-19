/* Native Unity test for godzilla_node/main/gz_power.h — the pure, read-only
 * power decode: AXP2101 register snapshot (2.06) and the ADC divider (ES3C28P).
 * The rule under test is "show only what the hardware measured": no
 * percentage without a running gauge, no voltage without a running ADC or an
 * implausible value, nothing at all without a battery. I2C / ADC are on-target. */
#include <unity.h>
#include "../../godzilla_node/main/gz_power.h"

void setUp(void) {}
void tearDown(void) {}

static gz_axp2101_regs_t regs(uint8_t st1, uint8_t st2, uint8_t gauge, uint8_t adc,
                              uint16_t mv, uint8_t pct)
{
    gz_axp2101_regs_t r;
    r.status1 = st1; r.status2 = st2; r.gauge_ctrl = gauge; r.adc_ctrl = adc;
    r.vbat_h = (uint8_t)((mv >> 8) & 0x1F); r.vbat_l = (uint8_t)(mv & 0xFF); r.pct = pct;
    return r;
}

static void test_axp_full_reading(void)
{
    /* VBUS good + battery present, charging, gauge + ADC on, 4012 mV, 87 %. */
    gz_axp2101_regs_t r = regs(0x28, 0x20, 0x08, 0x01, 4012, 87);
    gz_power_t p;
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_TRUE(p.vbus);
    TEST_ASSERT_TRUE(p.batt);
    TEST_ASSERT_EQUAL_INT(GZ_CHG_CHARGING, p.chg);
    TEST_ASSERT_TRUE(p.pct_valid);  TEST_ASSERT_EQUAL_UINT8(87, p.pct);
    TEST_ASSERT_TRUE(p.mv_valid);   TEST_ASSERT_EQUAL_UINT16(4012, p.mv);
}

static void test_axp_charge_states(void)
{
    gz_power_t p; gz_axp2101_regs_t r;
    r = regs(0x08, 0x00, 0, 0, 0, 0); gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_EQUAL_INT(GZ_CHG_STANDBY, p.chg);
    r = regs(0x08, 0x40, 0, 0, 0, 0); gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_EQUAL_INT(GZ_CHG_DISCHARGING, p.chg);
    r = regs(0x08, 0x60, 0, 0, 0, 0); gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_EQUAL_INT(GZ_CHG_UNKNOWN, p.chg);
    /* Low status2 bits (charger sub-state) don't disturb the decode. */
    r = regs(0x08, 0x27, 0, 0, 0, 0); gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_EQUAL_INT(GZ_CHG_CHARGING, p.chg);
}

static void test_axp_measurements_off_are_not_shown(void)
{
    gz_power_t p;
    /* Battery present but the gauge and the voltage ADC were never enabled:
     * the bytes are whatever the part holds, so nothing is shown. */
    gz_axp2101_regs_t r = regs(0x08, 0x40, 0x00, 0x00, 3900, 55);
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_TRUE(p.batt);
    TEST_ASSERT_FALSE(p.pct_valid);
    TEST_ASSERT_FALSE(p.mv_valid);
    TEST_ASSERT_EQUAL_UINT8(0, p.pct);
    TEST_ASSERT_EQUAL_UINT16(0, p.mv);
    /* Gauge on, ADC off: percentage only. */
    r = regs(0x08, 0x40, 0x08, 0x00, 3900, 55);
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_TRUE(p.pct_valid);
    TEST_ASSERT_FALSE(p.mv_valid);
}

static void test_axp_no_battery_and_bad_values(void)
{
    gz_power_t p;
    /* USB only, no battery: no reading even with both measurements on. */
    gz_axp2101_regs_t r = regs(0x20, 0x00, 0x08, 0x01, 4200, 100);
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_TRUE(p.vbus);
    TEST_ASSERT_FALSE(p.batt);
    TEST_ASSERT_FALSE(p.pct_valid);
    TEST_ASSERT_FALSE(p.mv_valid);
    /* Out-of-range percentage / implausible voltage are refused. */
    r = regs(0x08, 0x00, 0x08, 0x01, 1200, 140);
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_FALSE(p.pct_valid);
    TEST_ASSERT_FALSE(p.mv_valid);
    /* 13-bit packing: high byte's top 3 bits are masked. */
    r = regs(0x08, 0x00, 0x08, 0x01, 3700, 50);
    r.vbat_h |= 0xE0;
    gz_power_axp2101_decode(&r, &p);
    TEST_ASSERT_EQUAL_UINT16(3700, p.mv);
}

static void test_adc_divider(void)
{
    gz_power_t p;
    gz_power_adc_decode(1960, 2, 1, &p);              /* 2:1 divider -> 3920 mV */
    TEST_ASSERT_TRUE(p.mv_valid);
    TEST_ASSERT_EQUAL_UINT16(3920, p.mv);
    TEST_ASSERT_FALSE(p.pct_valid);                   /* never a guessed % */
    TEST_ASSERT_EQUAL_INT(GZ_CHG_UNKNOWN, p.chg);
    gz_power_adc_decode(0, 2, 1, &p);                 /* no battery / floating */
    TEST_ASSERT_FALSE(p.mv_valid);
    gz_power_adc_decode(2600, 2, 1, &p);              /* 5.2 V: not a cell */
    TEST_ASSERT_FALSE(p.mv_valid);
    gz_power_adc_decode(1000, 2, 0, &p);              /* bad divider */
    TEST_ASSERT_FALSE(p.mv_valid);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_axp_full_reading);
    RUN_TEST(test_axp_charge_states);
    RUN_TEST(test_axp_measurements_off_are_not_shown);
    RUN_TEST(test_axp_no_battery_and_bad_values);
    RUN_TEST(test_adc_divider);
    return UNITY_END();
}
