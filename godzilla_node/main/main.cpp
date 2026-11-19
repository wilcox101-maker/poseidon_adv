/*
 * main.cpp — GODZILLA node bring-up skeleton.
 *
 * Minimal Arduino entry for [env:godzilla] (Waveshare ESP32-S3-Touch-LCD-1.85B).
 * Task 1 of the P1 node-producer brief: prove the env builds in CI and that the
 * shared v5 wire protocol (c5_node/main/proto.h) and the detection batching
 * core (gz_batch.h, #50) compile and link on-target on the S3.
 *
 * No radio yet. Passive only — this transmits nothing. The NimBLE/Wi-Fi scan
 * loop and the ESP-NOW publisher land in task 3; the batches below are init'd
 * and ready for that loop to feed via gz_ble_batch_add()/gz_wifi_batch_add().
 */
#include <Arduino.h>

#include "gz_batch.h"   /* pulls in ../../c5_node/main/proto.h (v5 wire structs) */

/* This node's id on the POSEIDON fabric. Placeholder until task 3 wires HELLO;
 * the S3 maps node_id -> sender MAC from the HELLO frame. */
static constexpr uint8_t GZ_NODE_ID = 2;

static posei_ble_det_t  s_ble;
static posei_wifi_det_t s_wifi;

void setup()
{
    Serial.begin(115200);
    delay(200);

    gz_ble_batch_init(&s_ble, GZ_NODE_ID);
    gz_wifi_batch_init(&s_wifi, GZ_NODE_ID);

    Serial.println();
    Serial.println("POSEIDON GODZILLA node — bring-up skeleton");
    Serial.printf("  proto v%u, POSEI_DET_MAX=%u\n",
                  (unsigned)POSEI_VERSION, (unsigned)POSEI_DET_MAX);
    Serial.printf("  wire sizes: ble_det=%u B  wifi_det=%u B  msg=%u B\n",
                  (unsigned)sizeof(posei_ble_det_t),
                  (unsigned)sizeof(posei_wifi_det_t),
                  (unsigned)sizeof(posei_msg_t));
    Serial.println("  radio: not started (passive scan + ESP-NOW publish land in task 3)");
}

void loop()
{
    /* Heartbeat only until the radio loop exists. */
    static uint32_t beats = 0;
    Serial.printf("gz alive %lu\n", (unsigned long)beats++);
    delay(5000);
}
