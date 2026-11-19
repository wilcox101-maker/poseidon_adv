/*
 * gz_display.h — the display API for the GODZILLA node, one back end per board
 * (GZ_BOARD_DISPLAY): gz_display.cpp (1.85B ST77916, below), and for the port
 * gz_display_co5300.cpp (AMOLED-2.06) and gz_display_ili9341.cpp (ES3C28P).
 * gz_ui.cpp calls only this API, so it is the same on every board. On the
 * AMOLED "backlight" is the panel's DCS brightness (0x51); the panel starts
 * dark either way, so boot never flashes.
 *
 * 1.85B back end:
 * Brings up the 1.85B's 360x360 ST77916 over QSPI on SPI2_HOST via the ESP-IDF
 * esp_lcd core (bundled in the Arduino core) and Espressif's esp_lcd_st77916
 * driver, vendored from Waveshare's 1.85B demo. Before the panel init it reads
 * register 0x04 at a slow clock to tell the two panel revisions apart and pick
 * Waveshare's matching vendor init table (gz_st77916_rev.h).
 *
 * The backlight (GPIO5, LEDC PWM) stays OFF until the UI has pushed its first
 * frame, so boot never flashes white.
 *
 * Pins: gz_board.h (cited to the Waveshare 1.85B wiki).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* QSPI panel clock. Waveshare's demo runs 80 MHz; start conservative per the
 * P2a advice and raise only after a clean bench run. */
#define GZ_LCD_QSPI_HZ        (40u * 1000u * 1000u)

/* 4-wire SPI panel clock (ES3C28P ILI9341V). Conservative start, raised only
 * after a clean bench run. */
#define GZ_LCD_SPI_HZ         (40u * 1000u * 1000u)
/* Slow clock for the register-0x04 revision probe (Waveshare uses 5 MHz). */
#define GZ_LCD_PROBE_HZ       (5u * 1000u * 1000u)
/* Default backlight once the first frame is up (0..100 %). */
#define GZ_LCD_BL_DEFAULT_PCT 70u
/* Rows per LVGL partial draw buffer. Also sizes the largest single QSPI
 * transfer, so the bus and the UI can never disagree. 36 rows = 1/10 screen,
 * the same split Waveshare's demo uses. */
#define GZ_LCD_BUF_LINES      36u

/* Called from the SPI ISR when a draw_bitmap transfer has fully left the bus.
 * Must be ISR-safe; return true if it woke a higher-priority task. */
typedef bool (*gz_display_done_cb_t)(void *ctx);

/* Bring up bus, probe revision, init panel, display on (backlight still off).
 * Returns false (and logs why) if any esp_lcd step fails. */
bool gz_display_init(gz_display_done_cb_t on_done, void *ctx);

/* The panel revision the probe found: GZ_ST77916_REV_1/_2/_UNKNOWN on the
 * 1.85B; GZ_ST77916_REV_UNKNOWN (no probe) on the other boards. */
int gz_display_panel_rev(void);

/* Queue a rectangle of RGB565 pixels (already byte-swapped for the panel).
 * x2/y2 are EXCLUSIVE. The buffer must stay valid until on_done fires. */
void gz_display_draw(int x1, int y1, int x2_excl, int y2_excl, const void *px);

/* Backlight 0..100 %. Call from task context (not the ISR). */
void gz_display_backlight(uint8_t pct);
