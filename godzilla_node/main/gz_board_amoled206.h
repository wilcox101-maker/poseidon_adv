/*
 * gz_board_amoled206.h — Waveshare ESP32-S3-Touch-AMOLED-2.06 pin map.
 * Second GODZILLA board. Included through gz_board.h when the build defines
 * GZ_BOARD_AMOLED206 ([env:godzilla-amoled206]); never directly.
 *
 * Sources:
 *   Waveshare wiki GPIO allocation —
 *     https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.06
 *     LCD   CO5300 QSPI 410x502 AMOLED: SIO0-3=4/5/6/7 SCL=11 CS=12 RST=8 TE=13
 *     Touch FT3168  I2C: SCL=14 SDA=15 INT=38 RST=9
 *     IMU   QMI8658 I2C (shared) INT=21 · RTC PCF85063 I2C (shared) INT=39
 *     PMIC  AXP2101 I2C (shared) · Codec ES8311 / mic ADC ES7210 I2C (shared)
 *     TF    SPI: MOSI=1 SCK=2 MISO=3 CS=17
 *     Keys  BOOT=GPIO0 (low = pressed) · PWR readable on GPIO10 (high = pressed)
 *   Addresses — Waveshare demo repo waveshareteam/ESP32-S3-Touch-AMOLED-2.06
 *     @ 3de8684: FT3168 0x38 (Arduino_FT3x68.h), QMI8658 0x6B (SensorLib);
 *     PCF85063 0x51 is the part's fixed address. AXP2101 0x34, ES8311 0x18 and
 *     ES7210 0x40 are the parts' usual defaults — INFERRED, to be confirmed by
 *     the boot I2C scan on Dale's bench (the wiki lists no addresses).
 *
 * Unit 1 of the port (design doc §6) is the pin map and a HEADLESS build: the
 * radios, link, publish path and RTC run, but the display, touch and TF slot
 * are not driven yet (GZ_BOARD_HAS_UI / _HAS_SD = 0; the UI, display and SD
 * sources are left out of this env's build_src_filter). Units 2-5 turn them on.
 *
 * Pure: macros + compile-time collision guards, compiled on the host by
 * test/test_godzilla_board_amoled206.
 */
#pragma once

/* ---- Board identity + capabilities (see gz_board.h) ---- */
#define GZ_BOARD_NAME        "ESP32-S3-Touch-AMOLED-2.06"
#define GZ_BOARD_HAS_UI      1    /* CO5300 + FT3168 driven (full UI)          */
#define GZ_BOARD_HAS_SD      1    /* SPI TF slot + Recon writer driven        */
#define GZ_BOARD_SD_SPI      1    /* SD over SPI on HSPI/SPI3 (gz_sd.h)       */
#define GZ_SD_SPI_HZ  20000000u   /* conservative; raise after a bench run   */
#define GZ_BOARD_HAS_RTC     1    /* PCF85063                              */
#define GZ_BOARD_HAS_IMU     1    /* QMI8658                               */
#define GZ_BOARD_HAS_GAUGE   1    /* AXP2101 PMIC (read-only)               */
#define GZ_BOARD_HAS_MIC_ADC 1    /* ES7210                                */
#define GZ_LCD_ROUND         0    /* rectangular 410x502, rounded corners     */
#define GZ_BOARD_DISPLAY     2    /* GZ_PANEL_CO5300 (gz_display_co5300.cpp)  */
#define GZ_I2C_NAME_TOUCH    "FT3168 touch"
#define GZ_BOARD_TOUCH_FT    1    /* FocalTech driver (gz_touch_ft.h)         */
#define GZ_TOUCH_FT_ID_REG   0xA0 /* FT3x68 device ID: 0x03 = FT3168 (Waveshare driver) */
#define GZ_TOUCH_FT_BOOT_MS  200  /* after reset; bench-tunable               */
#define GZ_I2C_NAME_GAUGE    "AXP2101 PMIC (unused)"

/* ---- LCD: CO5300 AMOLED over QSPI (no backlight: brightness is DCS 0x51) ---- */
#define GZ_PIN_LCD_CS     12
#define GZ_PIN_LCD_PCLK   11
#define GZ_PIN_LCD_DATA0   4
#define GZ_PIN_LCD_DATA1   5
#define GZ_PIN_LCD_DATA2   6
#define GZ_PIN_LCD_DATA3   7
#define GZ_PIN_LCD_RST     8
#define GZ_PIN_LCD_TE     13   /* tearing-effect sync, input */

#define GZ_LCD_WIDTH     410
#define GZ_LCD_HEIGHT    502
#define GZ_LCD_COL_GAP    22   /* CO5300 RAM column offset (Waveshare demo) */

/* ---- Shared I2C bus (touch, RTC, IMU, PMIC, codecs) ---- */
#define GZ_PIN_I2C_SCL    14
#define GZ_PIN_I2C_SDA    15

