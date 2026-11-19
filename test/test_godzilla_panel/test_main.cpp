/* Native Unity test for godzilla_node/main/gz_panel_tables.h — the CO5300 and
 * ILI9341V vendor init tables and their selection (#88 §2.3 "display
 * init-table selection"). Checks the invariants that a bad edit would break:
 * table chosen by panel id, every entry's byte count matches its data, sleep-
 * out comes before display-on with its 120 ms settle, the pixel format is
 * RGB565, the AMOLED starts dark, and the ILI9341V's inversion / BGR portrait
 * settings match the vendor file. The SPI / QSPI transfers are on-target. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_panel_tables.h"

void setUp(void) {}
void tearDown(void) {}

/* Index of the first entry with `cmd`, or -1. */
static int find(const gz_lcd_init_cmd_t *t, size_t n, int cmd)
{
    for (size_t i = 0; i < n; ++i) if (t[i].cmd == cmd) return (int)i;
    return -1;
}

static void check_common(const gz_lcd_init_cmd_t *t, size_t n)
{
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_TRUE(n > 0);
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(t[i].cmd >= 0 && t[i].cmd <= 0xFF, "8-bit command");
        if (t[i].data_bytes == 0) TEST_ASSERT_NULL(t[i].data);
        else                      TEST_ASSERT_NOT_NULL(t[i].data);
    }
    int slp = find(t, n, GZ_DCS_SLPOUT), on = find(t, n, GZ_DCS_DISPON);
    TEST_ASSERT_TRUE(slp >= 0 && on >= 0);
    TEST_ASSERT_TRUE_MESSAGE(slp < on, "sleep-out before display-on");
    TEST_ASSERT_TRUE_MESSAGE(t[slp].delay_ms >= 120, "120 ms after sleep-out");
    int fmt = find(t, n, GZ_DCS_COLMOD);
    TEST_ASSERT_TRUE(fmt >= 0);
    TEST_ASSERT_EQUAL_size_t(1, t[fmt].data_bytes);
    TEST_ASSERT_EQUAL_HEX8(0x55, ((const uint8_t *)t[fmt].data)[0]);   /* RGB565 */
}

static void test_selection(void)
{
    size_t n = 99;
    TEST_ASSERT_EQUAL_PTR(gz_co5300_init, gz_panel_init_table(GZ_PANEL_CO5300, &n));
    TEST_ASSERT_EQUAL_size_t(GZ_CO5300_INIT_LEN, n);
    TEST_ASSERT_EQUAL_PTR(gz_ili9341v_init, gz_panel_init_table(GZ_PANEL_ILI9341V, &n));
    TEST_ASSERT_EQUAL_size_t(GZ_ILI9341V_INIT_LEN, n);
    /* The ST77916 picks its table from the reg-04 revision probe instead. */
    TEST_ASSERT_NULL(gz_panel_init_table(GZ_PANEL_ST77916, &n));
    TEST_ASSERT_EQUAL_size_t(0, n);
    TEST_ASSERT_NULL(gz_panel_init_table(42, &n));
}

static void test_co5300_table(void)
{
    check_common(gz_co5300_init, GZ_CO5300_INIT_LEN);
    TEST_ASSERT_EQUAL_INT(GZ_DCS_SLPOUT, gz_co5300_init[0].cmd);      /* first, as in the vendor seq */
    int b = find(gz_co5300_init, GZ_CO5300_INIT_LEN, GZ_DCS_BRIGHTNESS);
    TEST_ASSERT_TRUE(b >= 0);
    TEST_ASSERT_EQUAL_HEX8(0x00, ((const uint8_t *)gz_co5300_init[b].data)[0]);   /* dark at boot */
    int c = find(gz_co5300_init, GZ_CO5300_INIT_LEN, GZ_DCS_CTRL_DISP);
    TEST_ASSERT_TRUE_MESSAGE(c >= 0 && c < b, "brightness control enabled before 0x51");
    TEST_ASSERT_EQUAL_HEX8(0x80, ((const uint8_t *)gz_co5300_init[2].data)[0]);   /* C4 80 */
}

static void test_ili9341v_table(void)
{
    check_common(gz_ili9341v_init, GZ_ILI9341V_INIT_LEN);
    TEST_ASSERT_EQUAL_size_t(22, GZ_ILI9341V_INIT_LEN);                 /* vendor file: 22 steps */
    int inv = find(gz_ili9341v_init, GZ_ILI9341V_INIT_LEN, GZ_DCS_INVON);
    TEST_ASSERT_TRUE_MESSAGE(inv >= 0, "IPS panel needs inversion on");
    int mad = find(gz_ili9341v_init, GZ_ILI9341V_INIT_LEN, GZ_DCS_MADCTL);
    TEST_ASSERT_TRUE(mad >= 0);
    TEST_ASSERT_EQUAL_HEX8(0x08, ((const uint8_t *)gz_ili9341v_init[mad].data)[0]);  /* portrait, BGR */
    /* Gamma tables are 15 bytes each. */
    TEST_ASSERT_EQUAL_size_t(15, gz_ili9341v_init[find(gz_ili9341v_init, GZ_ILI9341V_INIT_LEN, 0xE0)].data_bytes);
    TEST_ASSERT_EQUAL_size_t(15, gz_ili9341v_init[find(gz_ili9341v_init, GZ_ILI9341V_INIT_LEN, 0xE1)].data_bytes);
    /* Display-on is the last step. */
    TEST_ASSERT_EQUAL_INT(GZ_DCS_DISPON, gz_ili9341v_init[GZ_ILI9341V_INIT_LEN - 1].cmd);
}

static void test_brightness_byte(void)
{
    TEST_ASSERT_EQUAL_UINT8(0,   gz_panel_brightness_byte(0));
    TEST_ASSERT_EQUAL_UINT8(255, gz_panel_brightness_byte(100));
    TEST_ASSERT_EQUAL_UINT8(255, gz_panel_brightness_byte(250));       /* clamped */
    TEST_ASSERT_EQUAL_UINT8(179, gz_panel_brightness_byte(70));        /* 178.5 -> 179 */
    TEST_ASSERT_EQUAL_UINT8(128, gz_panel_brightness_byte(50));        /* 127.5 -> 128 */
    TEST_ASSERT_EQUAL_UINT8(3,   gz_panel_brightness_byte(1));
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_selection);
    RUN_TEST(test_co5300_table);
    RUN_TEST(test_ili9341v_table);
    RUN_TEST(test_brightness_byte);
    return UNITY_END();
}
