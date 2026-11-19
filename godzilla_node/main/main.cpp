/*
 * main.cpp — GODZILLA node entry.
 *
 * Task 3a: ESP-NOW link + HELLO every 5 s so the Cardputer (S3) registers the
 * node (C5 -> Diagnostics). Task 3b: continuous passive BLE scan. Task 3c:
 * passive Wi-Fi promiscuous scan hopping 1..13. Both feed gz_batch.h and
 * publish over ESP-NOW on a ~1 s tick.
 *
 * Passive only — nothing transmits except ESP-NOW HELLO/publish.
 */
#include <Arduino.h>

#include "gz_link.h"
#include "gz_hello.h"      /* GZ_HELLO_NAME / GZ_ROLE for the boot log */
#include "gz_ble_scan.h"   /* task 3b: passive BLE scan → batch → publish */
#include "gz_wifi_scan.h"  /* task 3c: passive Wi-Fi scan → batch → publish */
#include "gz_i2c_scan.h"   /* P2a: boot-time I2C bus probe (touch + RTC) */
#include "gz_stats.h"      /* P2a: counters behind the status screen */
#include "gz_ui.h"         /* P2a: ST77916 display + LVGL status screen */
#include "gz_rtc.h"        /* P2a: PCF85063 RTC read / build-time seed */

static constexpr uint32_t GZ_HELLO_PERIOD_MS = 5000;

/* This node's id on the fabric; carried in detection batches. The S3 ties it
 * to this node's MAC via the HELLO it hears. */
static constexpr uint8_t GZ_NODE_ID = 2;

void setup()
{
    Serial.begin(115200);
    delay(200);

    /* P2a board bring-up, before the radios: confirm PSRAM is mapped (the UI
     * framebuffer will live there) and probe the I2C bus so Dale's first flash
     * confirms the touch/RTC pins on real hardware. Fail loudly on no PSRAM. */
    size_t psram = ESP.getPsramSize();
    if (psram == 0) {
        Serial.println("!! PSRAM NOT DETECTED (ESP.getPsramSize()==0) — check "
                       "board_build.arduino.memory_type=qio_opi + -DBOARD_HAS_PSRAM");
    } else {
        Serial.printf("  PSRAM: %u KB\n", (unsigned)(psram / 1024));
    }
    uint8_t i2c_found = gz_i2c_scan_report();

    /* RTC bring-up on the same bus the scan just opened: read the PCF85063 and,
     * if its oscillator stopped (cold boot / no backup cell), seed it from the
     * firmware build time so the clock and the later pocket-watch face start
     * sane. Passive — I2C to a soldered peripheral, no radio. */
    gz_datetime_t gz_now;
    bool rtc_seeded = false;
    bool rtc_ok = gz_rtc_begin(&gz_now, &rtc_seeded);
    gz_stats_set_rtc(rtc_ok, rtc_seeded);   /* watch face: "hold to set" hint */

    /* P2a: status screen. Identity first, then the panel + LVGL task. The
     * radios below start either way — headless is still a working node. */
    gz_stats_set_static(GZ_HELLO_NAME, GZ_LINK_CHANNEL,
                        (uint32_t)(psram / 1024), i2c_found);
    gz_ui_start();

    gz_link_init();
    gz_link_send_hello();        /* announce immediately, like c5_node */
    gz_ble_scan_init(GZ_NODE_ID);
    gz_wifi_scan_init(GZ_NODE_ID);

    Serial.println();
    Serial.println("POSEIDON GODZILLA node — P2a: status screen + passive BLE/Wi-Fi publish");
    Serial.printf("  link: WiFi STA ch%u, ESP-NOW, broadcast peer\n",
                  (unsigned)GZ_LINK_CHANNEL);
    Serial.printf("  HELLO '%s' role=%u every %lu ms\n",
                  GZ_HELLO_NAME, (unsigned)GZ_ROLE,
                  (unsigned long)GZ_HELLO_PERIOD_MS);
    Serial.printf("  BLE: passive scan, publish every %u ms\n",
                  (unsigned)GZ_BLE_PUBLISH_MS);
    Serial.printf("  Wi-Fi: promiscuous mgmt, hop %u..%u @ %u ms, publish every %u ms\n",
                  (unsigned)GZ_WIFI_CH_MIN, (unsigned)GZ_WIFI_CH_MAX,
                  (unsigned)GZ_WIFI_HOP_MS, (unsigned)GZ_WIFI_PUBLISH_MS);
    Serial.println("  passive only — capture is receive-only; publishes return to ch1");
}

void loop()
{
    static uint32_t last_hello = 0;
    uint32_t now = millis();
    if (now - last_hello >= GZ_HELLO_PERIOD_MS) {
        last_hello = now;
        gz_link_send_hello();
    }
    gz_ble_scan_tick();          /* publishes the BLE batch when due */
    gz_wifi_scan_tick();         /* hops channel + publishes the Wi-Fi batch */
    delay(10);
}
