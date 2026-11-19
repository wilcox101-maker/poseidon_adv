/*
 * gz_i2c_scan.h — boot-time I2C bus probe for the GODZILLA node (P2a).
 *
 * A boot-time scan of the shared I2C bus prints every address that ACKs, so
 * Dale's first flash confirms the pin map on real hardware: we expect the
 * CST816S touch (0x15) and the PCF85063 RTC (0x51) to answer. SCL 10 / SDA 11
 * and TOUCH_RST 1 match Waveshare's 1.85B page; the scan is the hardware
 * proof. If the bus is wired to other pins, nothing ACKs — and that silence
 * is itself the signal. The 1.85B also carries an IMU, a battery gauge and two
 * audio parts on this bus; they are labelled but not driven (P3). The touch
 * controller sleeps and may not ACK a scan: a missing 0x15 alone is not proof
 * of a wrong pin (gz_touch.h resets it and reports separately).
 *
 * This is deliberately ahead of the ST77916/LVGL driver: committing a display
 * init table to pins we have not yet seen ACK would be guessing. The scan
 * confirms the bus first.
 *
 * Passive w.r.t. the project's RF rules: it drives only the on-board I2C bus to
 * query soldered-on peripherals; it transmits no radio frames.
 *
 * Split: the address -> expected-device mapping is pure (no Arduino/Wire) and
 * reads its addresses from gz_board.h, so test/test_godzilla_i2c pins it on the
 * host. The actual Wire probing is ARDUINO-only, behind #ifdef.
 */
#pragma once

#include <stdint.h>

#include "gz_board.h"   /* GZ_I2C_ADDR_TOUCH / GZ_I2C_ADDR_RTC / GZ_PIN_I2C_* */

/* Pure: annotate an I2C address with the peripheral the 1.85B map expects
 * there, for the boot log and for the host test. Single-sourced on gz_board.h
 * so a pin/address edit cannot drift the two apart. */
static inline const char *gz_i2c_expected_device(uint8_t addr)
{
    if (addr == GZ_I2C_ADDR_TOUCH) return "CST816S touch";
    if (addr == GZ_I2C_ADDR_RTC)   return "PCF85063 RTC";
    if (addr == GZ_I2C_ADDR_IMU)     return "QMI8658 IMU (unused)";
    if (addr == GZ_I2C_ADDR_GAUGE)   return "BQ27220 battery gauge (unused)";
    if (addr == GZ_I2C_ADDR_CODEC)   return "ES8311 codec? (unused)";
    if (addr == GZ_I2C_ADDR_MIC_ADC) return "ES7210 mic ADC? (unused)";
    return "unknown";
}

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>

/* Probe the shared touch+RTC I2C bus and print every 7-bit address that ACKs,
 * each annotated with the peripheral we expect there. Returns the count found.
 * Call once at boot, after Serial is up. Standard 0x08..0x77 7-bit range. */
static inline uint8_t gz_i2c_scan_report()
{
    Wire.begin((int)GZ_PIN_I2C_SDA, (int)GZ_PIN_I2C_SCL);
    Serial.printf("  I2C scan (SDA=%u SCL=%u):\n",
                  (unsigned)GZ_PIN_I2C_SDA, (unsigned)GZ_PIN_I2C_SCL);
    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("    0x%02X  %s\n", addr, gz_i2c_expected_device(addr));
            ++found;
        }
    }
    if (found == 0) {
        Serial.println("    (none) — verify SDA/SCL against the 1.85B wiki pin map");
    } else {
        Serial.printf("    %u device(s); expect 0x%02X touch + 0x%02X RTC\n",
                      (unsigned)found,
                      (unsigned)GZ_I2C_ADDR_TOUCH, (unsigned)GZ_I2C_ADDR_RTC);
    }
    return found;
}
#endif /* ARDUINO */
