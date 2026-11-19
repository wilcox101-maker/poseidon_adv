/*
 * gz_ble_eddystone.h — passive Eddystone decoder for the BLE Lab
 * (detection-breadth orders, 2026-11-19, §1.1). Pure: stdint/string only,
 * host-tested by test/test_godzilla_ble_eddystone.
 *
 * Second standard-beacon family after iBeacon (gz_ble_beacon.h); AltBeacon
 * follows in its own PR. Decode-only, passive: Eddystone broadcasts a beacon
 * identifier, a URL, telemetry or an ephemeral ID — nothing here connects,
 * pairs or resolves an owner (that stays Dale-gated, §4). EID is reported as
 * "present" only; its rotating value is not an identity we resolve.
 *
 * Wire format (Google Eddystone spec): the advert carries Service Data for
 * the 16-bit UUID 0xFEAA (AD type 0x16, first two data bytes 0xAA 0xFE
 * little-endian). The service-data body's first byte is the frame type:
 *   0x00 UID : tx_power(1,s8) namespace(10) instance(6) [rfu(2)]
 *   0x10 URL : tx_power(1,s8) scheme(1) encoded-url(...)
 *   0x20 TLM : version(1) vbatt(2,be mV) temp(2,be 8.8) adv_cnt(4,be) sec_cnt(4,be)
 *   0x30 EID : tx_power(1,s8) eid(8)
 * All multi-byte fields are big-endian. Lengths are checked at every step;
 * a short, truncated, oversized or mis-typed body rejects cleanly without
 * over-reading. URL scheme/suffix expansion follows the spec's code tables.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define GZ_EDDYSTONE_UUID    0xFEAA
#define GZ_EDDY_UID          0x00
#define GZ_EDDY_URL          0x10
#define GZ_EDDY_TLM          0x20
#define GZ_EDDY_EID          0x30

typedef struct {
    bool     valid;        /* a well-formed Eddystone frame was decoded     */
    uint8_t  frame;        /* one of GZ_EDDY_*                               */
    int8_t   tx_power;      bool has_tx;    /* UID/URL/EID ranging byte       */
    /* UID */
    uint8_t  namespace_id[10];
    uint8_t  instance_id[6];
    /* URL */
    char     url[64];       /* expanded, printable; truncated if longer      */
    /* TLM */
    uint16_t vbatt_mv;
    int16_t  temp_milli_c;  /* 8.8 fixed -> whole degrees in temp_c          */
    float    temp_c;
    uint32_t adv_cnt;
    uint32_t sec_cnt;
    /* EID */
    uint8_t  eid[8];
} gz_eddystone_t;

static inline uint16_t gz_ed_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint16_t gz_ed_be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline uint32_t gz_ed_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Eddystone-URL scheme prefixes (byte 0 of the URL field). */
static inline const char *gz_eddy_url_scheme(uint8_t c)
{
    switch (c) {
    case 0x00: return "http://www.";
    case 0x01: return "https://www.";
    case 0x02: return "http://";
    case 0x03: return "https://";
    default:   return NULL;
    }
}

/* Eddystone-URL suffix expansion codes (0x00..0x0D); other bytes are literal
 * printable ASCII. */
static inline const char *gz_eddy_url_suffix(uint8_t c)
{
    switch (c) {
    case 0x00: return ".com/";
    case 0x01: return ".org/";
    case 0x02: return ".edu/";
    case 0x03: return ".net/";
    case 0x04: return ".info/";
    case 0x05: return ".biz/";
    case 0x06: return ".gov/";
    case 0x07: return ".com";
    case 0x08: return ".org";
    case 0x09: return ".edu";
    case 0x0A: return ".net";
    case 0x0B: return ".info";
    case 0x0C: return ".biz";
    case 0x0D: return ".gov";
    default:   return NULL;
    }
}

/* Append a NUL-terminated string to dst[cap] at *len, clamping. */
static inline void gz_eddy_append(char *dst, size_t cap, size_t *len, const char *s)
{
    while (*s && *len + 1 < cap) dst[(*len)++] = *s++;
    dst[*len] = '\0';
}

/* Decode an Eddystone service-data body (the bytes AFTER the 0xAA 0xFE UUID),
 * `n` bytes long. Internal; gz_eddystone_parse finds the body first. */
