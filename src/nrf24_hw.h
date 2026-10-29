/*
 * nrf24_hw — nRF24L01+ driver for the PINGEQUA Hydra RF Cap (Cardputer ADV).
 *
 * Official Hydra pinout (shared HSPI with the microSD and the CC1101):
 *   SCK=40  MISO=39  MOSI=14
 *   nRF24  CS=6  CE=4
 *   CC1101 CS=13 GDO0=5   (cc1101_hw.h)
 *
 * CS overlaps CAP-LoRa1262 BUSY (GPIO 6). CE overlaps DIO1 (GPIO 4).
 * Parking the CC1101 drives GPIO 13, which is GPS UART TX on the
 * LoRa-GNSS hat — release that UART before taking the bus.
 */
#pragma once

#include <Arduino.h>
#include <RF24.h>

#define NRF24_SCK   40
#define NRF24_MISO  39
#define NRF24_MOSI  14
#define NRF24_CS    6
#define NRF24_CE    4

bool  nrf24_begin(void);
void  nrf24_end(void);
bool  nrf24_is_up(void);
RF24 &nrf24_radio(void);

