/*
 * gz_le_equip.h — passive law-enforcement-equipment classifier by Wi-Fi OUI
 * (detection §4, unit 3). Pure: stdint only, host-tested by
 * test/test_godzilla_le_equip.
 *
 * Tags a Wi-Fi contact whose source OUI (first 3 octets) is IEEE-registered to
 * a maker of body-worn cameras / law-enforcement equipment as LE_EQUIP. By the
 * IEEE-assigned OUI ONLY — never an SSID or name. src/sigdb_surveillance.h
 * removed Axon SSID patterns because no public fingerprint exists and SSID
 * matches produced false positives; this unit does not reintroduce that.
 *
 * Confidence is LOW for the whole family (orders): a body-cam maker's OUI is a
 * weak signal on its own — the same silicon can appear in non-LE products, and
 * much LE gear uses third-party radios whose OUI is not the vendor's — so a
 * match needs corroboration and never raises the alert overlay by itself.
 *
 * Source (primary): the IEEE MA-L OUI registry (https://standards-oui.ieee.org/).
 * The one verified law-enforcement-equipment registrant is "Axon Enterprise,
 * Inc." (the body-camera / TASER maker), MA-L prefix 00:25:DF, registered
 * 2010-01-05 (confirmed via the IEEE registry; cross-checked on the maclookup
 * aggregator). This is distinct from the unrelated "Axon Networks, Inc." The
 * orders say to ship the family empty if no maker OUI exists; one does, so the
 * table holds it — and only it, to keep the signal honest.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_wifi_mac.h"   /* gz_mac_oui / gz_mac_is_local (shared helpers) */

enum { GZ_LE_EQUIP_NONE = 0, GZ_LE_EQUIP_LOW = 1 };

/* OUIs IEEE-assigned to LE-equipment makers. LOW by definition (see header). */
static const uint32_t gz_le_equip_oui_low[] = {
    0x0025DF,   /* Axon Enterprise, Inc. (MA-L, body cameras / TASER) */
};

/* Confidence for a bare 24-bit OUI: LOW if an LE-equipment maker, else NONE. */
static inline int gz_le_equip_oui_conf(uint32_t oui)
{
    for (unsigned i = 0; i < sizeof gz_le_equip_oui_low / sizeof gz_le_equip_oui_low[0]; ++i)
        if (gz_le_equip_oui_low[i] == oui) return GZ_LE_EQUIP_LOW;
    return GZ_LE_EQUIP_NONE;
}

/* Classify a station/AP MAC. A randomized (locally-administered) address is not
 * attributed to any vendor. */
static inline int gz_le_equip_classify_mac(const uint8_t mac[6])
{
    if (gz_mac_is_local(mac)) return GZ_LE_EQUIP_NONE;
    return gz_le_equip_oui_conf(gz_mac_oui(mac));
}
