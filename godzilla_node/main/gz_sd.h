/*
 * gz_sd.h — the TF-card bus, chosen per board (three-board port, #88 §3.5).
 *
 *   GZ_BOARD_SD_SPI 0  SDMMC 4-bit via SD_MMC — 1.85B, ES3C28P
 *   GZ_BOARD_SD_SPI 1  SPI via SD on its own host — AMOLED-2.06
 *                      (MOSI 1, SCK 2, MISO 3, CS 17; the display's QSPI uses
 *                      SPI2, so the card takes HSPI = SPI3 on the ESP32-S3)
 *
 * Both are bundled with the Arduino core (no new library) and both are an
 * fs::FS, so gz_recon_task.cpp writes through GZ_SD_FS the same way on every
 * board. Never formats a card (format_if_empty / format_if_mount_failed false).
 *
 * ONE TU ONLY: holds the SPI bus object for the SPI boards; used solely by
 * gz_recon_task.cpp, the only code that touches the card.
 */
#pragma once

#include "gz_board.h"

#if GZ_BOARD_HAS_SD

#include <Arduino.h>
#include <FS.h>

#if GZ_BOARD_SD_SPI
#include <SPI.h>
#include <SD.h>

#define GZ_SD_FS        SD
#define GZ_SD_BUS_NAME  "SPI"

static SPIClass s_gz_sd_spi(HSPI);   /* SPI3: SPI2 belongs to the display */

/* Mount the card. Safe to call again after a failure. */
static inline bool gz_sd_begin(void)
{
    s_gz_sd_spi.begin(GZ_PIN_SDSPI_SCK, GZ_PIN_SDSPI_MISO, GZ_PIN_SDSPI_MOSI,
                      GZ_PIN_SDSPI_CS);
    return SD.begin(GZ_PIN_SDSPI_CS, s_gz_sd_spi, GZ_SD_SPI_HZ, "/sdcard", 5, false);
}

#else /* SDMMC 4-bit */
#include <SD_MMC.h>

#define GZ_SD_FS        SD_MMC
#define GZ_SD_BUS_NAME  "SDMMC 4-bit"

static inline bool gz_sd_begin(void)
{
    if (!SD_MMC.setPins(GZ_PIN_SD_CLK, GZ_PIN_SD_CMD, GZ_PIN_SD_D0,
                        GZ_PIN_SD_D1, GZ_PIN_SD_D2, GZ_PIN_SD_D3)) {
        Serial.println("!! SD: setPins rejected the gz_board.h pin map");
        return false;
    }
    return SD_MMC.begin("/sdcard", false, false);   /* 4-bit, never format */
}
#endif

static inline void gz_sd_end(void) { GZ_SD_FS.end(); }

#endif /* GZ_BOARD_HAS_SD */
