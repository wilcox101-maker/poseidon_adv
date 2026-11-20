/*
 * gz_alpr.h — passive non-Flock ALPR classifier by Wi-Fi OUI (detection §4,
 * unit 4). Pure: stdint only, host-tested by test/test_godzilla_alpr.
 *
 * Tags a Wi-Fi contact whose source OUI (first 3 octets) is IEEE-registered to
 * an automatic-licence-plate-reader (ALPR) maker as ALPR. By the IEEE-assigned
 * OUI ONLY — never an SSID or name. Flock's own cameras are classified
 * separately (POSEI_DET_FAM_FLOCK, flock_frame.h); this unit is the OTHER ALPR
 * vendors.
 *
 * Confidence is LOW for the whole family (orders): an ALPR maker's OUI is a
 * weak signal — the listed makers also ship non-ALPR security products on the
 * same OUI — so a match needs corroboration and never raises the alert overlay
 * by itself.
 *
 * Source (primary): the IEEE MA-L OUI registry (https://standards-oui.ieee.org/).
 * The listed prefixes are full 24-bit MA-L blocks assigned to "Genetec Inc."
 * (St. Laurent, Quebec — the AutoVu / SharpV ALPR maker): 00:BF:15 (2012) and
 * 0C:BF:15 (2011), cross-checked on the maclookup aggregator. Genetec Inc. also
 * holds an IAB (00:50:C2:BE:7, a shared 24-bit prefix), deliberately not listed.
 *
 * Deliberately NOT listed, because matching them on the 24-bit OUI would
 * misattribute other companies' devices:
 *   - 00:0A:B1 "GENETEC Corporation" is an UNRELATED company in Shinjuku,
 *     Tokyo (registered 2002), not Genetec Inc. of Quebec — a name collision
 *     like Axon Networks vs Axon Enterprise (gz_le_equip.h);
 *   - ELSAG (Leonardo's ALPR division) holds only a 36-bit MA-S sub-block
 *     (70:B3:D5:1C:5…) under the IEEE IAB pool 70:B3:D5, whose 24-bit prefix is
 *     shared across many unrelated registrants — so there is no ELSAG-exclusive
 *     24-bit OUI to match;
 *   - "Motorola Solutions / Vigilant" ship ALPR under Motorola's broad OUIs
 *     that cover overwhelmingly non-ALPR two-way radios and body cameras, so a
 *     24-bit match there would tag ordinary radios as plate readers.
 * Both are left out rather than added as noisy signals.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_wifi_mac.h"   /* gz_mac_oui / gz_mac_is_local (shared helpers) */

enum { GZ_ALPR_NONE = 0, GZ_ALPR_LOW = 1 };

/* OUIs IEEE-assigned (full 24-bit MA-L) to non-Flock ALPR makers. LOW. */
static const uint32_t gz_alpr_oui_low[] = {
    0x00BF15,   /* Genetec Inc. (MA-L, 2012)        */
    0x0CBF15,   /* Genetec Inc. (MA-L, 2011)        */
};

/* Confidence for a bare 24-bit OUI: LOW if a listed ALPR maker, else NONE. */
static inline int gz_alpr_oui_conf(uint32_t oui)
{
    for (unsigned i = 0; i < sizeof gz_alpr_oui_low / sizeof gz_alpr_oui_low[0]; ++i)
        if (gz_alpr_oui_low[i] == oui) return GZ_ALPR_LOW;
    return GZ_ALPR_NONE;
}

/* Classify a station/AP MAC. A randomized (locally-administered) address is not
 * attributed to any vendor. */
static inline int gz_alpr_classify_mac(const uint8_t mac[6])
{
    if (gz_mac_is_local(mac)) return GZ_ALPR_NONE;
    return gz_alpr_oui_conf(gz_mac_oui(mac));
}
