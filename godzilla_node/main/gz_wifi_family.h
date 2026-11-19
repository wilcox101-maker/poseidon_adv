/*
 * gz_wifi_family.h — passive Wi-Fi device-family tag from the IEs already
 * sniffed (detection-breadth orders, 2026-11-19, §2 item 5). Pure:
 * stdint/string only, host-tested by test/test_godzilla_wifi_family.
 *
 * Walks the 802.11 information elements of a management frame (probe request
 * or beacon) that the node already captures — no new sniff mode, no TX — and
 * tags the contact as AP / phone / laptop / IoT ONLY where an element makes it
 * unambiguous. Everything else is GZ_FAM_UNKNOWN. This classifier never
 * guesses: a tag is emitted only on a documented, discriminating signal, so a
 * contact we cannot place stays "unknown" rather than being mislabelled.
 *
 * IE format (802.11): a sequence of [element-id (1)][length (1)][length bytes].
 * A vendor-specific element (id 221 / 0xDD) carries [OUI (3)][type (1)][data].
 * A length that runs past the buffer stops the walk; nothing is ever read past
 * the end.
 *
 * Unambiguous signals currently whitelisted:
 *   - AP  : a TIM element (id 5) is present. TIM is carried only in an AP's
 *           beacons; a client station never sends it.
 *   - IoT : a vendor-specific element with the Espressif OUI (18:FE:34) and no
 *           TIM — an Espressif Wi-Fi module acting as a station. (Verified IEEE
 *           OUI assignment.)
 * phone and laptop are defined in the enum but are emitted only when a
 * genuinely unambiguous marker is matched; none is whitelisted yet (an OUI
 * such as Apple's 00:17:F2 marks a maker, not phone-vs-laptop, so using it
 * would be a guess). They therefore stay "unknown" until a discriminating
 * marker is added — deliberately, per the no-guessing rule.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GZ_FAM_UNKNOWN = 0,
    GZ_FAM_AP,
    GZ_FAM_PHONE,
    GZ_FAM_LAPTOP,
    GZ_FAM_IOT,
} gz_wifi_family_t;

#define GZ_IE_SSID        0x00
#define GZ_IE_TIM         0x05
#define GZ_IE_VENDOR      0xDD

/* OUIs (first three octets, big-endian as on the wire). */
#define GZ_OUI_ESPRESSIF  0x18FE34

static inline uint32_t gz_fam_oui(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

/* Classify a management frame's IE block (`ies`, `len` bytes) into a device
 * family. Bounds-checked at every step; a truncated, oversized or garbage
 * block yields GZ_FAM_UNKNOWN without over-reading. */
static inline gz_wifi_family_t gz_wifi_family_classify(const uint8_t *ies, int len)
{
    if (!ies || len < 0) return GZ_FAM_UNKNOWN;
    bool has_tim = false;
    bool has_espressif = false;
    int i = 0;
    while (i + 2 <= len) {
        uint8_t id = ies[i];
        uint8_t l  = ies[i + 1];
        if (i + 2 + l > len) break;              /* overrun: stop          */
        const uint8_t *d = ies + i + 2;
        if (id == GZ_IE_TIM) {
            has_tim = true;
        } else if (id == GZ_IE_VENDOR && l >= 4) {
            if (gz_fam_oui(d) == GZ_OUI_ESPRESSIF) has_espressif = true;
        }
        i += 2 + l;
    }
    /* Unambiguous order: an AP's TIM wins; else an Espressif station is IoT. */
    if (has_tim)       return GZ_FAM_AP;
    if (has_espressif) return GZ_FAM_IOT;
    return GZ_FAM_UNKNOWN;
}

static inline const char *gz_wifi_family_name(gz_wifi_family_t f)
{
    switch (f) {
    case GZ_FAM_AP:     return "AP";
    case GZ_FAM_PHONE:  return "phone";
    case GZ_FAM_LAPTOP: return "laptop";
    case GZ_FAM_IOT:    return "IoT";
    default:            return "unknown";
    }
}
