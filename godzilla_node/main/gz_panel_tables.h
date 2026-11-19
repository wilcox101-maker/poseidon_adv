/*
 * gz_panel_tables.h — vendor init tables for the two ported panels (three-board
 * port, completion orders #88 §3.4), plus the pure helpers that choose and use
 * them. The 1.85B's ST77916 tables stay in gz_st77916_tables.* (unchanged).
 *
 * Both panels are driven by the vendored esp_lcd_st77916 driver, which sends
 * MADCTL + COLMOD from its stored values, then this table, and adopts any
 * 0x36 / 0x3A found in the table (standard MIPI-DCS on both controllers, so
 * nothing conflicts). The driver never sends SLPOUT itself: each table does.
 *
 * CO5300 (AMOLED-2.06, QSPI) — sequence from Arduino_GFX's
 * co5300_init_operations, as used by Waveshare's AMOLED-2.06 demo (facts cited;
 * no code taken). One deliberate change: brightness 0x51 starts at 0x00, not
 * 0xD0, so the panel stays dark until the first LVGL frame is up (the same "no
 * flash at boot" rule as the 1.85B's backlight). gz_display_backlight() raises
 * it afterwards.
 *
 * ILI9341V (ES3C28P, 4-wire SPI) — lcdwiki's vendor init (ILI9341V_Init.txt for
 * the ES3C28P): power / timing / gamma, inversion ON, MADCTL 0x08 (portrait,
 * BGR), COLMOD 0x55 (RGB565), then SLPOUT + 120 ms and DISPON.
 *
 * Pure: the entry type is layout-identical to the driver's
 * st77916_lcd_init_cmd_t (checked by static_assert in the on-target back ends),
 * so the host test (test/test_godzilla_panel) checks the tables themselves.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/* Same layout as st77916_lcd_init_cmd_t (esp_lcd_st77916.h). */
typedef struct {
    int          cmd;
    const void  *data;
    size_t       data_bytes;
    unsigned int delay_ms;
} gz_lcd_init_cmd_t;

/* Display controller ids (GZ_BOARD_DISPLAY in each board header). */
#define GZ_PANEL_ST77916   1   /* 1.85B: round 360x360 QSPI LCD + LEDC backlight */
#define GZ_PANEL_CO5300    2   /* AMOLED-2.06: 410x502 QSPI AMOLED, DCS brightness */
#define GZ_PANEL_ILI9341V  3   /* ES3C28P: 240x320 SPI LCD + GPIO backlight */

/* MIPI-DCS commands used below. */
#define GZ_DCS_SLPOUT      0x11
#define GZ_DCS_INVON       0x21
#define GZ_DCS_DISPON      0x29
#define GZ_DCS_MADCTL      0x36
#define GZ_DCS_COLMOD      0x3A
#define GZ_DCS_BRIGHTNESS  0x51   /* CO5300: write display brightness (normal) */
#define GZ_DCS_CTRL_DISP   0x53

/* ---- CO5300 (AMOLED-2.06) ------------------------------------------------- */
static const uint8_t gz_co5300_fe[]   = { 0x00 };
static const uint8_t gz_co5300_c4[]   = { 0x80 };   /* SPI mode control */
static const uint8_t gz_co5300_3a[]   = { 0x55 };   /* RGB565 */
static const uint8_t gz_co5300_53[]   = { 0x20 };   /* brightness control on */
static const uint8_t gz_co5300_63[]   = { 0xFF };   /* HBM brightness value */
static const uint8_t gz_co5300_51[]   = { 0x00 };   /* dark until first frame */
static const uint8_t gz_co5300_58[]   = { 0x00 };   /* sunlight enhancement off */

static const gz_lcd_init_cmd_t gz_co5300_init[] = {
    { GZ_DCS_SLPOUT,     NULL,         0, 120 },
    { 0xFE,              gz_co5300_fe, 1, 0 },
    { 0xC4,              gz_co5300_c4, 1, 0 },
    { GZ_DCS_COLMOD,     gz_co5300_3a, 1, 0 },
    { GZ_DCS_CTRL_DISP,  gz_co5300_53, 1, 0 },
    { 0x63,              gz_co5300_63, 1, 0 },
    { GZ_DCS_DISPON,     NULL,         0, 0 },
    { GZ_DCS_BRIGHTNESS, gz_co5300_51, 1, 0 },
    { 0x58,              gz_co5300_58, 1, 10 },
};
#define GZ_CO5300_INIT_LEN (sizeof gz_co5300_init / sizeof gz_co5300_init[0])

