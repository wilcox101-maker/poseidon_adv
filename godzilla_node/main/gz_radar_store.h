/*
 * gz_radar_store.h — the one shared radar table (P2b).
 *
 * Writers: the BLE / Wi-Fi publish ticks on the Arduino loop, once per
 * published batch. Reader: the LVGL UI task, which only ever works on a copy
 * from gz_radar_snapshot(). A spinlock guards the table (same pattern as
 * gz_stats). The NimBLE and promiscuous callbacks never touch it.
 *
 * It also owns the Recon queue (gz_recon.h), filled in the same critical
 * section that notes a row, so "noted" and "queued for the log" never disagree.
 */
#pragma once

#include <stdint.h>
#include "gz_radar.h"

/* Note every entry of one published batch. kind = GZ_RADAR_WIFI / _BLE;
 * entries = the batch's 10-byte wire entries. */
void gz_radar_feed(uint8_t kind, const uint8_t *entries, uint8_t count, uint32_t now_ms);

/* Recon session log (P3). While on, every row that is due (gz_recon_due) is
 * queued for the SD writer as it is noted. Turning it on starts a fresh
 * session: the queue is emptied and every row counts as not yet logged. */
void gz_radar_store_recon(bool on);

/* SD writer: take up to `max` queued rows, oldest first. *dropped (optional)
 * gets the number of rows refused so far this session because the queue was
 * full. Returns the count. */
int gz_radar_store_recon_pop(gz_radar_row_t *out, int max, uint32_t *dropped);

/* Copy up to `max` rows into `out`; returns the count. */
int gz_radar_snapshot(gz_radar_row_t *out, int max);
