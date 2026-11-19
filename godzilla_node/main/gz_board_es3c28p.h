/*
 * gz_board_es3c28p.h — 2.8" ESP32-S3 Display, ES3C28P (capacitive touch) pin map.
 * Third GODZILLA board. Included through gz_board.h when the build defines
 * GZ_BOARD_ES3C28P ([env:godzilla-es3c28p]); never directly.
 *
 * Source: lcdwiki, https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 *   SoC   ESP32-S3 N16R8 (16 MB flash, 8 MB OPI PSRAM)
 *   LCD   ILI9341V 240x320 IPS, 4-wire SPI: CS=10 DC=46 SCK=12 MOSI=11 MISO=13,
 *         RST shared with the S3's EN (no GPIO), backlight IO45 (high = on)
 *   Touch FT6336G I2C: SDA=16 SCL=15 RST=18 (low = reset) INT=17 (low on touch)
 *   TF    SDIO 4-bit: CLK=38 CMD=40 D0=39 D1=41 D2=48 D3=47
 *   Audio ES8311 + FM8002E amp: EN=IO1 (low = enabled), I2S MCLK=4 BCLK=5
 *         DIN=6 LRCK=7 DOUT=8; MEMS mic
 *   Power TP4054 charger, battery ADC on IO9 · RGB LED (WS2812-type) IO42
 *   Keys  BOOT=IO0 · expansion header IO2/3/14/21 · I2C header shares SDA16/SCL15
 *   RTC / IMU / PMIC: none listed.
 * The FT6336G address (0x38) is FocalTech's default and the ES8311 address
 * (0x18) the part's usual default — INFERRED, to be confirmed by the boot I2C
 * scan (the page lists no addresses). ES3N28P is the no-touch sibling; this map
 * targets the ES3C28P (design: docs/proposals/2026-11-19-godzilla-es3c28p-port.md).
 *
 * Port unit (addendum §5 step 2): pin map + build with the TF slot and Recon
 * writer ON (same SDMMC driver as the 1.85B, other pins) and the UI headless
 * until the ILI9341V / FT6336G units. The speaker amp is held OFF at boot.
 *
 * Note: IO45 (backlight) and IO46 (LCD DC) are ESP32-S3 strapping pins; the
 * vendor's design drives them to safe boot states, so we follow its
 * assignment and never repurpose them.
 *
 * Pure: macros + compile-time collision guards, compiled on the host by
 * test/test_godzilla_board_es3c28p.
 */
#pragma once

/* ---- Board identity + capabilities (see gz_board.h) ---- */
#define GZ_BOARD_NAME        "2.8in ESP32-S3 Display ES3C28P"
#define GZ_BOARD_HAS_UI      0    /* ILI9341V + FT6336G not driven yet       */
#define GZ_BOARD_HAS_SD      1    /* SDMMC 4-bit TF + Recon writer driven    */
#define GZ_BOARD_HAS_RTC     0    /* no RTC: software clock, "hold to set"   */
#define GZ_BOARD_HAS_IMU     0    /* no IMU                                  */
#define GZ_BOARD_HAS_GAUGE   0    /* no I2C power part; battery is ADC IO9   */
#define GZ_BOARD_HAS_MIC_ADC 0    /* MEMS mic, no I2C mic ADC                */
#define GZ_LCD_ROUND         0    /* rectangular 240x320 portrait            */
#define GZ_I2C_NAME_TOUCH    "FT6336G touch"
#define GZ_BOARD_TOUCH_FT    1    /* FocalTech driver (gz_touch_ft.h)         */
#define GZ_TOUCH_FT_ID_REG   0xA3 /* FT6x36 chip-ID register (inferred; logged only) */
#define GZ_TOUCH_FT_BOOT_MS  200  /* after reset; bench-tunable               */

/* ---- LCD: ILI9341V over 4-wire SPI ---- */
#define GZ_PIN_LCD_CS     10
#define GZ_PIN_LCD_DC     46
#define GZ_PIN_LCD_SCK    12
#define GZ_PIN_LCD_MOSI   11
#define GZ_PIN_LCD_MISO   13
#define GZ_PIN_LCD_BL     45   /* high = on */
/* No GZ_PIN_LCD_RST: the panel reset is on EN, so the driver soft-resets. */

#define GZ_LCD_WIDTH     240
#define GZ_LCD_HEIGHT    320

/* ---- I2C bus (touch only; also the 4-pin I2C header) ---- */
#define GZ_PIN_I2C_SCL    15
#define GZ_PIN_I2C_SDA    16

/* ---- Touch: FT6336G ---- */
#define GZ_I2C_ADDR_TOUCH 0x38   /* inferred, confirm on the bench */
#define GZ_PIN_TOUCH_RST  18
#define GZ_PIN_TOUCH_INT  17

/* ---- Audio (not driven; amp held off) ---- */
#define GZ_I2C_ADDR_CODEC 0x18   /* ES8311 (inferred) */
#define GZ_PIN_AMP_EN      1
#define GZ_AMP_OFF_LEVEL   1     /* FM8002E: low = enabled, so drive high */
#define GZ_PIN_I2S_MCLK    4
#define GZ_PIN_I2S_BCLK    5
#define GZ_PIN_I2S_DIN     6
#define GZ_PIN_I2S_LRCK    7
#define GZ_PIN_I2S_DOUT    8

/* ---- microSD (TF): SDMMC 4-bit (Recon log) ---- */
#define GZ_PIN_SD_CLK     38
#define GZ_PIN_SD_CMD     40
#define GZ_PIN_SD_D0      39
#define GZ_PIN_SD_D1      41
#define GZ_PIN_SD_D2      48
#define GZ_PIN_SD_D3      47

/* ---- Power / extras ---- */
#define GZ_PIN_BATT_ADC    9
#define GZ_PIN_RGB_LED    42
#define GZ_PIN_KEY_BOOT    0

/* Compile-time guards: no two pins below may collide, and the two I2C parts
 * must differ. A fat-fingered edit fails the build here. */
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#include <assert.h>
#define GZ_ES3_PINS GZ_PIN_LCD_CS, GZ_PIN_LCD_DC, GZ_PIN_LCD_SCK, GZ_PIN_LCD_MOSI,   \
                    GZ_PIN_LCD_MISO, GZ_PIN_LCD_BL, GZ_PIN_I2C_SCL, GZ_PIN_I2C_SDA,    \
                    GZ_PIN_TOUCH_RST, GZ_PIN_TOUCH_INT, GZ_PIN_AMP_EN,                 \
                    GZ_PIN_I2S_MCLK, GZ_PIN_I2S_BCLK, GZ_PIN_I2S_DIN, GZ_PIN_I2S_LRCK, \
                    GZ_PIN_I2S_DOUT, GZ_PIN_SD_CLK, GZ_PIN_SD_CMD, GZ_PIN_SD_D0,       \
                    GZ_PIN_SD_D1, GZ_PIN_SD_D2, GZ_PIN_SD_D3, GZ_PIN_BATT_ADC,         \
                    GZ_PIN_RGB_LED, GZ_PIN_KEY_BOOT
#ifdef __cplusplus
constexpr bool gz_es3_pins_unique(const int *p, int n)
{
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (p[i] == p[j]) return false;
    return true;
}
constexpr int gz_es3_pin_list[] = { GZ_ES3_PINS };
static_assert(gz_es3_pins_unique(gz_es3_pin_list,
                                 (int)(sizeof gz_es3_pin_list / sizeof gz_es3_pin_list[0])),
              "ES3C28P pins collide");
#endif
static_assert(GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_CODEC, "ES3C28P I2C addresses collide");
#endif
