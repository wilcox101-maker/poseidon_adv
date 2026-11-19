/*
 * gz_ble_scan.h — GODZILLA passive BLE scan → batch → publish (task 3b).
 *
 * Continuous NimBLE passive scan (no scan requests). Each advertisement is
 * classified by the shared src/det_classify.h and added to a lock-protected
 * staging batch (gz_batch.h) in the scan callback — allocation-free. A ~1 s
 * tick on the main loop packs the batch and broadcasts it over ESP-NOW
 * (gz_link.h), then resets it. Fire-and-forget: the S3 sends no ack.
 *
 * Passive only: BLE here is receive-only; the only transmit is the ESP-NOW
 * publish on the loop. Nothing is sent from the scan callback.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../c5_node/main/proto.h"

/* Publish cadence (orders: ~1 s). */
#define GZ_BLE_PUBLISH_MS 1000u

/* Pure: is a publish due? True when the batch asked to flush early (it filled
 * to POSEI_DET_MAX mid-window) or once period_ms has elapsed. Unsigned
 * subtraction, so it is correct across the millis() wraparound (~49.7 days).
 * Pinned by test/test_godzilla_publish_due. */
static inline bool gz_publish_due(uint32_t last_ms, uint32_t now_ms,
                                  uint32_t period_ms, bool flush)
{
    return flush || (uint32_t)(now_ms - last_ms) >= period_ms;
}

/* Init NimBLE + the staging batch (node_id) and start the continuous passive
 * scan. Call once from setup(), after gz_link_init(). */
void gz_ble_scan_init(uint8_t node_id);

/* Call every loop iteration. When a publish is due it packs the staging BLE
 * batch, broadcasts it (nothing is sent for an empty window), and resets it. */
void gz_ble_scan_tick(void);
