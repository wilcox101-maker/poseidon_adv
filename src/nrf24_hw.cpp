/*
 * nrf24_hw.cpp — nRF24L01+ init/teardown for Hydra RF Cap 424.
 */
#include "nrf24_hw.h"
#include "cc1101_hw.h"
#include "sd_helper.h"
#include "gps.h"
#include <SPI.h>

static RF24 *s_radio = nullptr;
static bool  s_up    = false;

void nrf24_park_others(void)
{
    /* Official Hydra: CC1101 CS is GPIO 13, the same pad as the LoRa-GNSS
     * hat's GPS UART TX. Driving it while the UART owns it fights the GPS
     * driver and the CC1101 stays selected, so the nRF24 probe reads
     * garbage ("chip not detected"). GDO0 (GPIO 5) is not a chip-select
     * and is left alone — the CC1101 drives it, and it is LoRa NSS on the
     * other hat. */
    if (gps_is_up()) gps_end();

    /* Deselect every other slave on the shared HSPI bus (SCK/MISO/MOSI
     * from the SD helper, same nets the RF24 object uses). */
    pinMode(SD_CS,     OUTPUT); digitalWrite(SD_CS,     HIGH);
    pinMode(CC1101_CS, OUTPUT); digitalWrite(CC1101_CS, HIGH);
}

bool nrf24_begin(void)
{
    if (s_up) nrf24_end();
    nrf24_park_others();

    /* The SD card is optional, but its HSPI pins are shared with this hat. */
    if (!sd_prepare_bus()) {
        Serial.println("[nrf24] shared SPI bus unavailable");
        pinMode(NRF24_CS, OUTPUT);
        digitalWrite(NRF24_CS, HIGH);
        pinMode(CC1101_CS, OUTPUT);
        digitalWrite(CC1101_CS, HIGH);
        return false;
    }

    /* Use the SD's HSPI instance (official Hydra SCK/MISO/MOSI). Global
     * SPI is FSPI, which M5GFX claims for the TFT — calling SPI.begin()
     * there stole the GPIO matrix from the display every nRF24 op and the
     * screen would flicker / freeze. Mirror CC1101's pattern instead:
     * reuse the already-initialised sd_get_spi() instance. */
    SPIClass &bus = sd_get_spi();
    pinMode(NRF24_CS, OUTPUT); digitalWrite(NRF24_CS, HIGH);
    pinMode(NRF24_CE, OUTPUT); digitalWrite(NRF24_CE, LOW);

    s_radio = new RF24(NRF24_CE, NRF24_CS);
    if (!s_radio->begin(&bus)) {
        Serial.println("[nrf24] begin failed");
        delete s_radio; s_radio = nullptr;
        /* Destructor may have released the pad. Drive CS high again so
         * the nRF24 cannot answer. CE stays high-Z (LoRa DIO1). */
        pinMode(NRF24_CS, OUTPUT);
        digitalWrite(NRF24_CS, HIGH);
        pinMode(NRF24_CE, INPUT);
        return false;
    }

    if (!s_radio->isChipConnected()) {
        Serial.println("[nrf24] chip not detected");
        delete s_radio; s_radio = nullptr;
        pinMode(NRF24_CS, OUTPUT);
        digitalWrite(NRF24_CS, HIGH);
        pinMode(NRF24_CE, INPUT);
        return false;
    }

    s_radio->setPALevel(RF24_PA_MAX);
    s_radio->setDataRate(RF24_1MBPS);
    s_radio->stopListening();

    s_up = true;
    Serial.println("[nrf24] up");
    return true;
}

void nrf24_end(void)
{
    if (!s_up) return;
    if (s_radio) {
        s_radio->powerDown();
        delete s_radio;
        s_radio = nullptr;
    }
    s_up = false;

    /* CS stays driven high so the nRF24 cannot answer later SPI from
     * the microSD or the CC1101. CE is not a chip-select; leave it
     * high-Z because GPIO 4 is SX1262 DIO1 on the CAP-LoRa1262.
     * CC1101 CS (GPIO 13) stays high until radio_switch hands it back
     * to the GPS UART. */
    pinMode(NRF24_CS, OUTPUT);
    digitalWrite(NRF24_CS, HIGH);
    pinMode(NRF24_CE, INPUT);
}

bool  nrf24_is_up(void) { return s_up; }

RF24 &nrf24_radio(void)
{
    if (!s_radio) {
        /* Soft fallback: return a never-begin()'d dummy instead of
         * esp_restart(). Callers are expected to check nrf24_is_up()
         * first, but if a latent path misses the check we log and
         * return a harmless-but-nonfunctional radio so the device
         * stays responsive. Same pattern as lora_radio(). */
        static RF24 dummy(NRF24_CE, NRF24_CS);
        static uint32_t last_warn = 0;
        if (millis() - last_warn > 2000) {
            Serial.println("[nrf24] nrf24_radio() called without nrf24_begin() — returning dummy");
            last_warn = millis();
        }
        return dummy;
    }
    return *s_radio;
}