static inline bool gz_eddystone_decode_body(const uint8_t *d, int n, gz_eddystone_t *out)
{
    memset(out, 0, sizeof *out);
    if (n < 1) return false;
    out->frame = d[0];
    switch (out->frame) {
    case GZ_EDDY_UID:
        if (n < 1 + 1 + 10 + 6) return false;    /* type+tx+ns+inst (rfu optional) */
        out->has_tx = true; out->tx_power = (int8_t)d[1];
        memcpy(out->namespace_id, d + 2, 10);
        memcpy(out->instance_id, d + 12, 6);
        out->valid = true;
        return true;
    case GZ_EDDY_URL: {
        if (n < 1 + 1 + 1) return false;         /* type+tx+scheme, url may be empty */
        out->has_tx = true; out->tx_power = (int8_t)d[1];
        const char *scheme = gz_eddy_url_scheme(d[2]);
        if (!scheme) return false;               /* reserved scheme -> reject  */
        size_t len = 0;
        gz_eddy_append(out->url, sizeof out->url, &len, scheme);
        for (int i = 3; i < n; ++i) {
            uint8_t c = d[i];
            const char *suf = gz_eddy_url_suffix(c);
            if (suf) { gz_eddy_append(out->url, sizeof out->url, &len, suf); }
            else if (c >= 0x20 && c <= 0x7E) {
                if (len + 1 < sizeof out->url) { out->url[len++] = (char)c; out->url[len] = '\0'; }
            } else {
                return false;                    /* control / non-ASCII -> reject */
            }
        }
        out->valid = true;
        return true;
    }
    case GZ_EDDY_TLM:
        if (n < 1 + 1 + 2 + 2 + 4 + 4) return false;
        out->vbatt_mv = gz_ed_be16(d + 2);
        {
            int16_t raw = (int16_t)gz_ed_be16(d + 4);   /* 8.8 signed fixed    */
            out->temp_c = (float)raw / 256.0f;
            out->temp_milli_c = (int16_t)((int32_t)raw * 1000 / 256);
        }
        out->adv_cnt = gz_ed_be32(d + 6);
        out->sec_cnt = gz_ed_be32(d + 10);
        out->valid = true;
        return true;
    case GZ_EDDY_EID:
        if (n < 1 + 1 + 8) return false;
        out->has_tx = true; out->tx_power = (int8_t)d[1];
        memcpy(out->eid, d + 2, 8);
        out->valid = true;
        return true;
    default:
        return false;                            /* unknown frame type        */
    }
}

/* Decode Eddystone from a raw advertising payload. Walks AD structures, finds
 * the 0xFEAA Service Data (AD type 0x16), and decodes its body. Returns true
 * and fills `out` only for a complete, well-formed frame. Bounds-checked at
 * every step; truncated/oversized/garbage input rejects without over-reading. */
static inline bool gz_eddystone_parse(const uint8_t *p, int len, gz_eddystone_t *out)
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
        int n = L - 1;
        if (type == 0x16 && n >= 2 && gz_ed_le16(d) == GZ_EDDYSTONE_UUID) {
            return gz_eddystone_decode_body(d + 2, n - 2, out);
        }
        i += 1 + L;
    }
    return false;
}

static inline const char *gz_eddystone_frame_name(uint8_t f)
{
    switch (f) {
    case GZ_EDDY_UID: return "UID";
    case GZ_EDDY_URL: return "URL";
    case GZ_EDDY_TLM: return "TLM";
    case GZ_EDDY_EID: return "EID";
    default:          return "?";
    }
}

/* One-line BLE Lab detail for a decoded Eddystone frame. */
static inline int gz_eddystone_summary(const gz_eddystone_t *b, char *out, size_t n)
{
    if (!b->valid) return snprintf(out, n, "%s", "");
    switch (b->frame) {
    case GZ_EDDY_UID:
        return snprintf(out, n,
            "Eddystone UID %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/"
            "%02x%02x%02x%02x%02x%02x",
            b->namespace_id[0], b->namespace_id[1], b->namespace_id[2],
            b->namespace_id[3], b->namespace_id[4], b->namespace_id[5],
            b->namespace_id[6], b->namespace_id[7], b->namespace_id[8],
            b->namespace_id[9],
            b->instance_id[0], b->instance_id[1], b->instance_id[2],
            b->instance_id[3], b->instance_id[4], b->instance_id[5]);
    case GZ_EDDY_URL:
        return snprintf(out, n, "Eddystone URL %s", b->url);
    case GZ_EDDY_TLM:
        return snprintf(out, n, "Eddystone TLM %umV %.1fC adv=%u",
                        (unsigned)b->vbatt_mv, (double)b->temp_c,
                        (unsigned)b->adv_cnt);
    case GZ_EDDY_EID:
        return snprintf(out, n, "Eddystone EID (present)");
    default:
        return snprintf(out, n, "Eddystone ?");
    }
}
