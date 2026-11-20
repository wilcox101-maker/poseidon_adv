/*
 * det_classify.h — shared BLE advertisement → detection-family classifier.
 *
 * GODZILLA reuse step 1: one pure classifier that the Cardputer features and
 * the GODZILLA node (also an ESP32-S3) can both call, instead of each feature
 * file deciding inside its NimBLE/UI code. Input is the raw advertising
 * payload (AD structures: len, type, data…), e.g. NimBLE getPayload().
 * Output is a DET_FAM_* code, numerically identical to POSEI_DET_FAM_* on the
 * wire (pinned by test_det_classify).
 *
 * Pure: stdint/stddef + sigdb_surveillance.h. No allocation, no Arduino.
 *
 * Priority (first match wins):
 *   1. REMOTEID  service data UUID 0xFFFA (ASTM F3411)
 *   2. RAVEN     manufacturer CID 0x09C8 (XUNTONG), or a Raven custom 16-bit
 *                service UUID. Standard SIG services in the Raven list
 *                (0x18xx: Device Information etc.) are NOT used — they are on
 *                most BLE devices and would flag everything.
 *   3. FINDMY    Apple CID 0x004C, Continuity type 0x12 (offline finding).
 *                AirTag vs other Find My accessories is not separable from the
 *                advertisement alone, so DET_FAM_AIRTAG is not emitted here.
 *   4. TILE      service UUID 0xFEED or 0xFD84 (list or service data)
 *   5. SMARTTAG  service data 0xFD5A, or Samsung CID 0x0075 with 42 09
 *                (ble_db.cpp rule). A bare Samsung CID is a phone/buds.
 *   6. IBEACON   Apple CID, type 0x02, length 0x15
 *   else BLE_GENERIC.
 *
 * 128-bit UUIDs reduce to 16 bits only when they use the Bluetooth base UUID.
 * Malformed AD structures (zero length, length past the buffer) end parsing;
 * whatever was parsed before still counts.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sigdb_surveillance.h"

/* Mirrors POSEI_DET_FAM_* / C5_DET_FAM_* (append only, never renumber). */
enum {
    DET_FAM_UNKNOWN     = 0,
    DET_FAM_WIFI_AP     = 1,
    DET_FAM_WIFI_STA    = 2,
    DET_FAM_BLE_GENERIC = 3,
    DET_FAM_FINDMY      = 4,
    DET_FAM_AIRTAG      = 5,
    DET_FAM_IBEACON     = 6,
    DET_FAM_REMOTEID    = 7,
    DET_FAM_FLOCK       = 8,
    DET_FAM_RAVEN       = 9,
    DET_FAM_TILE        = 10,
    DET_FAM_SMARTTAG    = 11,
    DET_FAM_SKIMMER     = 12,
    DET_FAM_CAMERA      = 13,
    DET_FAM_LE_EQUIP    = 14,
    DET_FAM_ALPR        = 15,
    DET_FAM_SMARTGLASS  = 16,
};

/* AD types used here (Bluetooth Core Supplement, part A). */
#define DET_AD_UUID16_PART   0x02
#define DET_AD_UUID16_FULL   0x03
#define DET_AD_UUID128_PART  0x06
#define DET_AD_UUID128_FULL  0x07
#define DET_AD_SVC_DATA16    0x16
#define DET_AD_MFG_DATA      0xFF

static inline const char *det_family_name(uint8_t fam)
{
    switch (fam) {
    case DET_FAM_WIFI_AP:     return "Wi-Fi AP";
    case DET_FAM_WIFI_STA:    return "Wi-Fi STA";
    case DET_FAM_BLE_GENERIC: return "BLE";
    case DET_FAM_FINDMY:      return "Find My";
    case DET_FAM_AIRTAG:      return "AirTag";
    case DET_FAM_IBEACON:     return "iBeacon";
    case DET_FAM_REMOTEID:    return "Remote ID";
    case DET_FAM_FLOCK:       return "Flock";
    case DET_FAM_RAVEN:       return "Raven";
    case DET_FAM_TILE:        return "Tile";
    case DET_FAM_SMARTTAG:    return "SmartTag";
    case DET_FAM_SKIMMER:     return "BT skimmer";
    case DET_FAM_CAMERA:      return "IP camera";
    case DET_FAM_LE_EQUIP:    return "LE gear";
    case DET_FAM_ALPR:        return "ALPR";
    case DET_FAM_SMARTGLASS:  return "Smart glasses";
    default:                  return "Unknown";
    }
}

/* Label for the tracker screens, or NULL if the family is not a tracker.
 * Every label fits an 11-char + NUL buffer. */
static inline const char *det_tracker_label(uint8_t fam)
{
    switch (fam) {
    case DET_FAM_FINDMY:   return "Find My";
    case DET_FAM_AIRTAG:   return "AirTag";
    case DET_FAM_SMARTTAG: return "SmartTag";
    case DET_FAM_TILE:     return "Tile";
    default:               return nullptr;
    }
}

