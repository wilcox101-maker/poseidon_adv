/*
 * odid_decode.h — ASTM F3411 / Open Drone ID decoding for BLE (pure).
 *
 * GODZILLA reuse step 2: the decode logic that used to live inside
 * features/drone_remoteid.cpp (mixed with its drone table, lock and SD log),
 * as a pure header both the Cardputer and the GODZILLA node can call.
 *
 * Layouts follow the reference library opendroneid-core-c
 * (libopendroneid/opendroneid.h): every message is 25 bytes, byte 0 is
 * [MessageType:4][ProtoVersion:4], little-endian fields, lat/lon x1e-7,
 * altitude (m + 1000) * 2, horizontal speed x0.25 (or x0.75 + 63.75 with the
 * multiplier flag), vertical speed x0.5, direction +180 with the EW flag.
 *
 * BLE framing (as opendroneid/receiver-android reads it): AD type 0x16
 * service data, UUID 0xFFFA, then app code 0x0D, a message counter, then one
 * message or a message pack [0xF?][size=25][count 1..9][count x 25].
 * odid_decode_ble_svc() takes the bytes AFTER the UUID.
 *
 * Fixes two bugs in the old in-feature decoder: it read the message from
 * straight after the UUID (so 0x0D was taken as the header and every field was
 * two bytes off), and it read the pack's size byte as the message count.
 *
 * Pure: stdint/stddef/string. No allocation, no Arduino.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ODID_MSG_SIZE       25
#define ODID_PACK_MAX       9
#define ODID_BLE_APP_CODE   0x0D

enum {
    ODID_MSG_BASIC_ID    = 0x0,
    ODID_MSG_LOCATION    = 0x1,
    ODID_MSG_AUTH        = 0x2,
    ODID_MSG_SELF_ID     = 0x3,
    ODID_MSG_SYSTEM      = 0x4,
    ODID_MSG_OPERATOR_ID = 0x5,
    ODID_MSG_PACK        = 0xF,
};

/* Accumulated view of one aircraft. Fields are only written by the message
 * that carries them; the have_* flags say which have been seen. */
typedef struct {
    bool     have_id;
    char     uas_id[21];     /* NUL-terminated, trailing spaces/NULs trimmed */
    uint8_t  id_type;        /* 0 none, 1 serial (CTA-2063-A), 2 CAA, 3 UTM, 4 session */
    uint8_t  ua_type;        /* 0 none, 1 aeroplane, 2 multirotor, ... */

    bool     have_loc;
    uint8_t  status;         /* 0 undeclared, 1 ground, 2 airborne, 3 emergency, ... */
    float    track_deg;      /* 0..359, 361 = unknown (as encoded) */
    float    speed_ms;
    float    vspeed_ms;
    double   lat_deg;
    double   lon_deg;
    float    alt_baro_m;     /* -1000 = invalid */
    float    alt_geo_m;      /* -1000 = invalid */
    float    height_m;       /* -1000 = invalid */

    bool     have_op;
    double   op_lat_deg;
    double   op_lon_deg;
} odid_info_t;

static inline int32_t odid_le32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static inline uint16_t odid_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}
static inline float odid_alt(uint16_t enc) { return (float)enc * 0.5f - 1000.0f; }

/* Decode one 25-byte message into `out`. Returns its message type (0..5,
 * or other recognised types that carry nothing stored), or -1 if the
 * arguments are bad. A pack (0xF) is not decoded here: returns -1. */
static inline int odid_decode_message(const uint8_t *m, size_t len, odid_info_t *out)
{
    if (!m || !out || len < ODID_MSG_SIZE) return -1;
    uint8_t type = (uint8_t)(m[0] >> 4);

    switch (type) {
    case ODID_MSG_BASIC_ID: {
        out->id_type = (uint8_t)(m[1] >> 4);
        out->ua_type = (uint8_t)(m[1] & 0x0F);
        memcpy(out->uas_id, m + 2, 20);
        out->uas_id[20] = '\0';
        for (int i = 19; i >= 0; --i) {
            if (out->uas_id[i] == ' ' || out->uas_id[i] == '\0') out->uas_id[i] = '\0';
            else break;
        }
        out->have_id = true;
        return type;
    }
    case ODID_MSG_LOCATION: {
        uint8_t f = m[1];
        bool ew   = (f & 0x02) != 0;
        bool mult = (f & 0x01) != 0;
        out->status    = (uint8_t)(f >> 4);
        out->track_deg = (float)m[2] + (ew ? 180.0f : 0.0f);
        out->speed_ms  = mult ? ((float)m[3] * 0.75f + 255.0f * 0.25f) : (float)m[3] * 0.25f;
        out->vspeed_ms = (float)(int8_t)m[4] * 0.5f;
        out->lat_deg   = (double)odid_le32(m + 5) / 1e7;
        out->lon_deg   = (double)odid_le32(m + 9) / 1e7;
        out->alt_baro_m = odid_alt(odid_le16(m + 13));
        out->alt_geo_m  = odid_alt(odid_le16(m + 15));
        out->height_m   = odid_alt(odid_le16(m + 17));
        out->have_loc = true;
        return type;
    }
    case ODID_MSG_SYSTEM:
        out->op_lat_deg = (double)odid_le32(m + 2) / 1e7;
        out->op_lon_deg = (double)odid_le32(m + 6) / 1e7;
        out->have_op = true;
        return type;
    case ODID_MSG_AUTH:
    case ODID_MSG_SELF_ID:
    case ODID_MSG_OPERATOR_ID:
        return type;               /* recognised; nothing stored yet */
    default:
        return -1;                 /* pack or unknown */
    }
}

