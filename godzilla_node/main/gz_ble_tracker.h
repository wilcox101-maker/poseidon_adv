/*
 * gz_ble_tracker.h — protective public tracker surfacing for the BLE Lab
 * (detection-breadth orders, 2026-11-19, §1 item 3). Pure: stdint/string
 * only, host-tested by test/test_godzilla_ble_tracker.
 *
 * Anti-stalking surfacing from the advert alone. It flags adverts whose
 * PUBLIC, well-documented signatures match a common item tracker, so an
 * unknown tracker travelling with you becomes visible in the BLE Lab as
 * "possible tracker (<family>)".
 *
 * DECODE-ONLY and strictly bounded to the public broadcast:
 *   - no pairing, no connection, no GATT read;
 *   - no identity, owner or account resolution;
 *   - no attempt to derive or track the rotating key/MAC.
 * All of that is Dale-gated (§4). This only matches a signature already
 * present in a packet the node passively hears, exactly like the beacon
 * decoders alongside it.
 *
 * Public signatures used (all documented, none proprietary):
 *   - Apple Find My / AirTag (separated): manufacturer data, company 0x004C,
 *     containing an Apple TLV of type 0x12 ("offline finding"). This is the
 *     advert an AirTag away from its owner broadcasts — the stalking-relevant
 *     one.
 *   - Tile: the 0xFEED service UUID (Tile, Inc.), in the service-UUID list or
 *     service data.
 *   - Samsung Galaxy SmartTag: the 0xFD5A service UUID (Samsung Electronics /
 *     SmartThings Find), in the service-UUID list or service data.
 * Anything else is not flagged; no family is ever guessed.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    GZ_TRACKER_NONE = 0,
    GZ_TRACKER_FINDMY,       /* Apple Find My / AirTag (separated)      */
    GZ_TRACKER_TILE,         /* Tile                                    */
    GZ_TRACKER_SMARTTAG,     /* Samsung Galaxy SmartTag                 */
} gz_tracker_t;

#define GZ_TILE_UUID        0xFEED
#define GZ_SMARTTAG_UUID    0xFD5A
#define GZ_APPLE_COMPANY    0x004C
#define GZ_APPLE_TLV_FINDMY 0x12

static inline uint16_t gz_trk_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Scan the Apple manufacturer-data body (bytes after the 0x004C company ID,
 * `n` long) for an Apple TLV of type `want`. Apple packs its manufacturer
 * data as a sequence of [type][len][len data bytes]; a length that overruns
 * stops the scan. Bounds-checked; never over-reads. */
static inline bool gz_trk_apple_has_tlv(const uint8_t *d, int n, uint8_t want)
{
    int i = 0;
    while (i + 2 <= n) {
        uint8_t t = d[i];
        uint8_t l = d[i + 1];
        if (i + 2 + l > n) break;          /* overrun: stop            */
        if (t == want) return true;
        i += 2 + l;
    }
    return false;
}

/* Classify a raw advertising payload as a known public tracker, or
 * GZ_TRACKER_NONE. Walks the AD structures (same [len][type][data] format as
 * the beacon decoders), bounds-checked at every step; truncated, oversized or
 * garbage input yields NONE without over-reading. First match wins in the
 * order Find My, Tile, SmartTag. */
static inline gz_tracker_t gz_tracker_classify(const uint8_t *p, int len)
{
    if (!p || len < 0) return GZ_TRACKER_NONE;
    int i = 0;
    while (i < len) {
        int L = p[i];
        if (L == 0) break;
        if (i + 1 + L > len) break;            /* overrun: stop         */
        uint8_t type = p[i + 1];
        const uint8_t *d = p + i + 2;
        int n = L - 1;                         /* data byte count       */
        switch (type) {
        case 0x02: case 0x03:                  /* incomplete/complete 16-bit UUIDs */
            for (int k = 0; k + 1 < n; k += 2) {
                uint16_t u = gz_trk_le16(d + k);
                if (u == GZ_TILE_UUID)     return GZ_TRACKER_TILE;
                if (u == GZ_SMARTTAG_UUID) return GZ_TRACKER_SMARTTAG;
            }
            break;
        case 0x16:                             /* 16-bit service data   */
            if (n >= 2) {
                uint16_t u = gz_trk_le16(d);
                if (u == GZ_TILE_UUID)     return GZ_TRACKER_TILE;
                if (u == GZ_SMARTTAG_UUID) return GZ_TRACKER_SMARTTAG;
            }
            break;
        case 0xFF:                             /* manufacturer-specific */
            if (n >= 2 && gz_trk_le16(d) == GZ_APPLE_COMPANY) {
                if (gz_trk_apple_has_tlv(d + 2, n - 2, GZ_APPLE_TLV_FINDMY))
                    return GZ_TRACKER_FINDMY;
            }
            break;
        default: break;
        }
        i += 1 + L;
    }
    return GZ_TRACKER_NONE;
}

static inline bool gz_tracker_is_tracker(gz_tracker_t t) { return t != GZ_TRACKER_NONE; }

static inline const char *gz_tracker_family(gz_tracker_t t)
{
    switch (t) {
    case GZ_TRACKER_FINDMY:   return "Find My";
    case GZ_TRACKER_TILE:     return "Tile";
    case GZ_TRACKER_SMARTTAG: return "SmartTag";
    default:                  return NULL;
    }
}

/* BLE Lab badge for a classified advert: "possible tracker (Find My)" etc.,
 * or "" when not a tracker. Protective wording — it never asserts ownership
 * or identity, only that a public tracker signature is present. */
static inline const char *gz_tracker_label(gz_tracker_t t)
{
    switch (t) {
    case GZ_TRACKER_FINDMY:   return "possible tracker (Find My)";
    case GZ_TRACKER_TILE:     return "possible tracker (Tile)";
    case GZ_TRACKER_SMARTTAG: return "possible tracker (SmartTag)";
    default:                  return "";
    }
}