/* ---- Touch: FT3168 ---- */
#define GZ_I2C_ADDR_TOUCH 0x38
#define GZ_PIN_TOUCH_RST   9
#define GZ_PIN_TOUCH_INT  38

/* ---- RTC: PCF85063 (same part as the 1.85B; gz_rtc.h is reused) ---- */
#define GZ_I2C_ADDR_RTC   0x51
#define GZ_PIN_RTC_INT    39

/* ---- IMU: QMI8658 (same part as the 1.85B) ---- */
#define GZ_I2C_ADDR_IMU   0x6B
#define GZ_PIN_IMU_INT    21

/* ---- Power: AXP2101 PMIC. READ-ONLY by design — never write a rail register
 * (design doc §3). Named GAUGE so the scan / status code shares one slot with
 * the 1.85B's BQ27220. ---- */
#define GZ_I2C_ADDR_GAUGE   0x34   /* inferred, confirm on the bench */

/* ---- Audio (not driven; out of scope as on the 1.85B) ---- */
#define GZ_I2C_ADDR_CODEC   0x18   /* ES8311 (inferred) */
#define GZ_I2C_ADDR_MIC_ADC 0x40   /* ES7210 (inferred) */
#define GZ_PIN_AUDIO_MCLK   16
#define GZ_PIN_AMP_EN       46     /* NS4150B speaker amp enable */

/* ---- microSD (TF): SPI, on its own host (not the display's QSPI) ---- */
#define GZ_PIN_SDSPI_MOSI  1
#define GZ_PIN_SDSPI_SCK   2
#define GZ_PIN_SDSPI_MISO  3
#define GZ_PIN_SDSPI_CS   17

/* ---- Keys ---- */
#define GZ_PIN_KEY_BOOT    0    /* low = pressed (strapping pin: input only) */
#define GZ_PIN_KEY_PWR    10    /* high = pressed (PWR key level, via the PMIC) */

/* Compile-time guards: everything on the I2C bus shares SCL/SDA on purpose,
 * but no two pins below may collide and no two I2C parts may share an address.
 * A fat-fingered edit fails the build here. */
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#include <assert.h>
/* Every driven or reserved GPIO, once. */
#define GZ_206_PINS GZ_PIN_LCD_CS, GZ_PIN_LCD_PCLK, GZ_PIN_LCD_DATA0, GZ_PIN_LCD_DATA1, \
                    GZ_PIN_LCD_DATA2, GZ_PIN_LCD_DATA3, GZ_PIN_LCD_RST, GZ_PIN_LCD_TE,  \
                    GZ_PIN_I2C_SCL, GZ_PIN_I2C_SDA, GZ_PIN_TOUCH_RST, GZ_PIN_TOUCH_INT, \
                    GZ_PIN_RTC_INT, GZ_PIN_IMU_INT, GZ_PIN_AUDIO_MCLK, GZ_PIN_AMP_EN,   \
                    GZ_PIN_SDSPI_MOSI, GZ_PIN_SDSPI_SCK, GZ_PIN_SDSPI_MISO,            \
                    GZ_PIN_SDSPI_CS, GZ_PIN_KEY_BOOT, GZ_PIN_KEY_PWR
#ifdef __cplusplus
constexpr bool gz_206_pins_unique(const int *p, int n)
{
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (p[i] == p[j]) return false;
    return true;
}
constexpr int gz_206_pin_list[] = { GZ_206_PINS };
static_assert(gz_206_pins_unique(gz_206_pin_list,
                                 (int)(sizeof gz_206_pin_list / sizeof gz_206_pin_list[0])),
              "AMOLED-2.06 pins collide");
#endif
/* I2C addresses (C and C++). */
static_assert(GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_RTC   && GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_IMU &&
              GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_GAUGE && GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_CODEC &&
              GZ_I2C_ADDR_TOUCH != GZ_I2C_ADDR_MIC_ADC &&
              GZ_I2C_ADDR_RTC   != GZ_I2C_ADDR_IMU   && GZ_I2C_ADDR_RTC   != GZ_I2C_ADDR_GAUGE &&
              GZ_I2C_ADDR_RTC   != GZ_I2C_ADDR_CODEC && GZ_I2C_ADDR_RTC   != GZ_I2C_ADDR_MIC_ADC &&
              GZ_I2C_ADDR_IMU   != GZ_I2C_ADDR_GAUGE && GZ_I2C_ADDR_IMU   != GZ_I2C_ADDR_CODEC &&
              GZ_I2C_ADDR_IMU   != GZ_I2C_ADDR_MIC_ADC &&
              GZ_I2C_ADDR_GAUGE != GZ_I2C_ADDR_CODEC && GZ_I2C_ADDR_GAUGE != GZ_I2C_ADDR_MIC_ADC &&
              GZ_I2C_ADDR_CODEC != GZ_I2C_ADDR_MIC_ADC, "AMOLED-2.06 I2C addresses collide");
#endif