/* ---- ILI9341V (ES3C28P) --------------------------------------------------- */
static const uint8_t gz_ili_cf[] = { 0x00, 0xC1, 0x30 };
static const uint8_t gz_ili_ed[] = { 0x64, 0x03, 0x12, 0x81 };
static const uint8_t gz_ili_e8[] = { 0x85, 0x00, 0x78 };
static const uint8_t gz_ili_cb[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
static const uint8_t gz_ili_f7[] = { 0x20 };
static const uint8_t gz_ili_ea[] = { 0x00, 0x00 };
static const uint8_t gz_ili_c0[] = { 0x13 };
static const uint8_t gz_ili_c1[] = { 0x13 };
static const uint8_t gz_ili_c5[] = { 0x22, 0x35 };
static const uint8_t gz_ili_c7[] = { 0xBD };
static const uint8_t gz_ili_36[] = { 0x08 };          /* portrait 240x320, BGR */
static const uint8_t gz_ili_b6[] = { 0x0A, 0xA2 };
static const uint8_t gz_ili_3a[] = { 0x55 };          /* RGB565 */
static const uint8_t gz_ili_f6[] = { 0x01, 0x30 };
static const uint8_t gz_ili_b1[] = { 0x00, 0x1B };
static const uint8_t gz_ili_f2[] = { 0x00 };
static const uint8_t gz_ili_26[] = { 0x01 };
static const uint8_t gz_ili_e0[] = { 0x0F, 0x35, 0x31, 0x0B, 0x0E, 0x06, 0x49, 0xA7,
                                     0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00 };
static const uint8_t gz_ili_e1[] = { 0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58,
                                     0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F };

static const gz_lcd_init_cmd_t gz_ili9341v_init[] = {
    { 0xCF,          gz_ili_cf, sizeof gz_ili_cf, 0 },
    { 0xED,          gz_ili_ed, sizeof gz_ili_ed, 0 },
    { 0xE8,          gz_ili_e8, sizeof gz_ili_e8, 0 },
    { 0xCB,          gz_ili_cb, sizeof gz_ili_cb, 0 },
    { 0xF7,          gz_ili_f7, sizeof gz_ili_f7, 0 },
    { 0xEA,          gz_ili_ea, sizeof gz_ili_ea, 0 },
    { 0xC0,          gz_ili_c0, sizeof gz_ili_c0, 0 },
    { 0xC1,          gz_ili_c1, sizeof gz_ili_c1, 0 },
    { 0xC5,          gz_ili_c5, sizeof gz_ili_c5, 0 },
    { 0xC7,          gz_ili_c7, sizeof gz_ili_c7, 0 },
    { GZ_DCS_INVON,  NULL,      0,                0 },
    { GZ_DCS_MADCTL, gz_ili_36, sizeof gz_ili_36, 0 },
    { 0xB6,          gz_ili_b6, sizeof gz_ili_b6, 0 },
    { GZ_DCS_COLMOD, gz_ili_3a, sizeof gz_ili_3a, 0 },
    { 0xF6,          gz_ili_f6, sizeof gz_ili_f6, 0 },
    { 0xB1,          gz_ili_b1, sizeof gz_ili_b1, 0 },
    { 0xF2,          gz_ili_f2, sizeof gz_ili_f2, 0 },
    { 0x26,          gz_ili_26, sizeof gz_ili_26, 0 },
    { 0xE0,          gz_ili_e0, sizeof gz_ili_e0, 0 },
    { 0xE1,          gz_ili_e1, sizeof gz_ili_e1, 0 },
    { GZ_DCS_SLPOUT, NULL,      0,                120 },
    { GZ_DCS_DISPON, NULL,      0,                0 },
};
#define GZ_ILI9341V_INIT_LEN (sizeof gz_ili9341v_init / sizeof gz_ili9341v_init[0])

/* ---- selection + helpers --------------------------------------------------- */

/* The vendor table a panel id uses, or NULL for the ST77916 (its revision
 * probe picks one of gz_st77916_tables.*) and for unknown ids. */
static inline const gz_lcd_init_cmd_t *gz_panel_init_table(int panel, size_t *len)
{
    switch (panel) {
    case GZ_PANEL_CO5300:   *len = GZ_CO5300_INIT_LEN;   return gz_co5300_init;
    case GZ_PANEL_ILI9341V: *len = GZ_ILI9341V_INIT_LEN; return gz_ili9341v_init;
    default:                *len = 0;                    return NULL;
    }
}

/* 0..100 % -> the CO5300's 0x51 brightness byte (0..255, rounded). */
static inline uint8_t gz_panel_brightness_byte(uint8_t pct)
{
    if (pct > 100) pct = 100;
    return (uint8_t)((pct * 255u + 50u) / 100u);
}
