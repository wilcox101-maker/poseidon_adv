/* Native Unity tests for gz_st77916_panel_rev() (godzilla_node/main/gz_st77916_rev.h).
 *
 * The 1.85B ships with two ST77916 panel revisions that need different vendor
 * init tables. Waveshare's own demo (examples/Arduino-V3.2.0/examples/
 * 01_lvgl_demo/Display_ST77916.cpp in waveshareteam/ESP32-S3-Touch-LCD-1.85B)
 * reads register 0x04 at a low clock and picks:
 *   00 7F 7F 7F -> table version 1
 *   00 02 7F 7F -> table version 2
 * Anything else is an unknown revision: we must NOT silently guess a table. */
#include <unity.h>
#include "../../godzilla_node/main/gz_st77916_rev.h"

void setUp(void) {}
void tearDown(void) {}

static void test_rev1_signature(void)
{
    const uint8_t r[4] = { 0x00, 0x7F, 0x7F, 0x7F };
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_1, gz_st77916_panel_rev(r));
}

static void test_rev2_signature(void)
{
    const uint8_t r[4] = { 0x00, 0x02, 0x7F, 0x7F };
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_2, gz_st77916_panel_rev(r));
}

/* A failed read leaves zeros; a floating bus reads 0xFF. Neither is a known
 * panel — the driver then falls back to the component's built-in table and
 * logs it, rather than force-feeding a vendor table to the wrong glass. */
static void test_unknown_signatures(void)
{
    const uint8_t zeros[4] = { 0x00, 0x00, 0x00, 0x00 };
    const uint8_t ffs[4]   = { 0xFF, 0xFF, 0xFF, 0xFF };
    const uint8_t near1[4] = { 0x01, 0x7F, 0x7F, 0x7F };  /* byte 0 must be 0 */
    const uint8_t near2[4] = { 0x00, 0x02, 0x7F, 0x7E };  /* byte 3 must be 7F */
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_UNKNOWN, gz_st77916_panel_rev(zeros));
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_UNKNOWN, gz_st77916_panel_rev(ffs));
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_UNKNOWN, gz_st77916_panel_rev(near1));
    TEST_ASSERT_EQUAL_INT(GZ_ST77916_REV_UNKNOWN, gz_st77916_panel_rev(near2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rev1_signature);
    RUN_TEST(test_rev2_signature);
    RUN_TEST(test_unknown_signatures);
    return UNITY_END();
}
