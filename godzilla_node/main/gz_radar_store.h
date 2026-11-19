/*
 * gz_radar_store.h — the one shared radar table (P2b).
 *
 * Writers: the BLE / Wi-Fi publish ticks on the Arduino loop, once per
 * published batch. Reader: the LVGL UI task, which only ever works on a copy
 * from gz_radar_snapshot(). A spinlock guards the table (same pattern as
 * gz_stats). The NimBLE and promiscuous callbacks never touch it.
 */
#pragma once

#include <stdint.h>
#include "gz_radar.h"

/* Note every entry of one published batch. kind = GZ_RADAR_WIFI / _BLE;
 * entries = the batch's 10-byte wire entries. */
void gz_radar_feed(uint8_t kind, const uint8_t *entries, uint8_t count, uint32_t now_ms);

/* Copy up to `max` rows into `out`; returns the count. */
int gz_radar_snapshot(gz_radar_row_t *out, int max);
