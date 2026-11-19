/*
 * main.cpp — GODZILLA node entry.
 *
 * Task 3a: bring up the ESP-NOW link and announce the node with a HELLO every
 * 5 s, so the Cardputer (S3) registers it as a peer (C5 -> Diagnostics). The
 * passive BLE and Wi-Fi scan loops that feed gz_batch.h and publish detections
 * land in 3b and 3c; this file only announces presence for now.
 *
 * Passive only — nothing transmits except ESP-NOW HELLO/publish.
 */
#include <Arduino.h>

#include "gz_link.h"
#include "gz_hello.h"   /* GZ_HELLO_NAME / GZ_ROLE for the boot log */
#include "gz_batch.h"   /* staging areas for 3b/3c; init'd ready to feed */

static constexpr uint32_t GZ_HELLO_PERIOD_MS = 5000;

/* This node's id on the fabric; carried in detection batches (3b/3c). The S3
 * ties it to this node's MAC via the HELLO it hears. */
static constexpr uint8_t GZ_NODE_ID = 2;

static posei_ble_det_t  s_ble;
static posei_wifi_det_t s_wifi;

void setup()
{
    Serial.begin(115200);
    delay(200);

    gz_ble_batch_init(&s_ble, GZ_NODE_ID);
    gz_wifi_batch_init(&s_wifi, GZ_NODE_ID);

    gz_link_init();
    gz_link_send_hello();   /* announce immediately, like c5_node */

    Serial.println();
    Serial.println("POSEIDON GODZILLA node — task 3a: ESP-NOW link + HELLO");
    Serial.printf("  link: WiFi STA ch%u, ESP-NOW, broadcast peer\n",
                  (unsigned)GZ_LINK_CHANNEL);
    Serial.printf("  HELLO '%s' role=%u every %lu ms\n",
                  GZ_HELLO_NAME, (unsigned)GZ_ROLE,
                  (unsigned long)GZ_HELLO_PERIOD_MS);
    Serial.println("  passive only — scan loops land in 3b/3c");
}

void loop()
{
    static uint32_t last_hello = 0;
    uint32_t now = millis();
    if (now - last_hello >= GZ_HELLO_PERIOD_MS) {
        last_hello = now;
        gz_link_send_hello();
    }
    delay(10);
}
