/*
 * main.cpp — GODZILLA node entry.
 *
 * Task 3a: ESP-NOW link + HELLO every 5 s so the Cardputer (S3) registers the
 * node (C5 -> Diagnostics). Task 3b: continuous passive BLE scan feeding
 * gz_batch.h, published over ESP-NOW on a ~1 s tick. The passive Wi-Fi loop
 * lands in 3c.
 *
 * Passive only — nothing transmits except ESP-NOW HELLO/publish.
 */
#include <Arduino.h>

#include "gz_link.h"
#include "gz_hello.h"      /* GZ_HELLO_NAME / GZ_ROLE for the boot log */
#include "gz_ble_scan.h"   /* task 3b: passive BLE scan → batch → publish */

static constexpr uint32_t GZ_HELLO_PERIOD_MS = 5000;

/* This node's id on the fabric; carried in detection batches. The S3 ties it
 * to this node's MAC via the HELLO it hears. */
static constexpr uint8_t GZ_NODE_ID = 2;

void setup()
{
    Serial.begin(115200);
    delay(200);

    gz_link_init();
    gz_link_send_hello();        /* announce immediately, like c5_node */
    gz_ble_scan_init(GZ_NODE_ID);

    Serial.println();
    Serial.println("POSEIDON GODZILLA node — task 3b: passive BLE scan + publish");
    Serial.printf("  link: WiFi STA ch%u, ESP-NOW, broadcast peer\n",
                  (unsigned)GZ_LINK_CHANNEL);
    Serial.printf("  HELLO '%s' role=%u every %lu ms\n",
                  GZ_HELLO_NAME, (unsigned)GZ_ROLE,
                  (unsigned long)GZ_HELLO_PERIOD_MS);
    Serial.printf("  BLE: passive scan, publish every %u ms\n",
                  (unsigned)GZ_BLE_PUBLISH_MS);
    Serial.println("  passive only — Wi-Fi scan lands in 3c");
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
    delay(10);
}
