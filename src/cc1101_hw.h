/*
 * cc1101_hw — CC1101 driver for the PINGEQUA Hydra RF Cap (Cardputer ADV).
 *
 * Official Hydra pinout (shared HSPI with the microSD):
 *   SCK=40  MISO=39  MOSI=14
 *   CC1101 CS=13  GDO0=5
 *   nRF24  CS=6   CE=4   (nrf24_hw.h)
 *
 * GPIO 13 is also the LoRa-GNSS hat's GPS UART TX, so CC1101 CS and the
 * GPS transmitter are mutually exclusive. GDO0 (GPIO 5) is not a GPS pin;
 * on the CAP-LoRa1262 that pad is SX1262 NSS.
 */
#pragma once

#include <Arduino.h>

#define CC1101_SCK   40
#define CC1101_MISO  39
#define CC1101_MOSI  14
#define CC1101_CS    13
#define CC1101_GDO0  5

bool cc1101_begin(float freq_mhz = 433.92f);
void cc1101_end(void);
bool cc1101_is_up(void);
void cc1101_set_freq(float mhz);
void cc1101_set_rx(void);
void cc1101_set_tx(void);
void cc1101_set_idle(void);
int  cc1101_get_rssi(void);

/* Park CS lines for other SPI peripherals HIGH before CC1101 SPI ops. */
void cc1101_park_others(void);
void cc1101_diag(void);   /* TEMP: chip-ID + live RSSI probe over serial */
