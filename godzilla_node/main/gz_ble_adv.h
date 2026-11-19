/*
 * gz_ble_adv.h — passive BLE advertisement decoder for the BLE Lab
 * (post-port orders #98 §2.4). Pure: stdint/string only, host-tested by
 * test/test_godzilla_ble_adv.
 *
 * What a passive scan can honestly show. The node only RECEIVES advertising
 * packets; it never connects. So this decodes what a device *announces*: its
 * name, TX power, appearance (keyboard, mouse, phone, watch, ...), the services
 * it lists, service data and the manufacturer — not its live GATT database,
 * and not HID input reports, which need a connection (and the HID Report Map
 * read over it). Connecting, pairing and emulating stay Dale-gated (#98 §4),
 * so Poseidon's src/hid_decode (a Report-Map decoder) has no passive use here
 * and is deliberately not copied.
 *
 * Format: an advertising payload is a sequence of AD structures, each
 * [length][type][length-1 data bytes]; length 0 ends the payload (Core Spec
 * Vol 3 Part C §11). Malformed lengths stop the parse and set `malformed`;
 * nothing past the end of the buffer is ever read.
 *
 * Names: appearance values and 16-bit service UUIDs from the Bluetooth SIG
 * Assigned Numbers (as mirrored in Zephyr's assigned_numbers.h / uuid.h, and
 * Nordic's SDK). Only well-established values are named; anything else is
 * shown as its hex value, never guessed.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* AD types used here. */
#define GZ_AD_FLAGS            0x01
#define GZ_AD_UUID16_SOME      0x02
#define GZ_AD_UUID16_ALL       0x03
#define GZ_AD_UUID32_SOME      0x04
#define GZ_AD_UUID32_ALL       0x05
#define GZ_AD_UUID128_SOME     0x06
#define GZ_AD_UUID128_ALL      0x07
#define GZ_AD_NAME_SHORT       0x08
#define GZ_AD_NAME_COMPLETE    0x09
#define GZ_AD_TX_POWER         0x0A
#define GZ_AD_SVC_DATA16       0x16
#define GZ_AD_APPEARANCE       0x19
#define GZ_AD_MFR              0xFF

#define GZ_ADV_NAME_MAX        30
#define GZ_ADV_UUID16_MAX      8

typedef struct {
    bool     malformed;              /* a length ran past the buffer          */
    bool     has_flags;     uint8_t  flags;
    bool     has_name;      bool     name_complete;
    char     name[GZ_ADV_NAME_MAX + 1];
    bool     has_tx;        int8_t   tx_dbm;
    bool     has_appearance; uint16_t appearance;
    uint8_t  n_uuid16;               /* listed (capped at GZ_ADV_UUID16_MAX)  */
    uint16_t uuid16[GZ_ADV_UUID16_MAX];
    uint8_t  n_uuid32, n_uuid128;    /* counted, not stored                    */
    bool     has_svc_data;  uint16_t svc_data_uuid;  uint8_t svc_data_len;
    bool     has_mfr;       uint16_t company;        uint8_t mfr_len;   /* bytes after the ID */
} gz_adv_t;

static inline uint16_t gz_adv_le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Keep a name printable: anything outside ASCII 0x20..0x7E becomes '?'. */
static inline void gz_adv_copy_name(char *dst, const uint8_t *src, int n)
{
    if (n > GZ_ADV_NAME_MAX) n = GZ_ADV_NAME_MAX;
    for (int i = 0; i < n; ++i) dst[i] = (src[i] >= 0x20 && src[i] <= 0x7E) ? (char)src[i] : '?';
    dst[n] = '\0';
}

