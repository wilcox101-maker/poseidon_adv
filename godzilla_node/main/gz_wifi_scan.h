/*
 * gz_wifi_scan.h — GODZILLA passive Wi-Fi scan → batch → publish (task 3c).
 *
 * Wi-Fi promiscuous capture, management frames only, channel-hopping 1..13.
 * Each frame is classified by the shared src/flock_frame.h and added to a
 * lock-protected staging batch (gz_batch.h) in the promiscuous callback —
 * allocation-free, enqueue only, never transmits. The main loop hops the
 * channel on a ~250 ms dwell and, on a ~1 s tick, packs + broadcasts the batch
 * over ESP-NOW (gz_link.h), which returns to the C5 link channel for the send.
 *
 * Passive only: capture is receive-only; the only transmit is the ESP-NOW
 * publish/HELLO. Nothing is sent from the promiscuous callback.
 */
#pragma once

#include <stdint.h>
#include "../../c5_node/main/proto.h"

#define GZ_WIFI_CH_MIN      1
#define GZ_WIFI_CH_MAX      13
#define GZ_WIFI_HOP_MS      250u    /* dwell per channel (~3.25 s full sweep) */
#define GZ_WIFI_PUBLISH_MS  1000u   /* same publish cadence as BLE */

/* Pure: next channel in the 1..GZ_WIFI_CH_MAX cycle (…13 -> 1). Any out-of-range
 * current (0, >13) folds back into range. Pinned by test/test_godzilla_wifi_hop. */
static inline uint8_t gz_next_channel(uint8_t cur)
{
    return (uint8_t)((cur % GZ_WIFI_CH_MAX) + 1);
}

/* Enable promiscuous mgmt capture on the already-running STA (gz_link_init must
 * have run first) and init the staging batch. Call once from setup(). */
void gz_wifi_scan_init(uint8_t node_id);

/* Call every loop iteration: hops the channel on the dwell timer and publishes
 * the Wi-Fi batch when due (nothing is sent for an empty window). */
void gz_wifi_scan_tick(void);

/* Monotonic count of promiscuous management frames captured since boot. Read-only
 * liveness for the bench self-test (item J); the page diffs it over a window for
 * a frames/s reading. Never transmits. */
uint32_t gz_wifi_scan_frames(void);
