/*
 * gz_ble_beacon.h — passive standard-beacon decoder for the BLE Lab
 * (detection-breadth orders, 2026-11-19, §1.1). Pure: stdint/string only,
 * host-tested by test/test_godzilla_ble_beacon.
 *
 * This file adds iBeacon decoding only; Eddystone and AltBeacon follow in
 * their own PRs (finest-grain first). It labels what an advert *announces* —
 * nothing here connects, pairs or resolves an identity (that stays Dale-gated,
 * §4). An iBeacon proximity UUID/major/minor is a broadcast constant printed
 * on the beacon itself, not a device identity.
 *
 * iBeacon layout, inside the manufacturer-specific AD structure (AD type
 * 0xFF): company ID 0x004C (Apple, little-endian on the wire), then
 *   subtype 0x02, subtype length 0x15 (21), 16-byte proximity UUID,
 *   major (2 bytes, big-endian), minor (2 bytes, big-endian),
 *   measured power (1 signed byte, RSSI at 1 m).
 * The whole AD structure is thus length 0x1A (26 data bytes after the type).
 *
 * Format safety mirrors gz_ble_adv.h: the advertising payload is a sequence
 * of [length][type][data] AD structures; a length that runs past the buffer
 * stops the walk. Nothing past the end of the buffer is ever read, and an
 * iBeacon is only reported when every one of its fixed-length fields is fully
 * present — a truncated or oversized candidate is rejected, not half-decoded.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define GZ_IBEACON_COMPANY   0x004C   /* Apple, as the SIG company ID       */
#define GZ_IBEACON_SUBTYPE   0x02
#define GZ_IBEACON_SUBLEN    0x15     /* 21 bytes of iBeacon body            */

typedef struct {
    bool     valid;        /* a complete iBeacon was decoded                 */
    uint8_t  uuid[16];     /* proximity UUID, big-endian as sent             */
    uint16_t major;
    uint16_t minor;
    int8_t   power;        /* measured power (RSSI at 1 m), signed           */
} gz_ibeacon_t;

static inline uint16_t gz_ib_be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline uint16_t gz_ib_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Decode an iBeacon from a raw advertising payload. Returns true and fills
 * `out` only for a complete, well-formed iBeacon; returns false (out->valid
 * cleared) otherwise. Bounds are checked at every step; a short buffer, a
 * length that overruns, a non-Apple maker or a wrong subtype/length all
 * reject cleanly without over-reading. */
static inline bool gz_ibeacon_parse(const uint8_t *p, int len, gz_ibeacon_t *out)
{
    memset(out, 0, sizeof *out);
    if (!p || len < 0) return false;
    int i = 0;
    while (i < len) {
        int L = p[i];
        if (L == 0) break;                         /* end / padding */
        if (i + 1 + L > len) break;                /* overrun: stop */
        uint8_t type = p[i + 1];
        const uint8_t *d = p + i + 2;              /* data bytes       */
        int n = L - 1;                             /* data byte count  */
        if (type == 0xFF && n >= 25) {
            /* company(2) + subtype(1) + sublen(1) + uuid(16) + major(2)
             * + minor(2) + power(1) = 25 data bytes. */
            if (gz_ib_le16(d) == GZ_IBEACON_COMPANY &&
                d[2] == GZ_IBEACON_SUBTYPE && d[3] == GZ_IBEACON_SUBLEN) {
                memcpy(out->uuid, d + 4, 16);
                out->major = gz_ib_be16(d + 20);
                out->minor = gz_ib_be16(d + 22);
                out->power = (int8_t)d[24];
                out->valid = true;
                return true;
            }
        }
        i += 1 + L;
    }
    return false;
}

/* Format the proximity UUID as the canonical 8-4-4-4-12 hyphenated string
 * (36 chars + NUL). `out` must hold at least 37 bytes. */
static inline void gz_ibeacon_uuid_str(const gz_ibeacon_t *b, char *out, size_t n)
{
    const uint8_t *u = b->uuid;
    snprintf(out, n,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
        u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

/* One-line BLE Lab detail for a decoded iBeacon:
 *   "iBeacon <uuid> <major>/<minor> @<power>dBm". Returns snprintf length. */
static inline int gz_ibeacon_summary(const gz_ibeacon_t *b, char *out, size_t n)
{
    if (!b->valid) return snprintf(out, n, "%s", "");
    char uuid[37];
    gz_ibeacon_uuid_str(b, uuid, sizeof uuid);
    return snprintf(out, n, "iBeacon %s %u/%u @%ddBm",
                    uuid, (unsigned)b->major, (unsigned)b->minor, (int)b->power);
}
