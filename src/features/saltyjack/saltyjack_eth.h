/*
 * saltyjack_eth — W5500 SPI Ethernet bring-up for SaltyJack.
 *
 * Wires the RJ45 path that saltyjack.h described as "future": an SPI Ethernet
 * netif on the W5500, so SaltyJack's STA-style LAN tools have a wired L2 to
 * attach to instead of Wi-Fi.
 *
 * Bus: the W5500 shares the Cardputer HSPI bus with the microSD
 * (docs/saltyjack-networking.md): SCK 40, MISO 39, MOSI 14. It is given its
 * own chip-select so the two devices coexist on one bus; we reuse the SD's
 * SPIClass (sd_get_spi()) rather than initialising HSPI twice.
 *   W5500 CS = 1, IRQ = 2, RST = open (module pull-up).
 * One-hat rule still applies: do not seat a W5500 together with Hydra/LoRa.
 *
 * Arduino-ESP32 3.x ETH driver (ETH_PHY_W5500). DHCP client by default.
 *
 * NOTE: compiles in CI; the shared-bus SD+W5500 coexistence and the DHCP lease
 * are hardware-bench items — this has not been verified on a physical module.
 */
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

/* Bring up the W5500 on the shared HSPI bus and start the DHCP client.
 * Idempotent; returns true once the driver has initialised (link/IP may
 * still be pending — poll the helpers below). */
bool      sj_eth_begin(void);
void      sj_eth_end(void);
bool      sj_eth_started(void);    /* driver initialised */
bool      sj_eth_link_up(void);    /* PHY reports the cable is up */
bool      sj_eth_has_ip(void);     /* DHCP installed a non-zero IPv4 */
IPAddress sj_eth_ip(void);
IPAddress sj_eth_gateway(void);
IPAddress sj_eth_subnet(void);
String    sj_eth_mac(void);
