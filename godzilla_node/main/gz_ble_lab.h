/*
 * gz_ble_lab.h — the BLE Lab's table of recent advertisements (#98 §2.4).
 *
 * The radar keeps only a family per device; the Lab needs what each device
 * actually announced. This keeps the last raw advertising payload (legacy
 * advertising: up to 31 bytes) of the GZ_LAB_MAX most recently heard BLE
 * devices, keyed by address, least-recently-heard evicted. Decoding happens
 * when the screen draws (gz_ble_adv.h), not in the radio callback.
 *
 * Fed from gz_ble_scan.cpp's passive onResult — exactly the packets the node
 * already receives; nothing new is captured or transmitted.
 *
 * Pure: stdint/string only, host-tested (test/test_godzilla_ble_adv). The
 * spinlocked store is gz_ble_lab_store.{h,cpp}.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define GZ_LAB_MAX       24
#define GZ_LAB_PAYLOAD   31     /* legacy advertising PDU data limit */

typedef struct {
    uint8_t  addr[6];
    uint8_t  atype;             /* 0 public, 1 random, ... (NimBLE)   */
    int8_t   rssi;
    uint8_t  len;               /* bytes kept in payload              */
    bool     truncated;         /* the advert was longer (extended)   */
    uint16_t seen;              /* adverts heard, saturating          */
    uint32_t last_ms;
    uint8_t  payload[GZ_LAB_PAYLOAD];
} gz_lab_row_t;

typedef struct {
    gz_lab_row_t e[GZ_LAB_MAX];
    int          n;
} gz_lab_table_t;

static inline void gz_lab_clear(gz_lab_table_t *t) { t->n = 0; }

/* Note one received advert. All-zero addresses are ignored (returns -1).
 * Returns the row index. Unsigned ages, so millis() wrap is safe. */
static inline int gz_lab_note(gz_lab_table_t *t, const uint8_t addr[6], uint8_t atype,
                              int8_t rssi, const uint8_t *payload, int len, uint32_t now_ms)
{
    static const uint8_t zero[6] = { 0, 0, 0, 0, 0, 0 };
    if (memcmp(addr, zero, 6) == 0) return -1;
    int i = -1;
    for (int k = 0; k < t->n; ++k)
        if (memcmp(t->e[k].addr, addr, 6) == 0) { i = k; break; }
    if (i < 0) {
        if (t->n < GZ_LAB_MAX) {
            i = t->n++;
        } else {
            i = 0;
            uint32_t oldest = now_ms - t->e[0].last_ms;
            for (int k = 1; k < t->n; ++k) {
                uint32_t age = now_ms - t->e[k].last_ms;
                if (age > oldest) { oldest = age; i = k; }
            }
        }
        memcpy(t->e[i].addr, addr, 6);
        t->e[i].seen = 0;
    }
    gz_lab_row_t *r = &t->e[i];
    if (len < 0) len = 0;
    r->truncated = len > GZ_LAB_PAYLOAD;
    r->len       = (uint8_t)(r->truncated ? GZ_LAB_PAYLOAD : len);
    if (r->len) memcpy(r->payload, payload, r->len);
    r->atype   = atype;
    r->rssi    = rssi;
    r->last_ms = now_ms;
    if (r->seen < 0xFFFF) r->seen++;
    return i;
}

/* Order a copy strongest-first (insertion sort; n <= GZ_LAB_MAX). */
static inline void gz_lab_sort_by_rssi(gz_lab_row_t *rows, int n)
{
    for (int i = 1; i < n; ++i) {
        gz_lab_row_t key = rows[i];
        int j = i - 1;
        while (j >= 0 && rows[j].rssi < key.rssi) { rows[j + 1] = rows[j]; --j; }
        rows[j + 1] = key;
    }
}