/* Raven custom service UUID: in the Raven list and not a SIG-assigned
 * service (0x1800-0x18FF). */
static inline bool det_raven_custom_uuid(uint16_t u)
{
    if (u >= 0x1800 && u <= 0x18FF) return false;
    return raven_uuid_match(u);
}

/* Little-endian 128-bit UUID → 16-bit, only for the Bluetooth base UUID
 * 0000xxxx-0000-1000-8000-00805F9B34FB. Returns false otherwise. */
static inline bool det_uuid128_to16(const uint8_t *u, uint16_t *out)
{
    static const uint8_t base_lo[12] = { 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00,
                                         0x00, 0x80, 0x00, 0x10, 0x00, 0x00 };
    for (int i = 0; i < 12; ++i) if (u[i] != base_lo[i]) return false;
    if (u[14] != 0 || u[15] != 0) return false;
    *out = (uint16_t)(u[12] | (u[13] << 8));
    return true;
}

static inline int det_hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline bool det_hex16(const char *s, uint16_t *out)
{
    uint16_t v = 0;
    for (int i = 0; i < 4; ++i) {
        int h = det_hexval(s[i]);
        if (h < 0) return false;
        v = (uint16_t)((v << 4) | h);
    }
    *out = v;
    return true;
}

/* UUID text → 16-bit, for the forms NimBLE versions print: "XXXX", "0xXXXX",
 * or the 36-char "0000XXXX-0000-1000-8000-00805F9B34FB" (base UUID only, any
 * case). Any other 128-bit UUID is vendor-specific and returns false. */
static inline bool det_uuid_str_to16(const char *s, uint16_t *out)
{
    static const char tail[] = "-0000-1000-8000-00805f9b34fb";
    if (!s || !out) return false;
    size_t n = 0;
    while (s[n]) ++n;
    if (n == 4) return det_hex16(s, out);
    if (n == 6 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return det_hex16(s + 2, out);
    if (n == 36) {
        if (s[0] != '0' || s[1] != '0' || s[2] != '0' || s[3] != '0') return false;
        for (int i = 0; i < 28; ++i) {
            char c = s[8 + i];
            if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
            if (c != tail[i]) return false;
        }
        return det_hex16(s + 4, out);
    }
    return false;
}

typedef struct {
    bool remoteid, raven, findmy, tile, smarttag, ibeacon;
} det_hits_t;

static inline void det_note_uuid16(det_hits_t *h, uint16_t u)
{
    if (det_raven_custom_uuid(u)) h->raven = true;
    if (u == 0xFEED || u == 0xFD84) h->tile = true;
}

static inline uint8_t det_classify_ble(const uint8_t *adv, size_t len)
{
    det_hits_t h = { false, false, false, false, false, false };
    if (adv) {
        size_t i = 0;
        while (i < len) {
            uint8_t l = adv[i];
            if (l == 0 || i + 1 + l > len) break;
            uint8_t type = adv[i + 1];
            const uint8_t *d = adv + i + 2;
            size_t dl = (size_t)l - 1;

            if (type == DET_AD_UUID16_PART || type == DET_AD_UUID16_FULL) {
                for (size_t k = 0; k + 1 < dl; k += 2)
                    det_note_uuid16(&h, (uint16_t)(d[k] | (d[k + 1] << 8)));
            } else if (type == DET_AD_UUID128_PART || type == DET_AD_UUID128_FULL) {
                for (size_t k = 0; k + 15 < dl; k += 16) {
                    uint16_t u;
                    if (det_uuid128_to16(d + k, &u)) det_note_uuid16(&h, u);
                }
            } else if (type == DET_AD_SVC_DATA16 && dl >= 2) {
                uint16_t u = (uint16_t)(d[0] | (d[1] << 8));
                if (u == 0xFFFA) h.remoteid = true;
                if (u == 0xFD5A) h.smarttag = true;
                det_note_uuid16(&h, u);
            } else if (type == DET_AD_MFG_DATA && dl >= 2) {
                uint16_t cid = (uint16_t)(d[0] | (d[1] << 8));
                if (raven_company_match(cid)) h.raven = true;
                if (cid == 0x004C && dl >= 3) {
                    if (d[2] == 0x12) h.findmy = true;
                    if (d[2] == 0x02 && dl >= 4 && d[3] == 0x15) h.ibeacon = true;
                }
                if (cid == 0x0075 && dl >= 4 && d[2] == 0x42 && d[3] == 0x09)
                    h.smarttag = true;
            }
            i += 1 + (size_t)l;
        }
    }
    if (h.remoteid) return DET_FAM_REMOTEID;
    if (h.raven)    return DET_FAM_RAVEN;
    if (h.findmy)   return DET_FAM_FINDMY;
    if (h.tile)     return DET_FAM_TILE;
    if (h.smarttag) return DET_FAM_SMARTTAG;
    if (h.ibeacon)  return DET_FAM_IBEACON;
    return DET_FAM_BLE_GENERIC;
}
