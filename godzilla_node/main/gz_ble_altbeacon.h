/*
 * gz_ble_altbeacon.h — passive AltBeacon decoder for the BLE Lab
 * (detection-breadth orders, 2026-11-19, §1.1). Pure: stdint/string only,
 * host-tested by test/test_godzilla_ble_altbeacon.
 *
 * Third and last standard-beacon family after iBeacon (#109) and Eddystone
 * (#110). Decode-only, passive: an AltBeacon broadcasts a manufacturer ID, a
 * 20-byte beacon ID and a reference RSSI — nothing here connects, pairs or
 * resolves an owner (that stays Dale-gated, §4). The beacon ID is a broadcast
 * constant, not a device identity.
 *
 * Wire format (AltBeacon spec, github.com/AltBeacon/spec), inside the
 * manufacturer-specific AD structure (AD type 0xFF):
 *   MFG ID       2 bytes, little-endian (the beacon maker's SIG company ID)
 *   beacon code  2 bytes, 0xBEAC sent big-endian (0xBE, 0xAC) — the family tag
 *   beacon ID   20 bytes
 *   ref RSSI     1 signed byte (RSSI at 1 m)
 *   MFG reserved 1 byte
 * The whole AD structure is thus length 0x1B (27 bytes incl. the type byte).
 * Unlike iBeacon, the company ID is not fixed; the 0xBEAC code is what
 * identifies an AltBeacon. Lengths are checked at every step; a short,
 * truncated, oversized or mis-tagged candidate rejects cleanly without
 * over-reading.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define GZ_ALTBEACON_CODE_HI  0xBE
#define GZ_ALTBEACON_CODE_LO  0xAC

typedef struct {
    bool     valid;        /* a complete AltBeacon was decoded              */
    uint16_t company;      /* MFG ID, little-endian on the wire             */
    uint8_t  beacon_id[20];
    int8_t   ref_rssi;     /* reference RSSI at 1 m, signed                 */
    uint8_t  mfg_reserved;
} gz_altbeacon_t;

static inline uint16_t gz_ab_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Decode an AltBeacon from a raw advertising payload. Walks AD structures,
 * finds the 0xFF manufacturer AD carrying the 0xBEAC beacon code, and
 * extracts the fields. Returns true and fills `out` only for a complete,
 * well-formed AltBeacon. Bounds-checked at every step; a short buffer, an
 * overrun, a missing beacon code or too-few data bytes reject cleanly. */
static inline bool gz_altbeacon_parse(const uint8_t *p, int len, gz_altbeacon_t *out)
{
    memset(out, 0, sizeof *out);
    if (!p || len < 0) return false;
    int i = 0;
    while (i < len) {
        int L = p[i];
        if (L == 0) break;
        if (i + 1 + L > len) break;              /* overrun: stop            */
        uint8_t type = p[i + 1];
        const uint8_t *d = p + i + 2;
        int n = L - 1;                           /* data byte count          */
        if (type == 0xFF && n >= 26) {
            /* mfg(2)+code(2)+id(20)+rssi(1)+reserved(1) = 26 data bytes. */
            if (d[2] == GZ_ALTBEACON_CODE_HI && d[3] == GZ_ALTBEACON_CODE_LO) {
                out->company = gz_ab_le16(d);
                memcpy(out->beacon_id, d + 4, 20);
                out->ref_rssi = (int8_t)d[24];
                out->mfg_reserved = d[25];
                out->valid = true;
                return true;
            }
        }
        i += 1 + L;
    }
    return false;
}

/* Format the 20-byte beacon ID as 40 lowercase hex chars (+ NUL). `out` must
 * hold at least 41 bytes. */
static inline void gz_altbeacon_id_str(const gz_altbeacon_t *b, char *out, size_t n)
{
    if (n == 0) return;
    size_t k = 0;
    for (int i = 0; i < 20 && k + 2 < n; ++i)
        k += (size_t)snprintf(out + k, n - k, "%02x", b->beacon_id[i]);
    out[k < n ? k : n - 1] = '\0';
}

/* One-line BLE Lab detail for a decoded AltBeacon:
 *   "AltBeacon <mfr-or-hex> <id> @<rssi>dBm". */
static inline int gz_altbeacon_summary(const gz_altbeacon_t *b, char *out, size_t n)
{
    if (!b->valid) return snprintf(out, n, "%s", "");
    char id[41];
    gz_altbeacon_id_str(b, id, sizeof id);
    return snprintf(out, n, "AltBeacon mfr%04X %s @%ddBm",
                    (unsigned)b->company, id, (int)b->ref_rssi);
}
