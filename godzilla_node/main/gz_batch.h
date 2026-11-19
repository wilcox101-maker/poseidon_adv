/*
 * gz_batch.h — GODZILLA node detection batching core.
 *
 * Producer half of the detection-publish contract
 * (docs/godzilla-detection-publish-contract.md). The radio task feeds every
 * sighting in here; this accumulates them into the wire structs from
 * c5_node/main/proto.h and packs a ready-to-send posei_msg_t.
 *
 * Pure: no radio, no heap, no Arduino/ESP-IDF headers — the batch IS the wire
 * struct, so there is no second copy and nothing to keep in sync. Compiles as
 * C and C++ and is covered natively by test/test_godzilla_batch.
 *
 * Rules it enforces:
 *   - one entry per device per batch (dedup by address; BLE also by addr_type),
 *   - count never exceeds POSEI_DET_MAX — a full batch refuses a new device
 *     with GZ_ADD_FULL so the caller can pack, send, reset and re-add,
 *   - payload_len is exactly posei_*_det_bytes(count) (== 2 + 10*count),
 *   - an empty batch packs to 0 bytes: nothing is sent.
 *
 * Typical loop:
 *     if (gz_ble_batch_add(&b, addr, type, rssi, fam) == GZ_ADD_FULL) {
 *         size_t n = gz_ble_batch_pack(&b, &msg, seq++);   // send n bytes
 *         gz_ble_batch_reset(&b);
 *         gz_ble_batch_add(&b, addr, type, rssi, fam);
 *     }
 *     // ...and on the publish timer: pack, send if n > 0, reset.
 *
 * Passive only: this builds detection reports. It transmits nothing itself.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../../c5_node/main/proto.h"

/* gz_*_batch_add() results. */
enum {
    GZ_ADD_NEW     = 0,  /* device appended as a new entry              */
    GZ_ADD_UPDATED = 1,  /* device already in this batch; entry merged  */
    GZ_ADD_FULL    = 2,  /* batch is full and the device is not in it   */
};

/* A family is "specific" once a classifier has said more than "some device".
 * A specific family is never overwritten by a vaguer later sighting. */
static inline int gz_family_is_specific(uint8_t family)
{
    return family != POSEI_DET_FAM_UNKNOWN && family != POSEI_DET_FAM_BLE_GENERIC;
}

/* ---- BLE ---- */

static inline void gz_ble_batch_init(posei_ble_det_t *b, uint8_t node_id)
{
    memset(b, 0, sizeof(*b));
    b->node_id = node_id;
}

/* Empty the batch for the next window; the node identity is kept. */
static inline void gz_ble_batch_reset(posei_ble_det_t *b)
{
    uint8_t node_id = b->node_id;
    gz_ble_batch_init(b, node_id);
}

/* Record one BLE sighting. A device already in the batch keeps one entry:
 * the strongest RSSI wins and a specific family replaces a vaguer one. */
static inline int gz_ble_batch_add(posei_ble_det_t *b, const uint8_t addr[6],
                                   uint8_t addr_type, int8_t rssi, uint8_t family)
{
    uint8_t n = b->count > POSEI_DET_MAX ? (uint8_t)POSEI_DET_MAX : b->count;
    for (uint8_t i = 0; i < n; i++) {
        posei_ble_det_entry_t *e = &b->det[i];
        if (e->addr_type == addr_type && memcmp(e->addr, addr, 6) == 0) {
            if (rssi > e->rssi) e->rssi = rssi;
            if (gz_family_is_specific(family) && !gz_family_is_specific(e->family))
                e->family = family;
            return GZ_ADD_UPDATED;
        }
    }
    if (n >= POSEI_DET_MAX) return GZ_ADD_FULL;

    posei_ble_det_entry_t *e = &b->det[n];
    memcpy(e->addr, addr, 6);
    e->addr_type = addr_type;
    e->rssi      = rssi;
    e->family    = family;
    e->flags     = 0;
    b->count     = (uint8_t)(n + 1);
    return GZ_ADD_NEW;
}

/* Pack the batch into a POSEI_TYPE_RESP_BLE_DET message. Returns the number
 * of bytes to put on the wire (posei_wire_len), or 0 if the batch is empty —
 * in which case `m` is untouched and nothing should be sent. */
static inline size_t gz_ble_batch_pack(const posei_ble_det_t *b, posei_msg_t *m, uint16_t seq)
{
    if (b->count == 0) return 0;
    size_t used = posei_ble_det_bytes(b->count);
    m->magic       = POSEI_MAGIC;
    m->version     = POSEI_VERSION;
    m->type        = POSEI_TYPE_RESP_BLE_DET;
    m->seq         = seq;
    m->payload_len = (uint16_t)used;
    memcpy(m->payload, b, used);
    if (b->count > POSEI_DET_MAX) m->payload[offsetof(posei_ble_det_t, count)] = POSEI_DET_MAX;
    return posei_wire_len(m);
}

/* ---- Wi-Fi ---- */

static inline void gz_wifi_batch_init(posei_wifi_det_t *b, uint8_t node_id)
{
    memset(b, 0, sizeof(*b));
    b->node_id = node_id;
}

static inline void gz_wifi_batch_reset(posei_wifi_det_t *b)
{
    uint8_t node_id = b->node_id;
    gz_wifi_batch_init(b, node_id);
}

/* Record one Wi-Fi sighting (AP or STA), keyed by MAC. On a repeat sighting
 * the strongest RSSI wins and brings its channel with it; a specific family
 * replaces POSEI_DET_FAM_UNKNOWN. */
static inline int gz_wifi_batch_add(posei_wifi_det_t *b, const uint8_t bssid[6],
                                    uint8_t channel, int8_t rssi, uint8_t family)
{
    uint8_t n = b->count > POSEI_DET_MAX ? (uint8_t)POSEI_DET_MAX : b->count;
    for (uint8_t i = 0; i < n; i++) {
        posei_wifi_det_entry_t *e = &b->det[i];
        if (memcmp(e->bssid, bssid, 6) == 0) {
            if (rssi > e->rssi) { e->rssi = rssi; e->channel = channel; }
            if (gz_family_is_specific(family) && !gz_family_is_specific(e->family))
                e->family = family;
            return GZ_ADD_UPDATED;
        }
    }
    if (n >= POSEI_DET_MAX) return GZ_ADD_FULL;

    posei_wifi_det_entry_t *e = &b->det[n];
    memcpy(e->bssid, bssid, 6);
    e->channel = channel;
    e->rssi    = rssi;
    e->family  = family;
    e->flags   = 0;
    b->count   = (uint8_t)(n + 1);
    return GZ_ADD_NEW;
}

/* Pack into a POSEI_TYPE_RESP_WIFI_DET message; 0 if the batch is empty. */
static inline size_t gz_wifi_batch_pack(const posei_wifi_det_t *b, posei_msg_t *m, uint16_t seq)
{
    if (b->count == 0) return 0;
    size_t used = posei_wifi_det_bytes(b->count);
    m->magic       = POSEI_MAGIC;
    m->version     = POSEI_VERSION;
    m->type        = POSEI_TYPE_RESP_WIFI_DET;
    m->seq         = seq;
    m->payload_len = (uint16_t)used;
    memcpy(m->payload, b, used);
    if (b->count > POSEI_DET_MAX) m->payload[offsetof(posei_wifi_det_t, count)] = POSEI_DET_MAX;
    return posei_wire_len(m);
}