static inline void gz_adv_parse(const uint8_t *p, int len, gz_adv_t *a)
{
    memset(a, 0, sizeof *a);
    int i = 0;
    while (i < len) {
        int L = p[i];
        if (L == 0) break;                         /* end / padding */
        if (i + 1 + L > len) { a->malformed = true; break; }
        uint8_t type = p[i + 1];
        const uint8_t *d = p + i + 2;
        int n = L - 1;                             /* data bytes */
        switch (type) {
        case GZ_AD_FLAGS:
            if (n >= 1) { a->has_flags = true; a->flags = d[0]; }
            break;
        case GZ_AD_UUID16_SOME: case GZ_AD_UUID16_ALL:
            for (int k = 0; k + 1 < n; k += 2)
                if (a->n_uuid16 < GZ_ADV_UUID16_MAX) a->uuid16[a->n_uuid16++] = gz_adv_le16(d + k);
            break;
        case GZ_AD_UUID32_SOME: case GZ_AD_UUID32_ALL:
            a->n_uuid32 = (uint8_t)(a->n_uuid32 + n / 4);
            break;
        case GZ_AD_UUID128_SOME: case GZ_AD_UUID128_ALL:
            a->n_uuid128 = (uint8_t)(a->n_uuid128 + n / 16);
            break;
        case GZ_AD_NAME_COMPLETE:
            a->has_name = true; a->name_complete = true; gz_adv_copy_name(a->name, d, n);
            break;
        case GZ_AD_NAME_SHORT:
            if (!a->name_complete) { a->has_name = true; gz_adv_copy_name(a->name, d, n); }
            break;
        case GZ_AD_TX_POWER:
            if (n >= 1) { a->has_tx = true; a->tx_dbm = (int8_t)d[0]; }
            break;
        case GZ_AD_APPEARANCE:
            if (n >= 2) { a->has_appearance = true; a->appearance = gz_adv_le16(d); }
            break;
        case GZ_AD_SVC_DATA16:
            if (n >= 2 && !a->has_svc_data) {
                a->has_svc_data = true; a->svc_data_uuid = gz_adv_le16(d);
                a->svc_data_len = (uint8_t)(n - 2);
            }
            break;
        case GZ_AD_MFR:
            if (n >= 2 && !a->has_mfr) {
                a->has_mfr = true; a->company = gz_adv_le16(d); a->mfr_len = (uint8_t)(n - 2);
            }
            break;
        default: break;
        }
        i += 1 + L;
    }
}

/* ---- names (Bluetooth SIG Assigned Numbers; unknown -> NULL) -------------- */

/* Appearance: HID subcategories, then the category (value >> 6). */
static inline const char *gz_adv_appearance_name(uint16_t v)
{
    switch (v) {
    case 0x03C0: return "HID";
    case 0x03C1: return "keyboard";
    case 0x03C2: return "mouse";
    case 0x03C3: return "joystick";
    case 0x03C4: return "gamepad";
    case 0x03C5: return "tablet";
    case 0x03C6: return "card reader";
    case 0x03C7: return "digital pen";
    case 0x03C8: return "barcode scanner";
    }
    switch (v >> 6) {
    case 1:  return "phone";
    case 2:  return "computer";
    case 3:  return "watch";
    case 4:  return "clock";
    case 5:  return "display";
    case 15: return "HID";
    default: return NULL;
    }
}

static inline const char *gz_adv_uuid16_name(uint16_t u)
{
    switch (u) {
    case 0x1802: return "Immediate Alert";
    case 0x1804: return "Tx Power";
    case 0x180A: return "Device Info";
    case 0x180D: return "Heart Rate";
    case 0x180F: return "Battery";
    case 0x1812: return "HID";
    case 0xFD6F: return "Exposure Notif";
    case 0xFE2C: return "Fast Pair";
    case 0xFEAA: return "Eddystone";
    default:     return NULL;
    }
}

static inline const char *gz_adv_company_name(uint16_t c)
{
    switch (c) {
    case 0x0006: return "Microsoft";
    case 0x004C: return "Apple";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    default:     return NULL;
    }
}

/* Announces itself as a HID device (appearance category 15 or the HID
 * service in its UUID list). Passive: says nothing about what it types. */
static inline bool gz_adv_is_hid(const gz_adv_t *a)
{
    if (a->has_appearance && (a->appearance >> 6) == 15) return true;
    for (int i = 0; i < a->n_uuid16; ++i) if (a->uuid16[i] == 0x1812) return true;
    return false;
}

/* One-line summary for a list row: name, kind, first named service, maker.
 * Missing parts are skipped; an advert with none of them reads "(no data)". */
static inline int gz_adv_summary(const gz_adv_t *a, char *out, size_t n)
{
    char buf[96]; size_t k = 0; buf[0] = '\0';
#define GZ_ADV_PART(s) do { if ((s) && (s)[0]) { \
        k += (size_t)snprintf(buf + k, sizeof buf - k, "%s%s", k ? " | " : "", (s)); \
        if (k >= sizeof buf) k = sizeof buf - 1; } } while (0)
    if (a->has_name) GZ_ADV_PART(a->name);
    if (a->has_appearance) GZ_ADV_PART(gz_adv_appearance_name(a->appearance));
    for (int i = 0; i < a->n_uuid16; ++i) {
        const char *s = gz_adv_uuid16_name(a->uuid16[i]);
        if (s) { GZ_ADV_PART(s); break; }
    }
    if (a->has_mfr) {
        const char *c = gz_adv_company_name(a->company);
        char hex[12];
        if (!c) { snprintf(hex, sizeof hex, "mfr %04X", (unsigned)a->company); c = hex; }
        GZ_ADV_PART(c);
    }
#undef GZ_ADV_PART
    return snprintf(out, n, "%s", k ? buf : "(no data)");
}
