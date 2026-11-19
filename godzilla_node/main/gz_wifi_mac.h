/*
 * gz_wifi_mac.h — passive Wi-Fi MAC privacy flag + OUI vendor naming for the
 * radar/Recon views (detection-breadth orders, 2026-11-19, §2 item 4). Pure:
 * stdint/string only, host-tested by test/test_godzilla_wifi_mac.
 *
 * From a management frame's source address already sniffed (no new sniff mode,
 * no TX), surface two facts:
 *   - the locally-administered (U/L) bit: a device using a RANDOMIZED MAC
 *     (phones since iOS 14 / Android 10 for privacy) sets it, so the address
 *     reads "randomized" rather than naming a maker that isn't there;
 *   - otherwise, the OUI (first 3 octets) names the vendor where we know it,
 *     else the raw OUI is shown. Never guessed.
 *
 * Bit layout (IEEE 802): octet 0, bit 0 (0x01) = multicast/group,
 * bit 1 (0x02) = locally administered. A locally-administered unicast address
 * is the randomized case; its OUI is not a real assignment, so we do not look
 * it up.
 *
 * OUI table: only long-established, verified IEEE assignments (Espressif,
 * Raspberry Pi). Confirmed against the IEEE OUI registry. Decode-only.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* IEEE 802 octet-0 bits. */
#define GZ_MAC_MULTICAST  0x01
#define GZ_MAC_LOCAL      0x02

/* True if the group/multicast bit is set (not an individual station). */
static inline bool gz_mac_is_multicast(const uint8_t mac[6])
{
    return (mac[0] & GZ_MAC_MULTICAST) != 0;
}

/* True if the locally-administered bit is set — i.e. a randomized/private MAC
 * rather than a globally-unique, vendor-assigned one. */
static inline bool gz_mac_is_local(const uint8_t mac[6])
{
    return (mac[0] & GZ_MAC_LOCAL) != 0;
}

/* The 24-bit OUI (first three octets) as a single value. */
static inline uint32_t gz_mac_oui(const uint8_t mac[6])
{
    return ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | (uint32_t)mac[2];
}

/* Vendor for a globally-administered OUI; NULL if not in the table. Values are
 * IEEE OUI registry assignments. */
static inline const char *gz_mac_oui_vendor(uint32_t oui)
{
    switch (oui) {
    /* Espressif Inc. */
    case 0x240AC4: case 0x30AEA4: case 0xA4CF12:
        return "Espressif";
    /* Raspberry Pi (Foundation / Trading Ltd). */
    case 0xB827EB: case 0xDCA632: case 0xE45F01:
        return "Raspberry Pi";
    default:
        return NULL;
    }
}

/* Compose the radar/Recon label for a station address into out[n]:
 *   locally-administered unicast -> "randomized"
 *   known vendor                 -> "Espressif"
 *   unknown global OUI           -> "OUI b827eb" (lowercase hex)
 * Returns snprintf length. A multicast address (rare for a station) still
 * reports its U/L nature so it is never mislabelled as a vendor. */
static inline int gz_mac_label(const uint8_t mac[6], char *out, size_t n)
{
    if (gz_mac_is_local(mac)) return snprintf(out, n, "%s", "randomized");
    const char *v = gz_mac_oui_vendor(gz_mac_oui(mac));
    if (v) return snprintf(out, n, "%s", v);
    return snprintf(out, n, "OUI %02x%02x%02x", mac[0], mac[1], mac[2]);
}