/* Decode a message pack [0xF?][size=25][count 1..9][count x 25].
 * Returns messages decoded, or -1 if the pack header or length is wrong. */
static inline int odid_decode_pack(const uint8_t *m, size_t mlen, odid_info_t *out)
{
    if (!m || !out || mlen < 3 || (m[0] >> 4) != ODID_MSG_PACK) return -1;
    uint8_t size = m[1], count = m[2];
    if (size != ODID_MSG_SIZE || count == 0 || count > ODID_PACK_MAX) return -1;
    if (3 + (size_t)count * ODID_MSG_SIZE > mlen) return -1;

    int n = 0;
    for (uint8_t k = 0; k < count; ++k) {
        const uint8_t *e = m + 3 + (size_t)k * ODID_MSG_SIZE;
        if ((e[0] >> 4) == ODID_MSG_PACK) continue;      /* no nesting */
        if (odid_decode_message(e, ODID_MSG_SIZE, out) >= 0) n++;
    }
    return n;
}

/* Decode a BLE 0xFFFA service-data body (bytes after the UUID).
 * Returns the number of messages decoded (0 if none were storable types),
 * or -1 if the framing is wrong (no app code, short, bad pack header). */
static inline int odid_decode_ble_svc(const uint8_t *sd, size_t len, odid_info_t *out)
{
    if (!sd || !out) return -1;
    if (len < 2 + ODID_MSG_SIZE || sd[0] != ODID_BLE_APP_CODE) return -1;
    const uint8_t *m = sd + 2;
    size_t mlen = len - 2;

    if ((m[0] >> 4) != ODID_MSG_PACK)
        return odid_decode_message(m, mlen, out) >= 0 ? 1 : 0;
    return odid_decode_pack(m, mlen, out);
}

/* ---- Wi-Fi Beacon transport ----
 * Vendor-specific IE 221: [DD][len][FA 0B BC][0D][counter][message pack]
 * (opendroneid-core-c wifi.c odid_wifi_build_message_pack_beacon_frame;
 * receiver-android WiFiBeaconScanner). */
#define ODID_WIFI_IE_VENDOR  221
#define ODID_WIFI_OUI_0      0xFA
#define ODID_WIFI_OUI_1      0x0B
#define ODID_WIFI_OUI_2      0xBC
#define ODID_WIFI_OUI_TYPE   0x0D

/* Find the ODID vendor IE in a tagged-parameter area. On success *body points
 * at the byte after the OUI type (the counter) and *body_len covers the rest
 * of that IE. IEs that run past `len` stop the walk. */
static inline bool odid_wifi_find_ie(const uint8_t *tags, size_t len,
                                     const uint8_t **body, size_t *body_len)
{
    if (!tags) return false;
    size_t i = 0;
    while (i + 2 <= len) {
        uint8_t id = tags[i], l = tags[i + 1];
        if (i + 2 + l > len) return false;
        const uint8_t *v = tags + i + 2;
        if (id == ODID_WIFI_IE_VENDOR && l >= 4 &&
            v[0] == ODID_WIFI_OUI_0 && v[1] == ODID_WIFI_OUI_1 &&
            v[2] == ODID_WIFI_OUI_2 && v[3] == ODID_WIFI_OUI_TYPE) {
            if (body)     *body = v + 4;
            if (body_len) *body_len = (size_t)l - 4;
            return true;
        }
        i += 2 + (size_t)l;
    }
    return false;
}

static inline bool odid_wifi_has_ie(const uint8_t *tags, size_t len)
{
    return odid_wifi_find_ie(tags, len, nullptr, nullptr);
}

/* Decode Remote ID from one beacon / probe-response frame. `len` includes
 * the 4-byte FCS (rx_ctrl.sig_len). Returns messages decoded, 0 if the frame
 * carries no ODID IE, -1 if it is not a parseable beacon/probe response or
 * the pack inside the IE is malformed. */
static inline int odid_decode_wifi_beacon(const uint8_t *f, int len, odid_info_t *out)
{
    if (!f || !out || len < 36 + 4) return -1;
    uint8_t type = (uint8_t)((f[0] >> 2) & 0x3), sub = (uint8_t)((f[0] >> 4) & 0xF);
    if (type != 0 || (sub != 0x8 && sub != 0x5)) return -1;

    const uint8_t *body; size_t blen;
    if (!odid_wifi_find_ie(f + 36, (size_t)(len - 36 - 4), &body, &blen)) return 0;
    if (blen < 1 + 3) return -1;                         /* counter + pack header */
    return odid_decode_pack(body + 1, blen - 1, out);
}
