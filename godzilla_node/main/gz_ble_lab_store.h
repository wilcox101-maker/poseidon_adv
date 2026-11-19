/*
 * gz_ble_lab_store.h — the one shared BLE Lab table (#98 §2.4). Writer: the
 * passive NimBLE scan callback (gz_ble_scan.cpp). Reader: the LVGL task, on a
 * copy. A spinlock guards it (same pattern as gz_radar_store).
 */
#pragma once

#include <stdint.h>
#include "gz_ble_lab.h"

void gz_ble_lab_feed(const uint8_t addr[6], uint8_t atype, int8_t rssi,
                     const uint8_t *payload, int len, uint32_t now_ms);

/* Copy up to `max` rows into `out`; returns the count. */
int gz_ble_lab_snapshot(gz_lab_row_t *out, int max);
