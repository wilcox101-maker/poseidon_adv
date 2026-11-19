/*
 * godzilla_det.h — S3-side merge of GODZILLA detection-publish batches.
 *
 * Wire (approach B, protocol v5, see c5_node/main/proto.h and src/c5_cmd.h):
 *   RESP_WIFI_DET (60) / RESP_BLE_DET (61) payload =
 *     node_id u8, count u8, then `count` 10-byte entries:
 *       addr[6], aux u8 (Wi-Fi: channel, BLE: addr_type), rssi i8,
 *       family u8 (POSEI_DET_FAM_*), flags u8.
 *   Wi-Fi and BLE entries share these offsets, so one parser serves both.
 *
 * Pure C/C++ (stdint/string only) so the native tests cover it; c5_cmd.cpp
 * calls it from the ESP-NOW recv callback while holding s_mux. No allocation,
 * no logging, no Arduino.
 *
 * Attribution: `src_mac` is the ESP-NOW sender (authoritative); `node_id` is
 * what the node says about itself (informational). Both are kept per row.
 *
 * Trust: `count` and every field come from the radio. A frame whose count
 * exceeds GZ_DET_BATCH_MAX, or whose payload_len is not exactly
 * 2 + 10 * count (the consumer contract, #45), is rejected whole, so no byte
 * past payload_len is ever read.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Mirrors of wire constants. test_godzilla_det pins each against proto.h. */
#define GZ_TYPE_WIFI_DET   60
#define GZ_TYPE_BLE_DET    61
#define GZ_DET_BATCH_MAX   64   /* POSEI_DET_MAX / C5_DET_MAX */
#define GZ_DET_HDR_SZ      2    /* node_id + count */
#define GZ_DET_ENTRY_SZ    10

/* Rows kept on the S3. 64 x 24 B = 1.5 KB, heap-allocated by c5_begin. */
#define GZ_DET_TABLE_MAX   64

enum { GZ_KIND_WIFI = 0, GZ_KIND_BLE = 1 };

typedef struct {
    uint8_t  addr[6];      /* BSSID / STA MAC, or BLE address              */
    uint8_t  kind;         /* GZ_KIND_*                                     */
    uint8_t  aux;          /* Wi-Fi: channel · BLE: addr_type (0 pub/1 rnd) */
    int8_t   rssi;         /* latest                                        */
    uint8_t  family;       /* POSEI_DET_FAM_* as received (not validated)   */
    uint8_t  flags;        /* as received; reserved in P0                   */
    uint8_t  node_id;      /* self-reported by the publishing node          */
    uint8_t  src_mac[6];   /* ESP-NOW sender of the latest sighting         */
    uint16_t hits;         /* sightings, saturating                         */
    uint32_t last_seen_ms; /* millis() at the latest sighting               */
} gz_det_t;

typedef struct {
    gz_det_t e[GZ_DET_TABLE_MAX];
    int      n;
} gz_det_table_t;

static inline void gz_det_clear(gz_det_table_t *t)
{
    if (t) t->n = 0;
}

/* Row index of (kind, addr), or -1. */
static inline int gz_det_find(const gz_det_table_t *t, uint8_t kind, const uint8_t addr[6])
{
    for (int i = 0; i < t->n; ++i)
        if (t->e[i].kind == kind && memcmp(t->e[i].addr, addr, 6) == 0) return i;
    return -1;
}

/* Merge one batch payload into the table.
 * Returns the number of entries applied (all-zero addresses are skipped; an
 * in-batch duplicate counts each time it refreshes its row), or -1 if the
 * frame is malformed, in which case the table is unchanged. */
static inline int gz_det_ingest(gz_det_table_t *t, uint8_t kind,
                                const uint8_t *payload, uint16_t payload_len,
                                const uint8_t src_mac[6], uint32_t now_ms)
{
    static const uint8_t zero[6] = { 0, 0, 0, 0, 0, 0 };

    if (!t || !payload || !src_mac) return -1;
    if (kind != GZ_KIND_WIFI && kind != GZ_KIND_BLE) return -1;
    if (payload_len < GZ_DET_HDR_SZ) return -1;

    const uint8_t node_id = payload[0];
    const uint8_t count   = payload[1];
    if (count > GZ_DET_BATCH_MAX) return -1;
    /* Exact length, per docs/godzilla-detection-publish-contract.md (#45). */
    if ((size_t)GZ_DET_HDR_SZ + (size_t)count * GZ_DET_ENTRY_SZ != payload_len) return -1;

    int applied = 0;
    for (uint8_t k = 0; k < count; ++k) {
        const uint8_t *p = payload + GZ_DET_HDR_SZ + (size_t)k * GZ_DET_ENTRY_SZ;
        if (memcmp(p, zero, 6) == 0) continue;

        int i = gz_det_find(t, kind, p);
        if (i < 0) {
            if (t->n < GZ_DET_TABLE_MAX) {
                i = t->n++;
            } else {
                /* Evict the least recently seen row. Unsigned age is
                 * correct across the millis() wrap. */
                i = 0;
                uint32_t oldest = now_ms - t->e[0].last_seen_ms;
                for (int j = 1; j < t->n; ++j) {
                    uint32_t age = now_ms - t->e[j].last_seen_ms;
                    if (age > oldest) { oldest = age; i = j; }
                }
            }
            memcpy(t->e[i].addr, p, 6);
            t->e[i].kind = kind;
            t->e[i].hits = 0;
        }

        gz_det_t *e = &t->e[i];
        e->aux     = p[6];
        e->rssi    = (int8_t)p[7];
        e->family  = p[8];
        e->flags   = p[9];
        e->node_id = node_id;
        memcpy(e->src_mac, src_mac, 6);
        e->last_seen_ms = now_ms;
        if (e->hits < 0xFFFF) e->hits++;
        applied++;
    }
    return applied;
}
