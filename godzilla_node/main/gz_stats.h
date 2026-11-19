/*
 * gz_stats.h — the one shared gz_status_t instance (P2a status screen).
 *
 * Writers: the BLE / Wi-Fi publish ticks on the Arduino loop. Reader: the LVGL
 * UI task (a different task, possibly a different core). A spinlock guards the
 * struct; the UI only ever works on a copy from gz_stats_snapshot(). Neither
 * the NimBLE callback nor the promiscuous callback touches this — they only
 * stage detections; the counters move when a batch is actually published.
 */
#pragma once

#include "gz_status.h"

/* Fixed-at-boot identity, shown on the status screen. */
void gz_stats_set_static(const char *node_name, uint8_t link_channel,
                         uint32_t psram_kb, uint8_t i2c_found);

/* One non-empty batch of `entries` detections went on air for `radio`. */
void gz_stats_note_publish(gz_radio_t radio, uint8_t entries);

/* Consistent copy for the UI. */
void gz_stats_snapshot(gz_status_t *out);
