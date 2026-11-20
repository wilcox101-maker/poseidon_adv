/*
 * gz_camera.h — passive covert / IP-camera classifier by Wi-Fi OUI (detection
 * §4, unit 2). Pure: stdint only, host-tested by test/test_godzilla_camera.
 *
 * From a management frame's source address the node already sniffed (no new
 * sniff mode, no TX), tag a contact whose OUI (first 3 octets) belongs to a
 * camera / video-surveillance manufacturer as CAMERA. The classification is by
 * the IEEE-assigned OUI only — never an SSID or a name (an SSID match produced
 * false positives; src/sigdb_surveillance.h dropped that approach).
 *
 * Confidence:
 *   - HIGH: an OUI assigned to a DEDICATED camera / surveillance maker — a
 *     company whose IEEE registration ships cameras, so the OUI is a strong
 *     signal. The table below (Hikvision, Zhejiang Dahua, Amcrest, Hangzhou
 *     Ezviz) are such makers.
 *   - LOW ("?"): an OUI of a parent company that ALSO ships many non-camera
 *     products (e.g. a conglomerate whose OUI covers both a doorbell camera and
 *     unrelated gear), so the OUI alone is weak. None are whitelisted yet —
 *     makers like Ring (Amazon), Eufy (Anker), Wyze and Arlo (Netgear) need
 *     per-OUI evidence that a given block is camera-only before they can be
 *     listed HIGH, or marked LOW where the block is shared; added when verified.
 *
 * A randomized / locally-administered MAC is never attributed to a vendor (its
 * OUI is not a real assignment); gz_camera_classify_mac returns NONE for it.
 *
 * Sources (primary): the IEEE MA-L OUI registry
 * (https://standards-oui.ieee.org/), the authoritative assignment of each
 * 24-bit prefix to its company. The prefixes below were compiled from that
 * registry (cross-checked via the Netify / maclookup OUI aggregators) under the
 * official registrant names: "Hangzhou Hikvision Digital Technology Co., Ltd.",
 * "Zhejiang Dahua Technology Co., Ltd.", "Amcrest Technologies", and "Hangzhou
 * Ezviz Software Co., Ltd." (a Hikvision consumer-camera brand). Amcrest also
 * holds an MA-M block (34:46:63:2…) whose 24-bit prefix 34:46:63 is shared with
 * other MA-M registrants, so it is deliberately NOT listed — matching it on the
 * 24-bit OUI alone would misattribute other companies' devices.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_wifi_mac.h"   /* gz_mac_oui / gz_mac_is_local (shared helpers) */

enum { GZ_CAMERA_NONE = 0, GZ_CAMERA_LOW = 1, GZ_CAMERA_HIGH = 2 };

/* 24-bit OUIs of dedicated camera / surveillance makers (IEEE MA-L). HIGH. */
static const uint32_t gz_camera_oui_high[] = {
    /* Hangzhou Hikvision Digital Technology Co., Ltd. */
    0xE4D58B, 0xC8A702, 0x085411, 0x08A189, 0x08CC81, 0x240F9B, 0x2432AE,
    0x244845, 0x2CA59C, 0x40ACBF, 0x4419B6, 0x44A642, 0x4C62DF, 0x4CBD8F,
    0x4CF5DC, 0x50E538, 0x5803FB, 0x5850ED, 0x5C345B, 0x64DB8B, 0x686DBC,
    0x80F5AE, 0x8CE748, 0x94E1AC, 0x989DE5, 0x98DF82, 0x98F112, 0xA0FF0C,
    0xA41437, 0xA4D5C2, 0xACCB51, 0xB4A382, 0xBC5E33, 0xBCAD28, 0xBCBAC2,
    0xC0517E, 0xC056E3, 0xC06DED, 0xDC07F8, 0x1012FB, 0xE0BAAD, 0xE0CA3C,
    0xE0DF13, 0xECA971, 0xA42902, 0xA4A459, 0x80489F, 0xE8A0ED, 0xFC9FFD,
    0x040312, 0x0C75D2, 0x1868CB, 0x188025, 0x2428FD, 0x2857BE, 0x340962,
    0x3C1BF8, 0x4447CC, 0x548C81, 0x54C415, 0x743FC2, 0x807C62, 0x80BEAF,
    0x849A40, 0x988B0A, 0xACB92F, 0xBC9B5E, 0xC42F90, 0xD4E853, 0xDCD26A,
    0xECC89C, 0xF84DFC, 0x4C1F86, 0xA44BD9, 0x88DE39, 0x849459, 0x083BC1,
    0x00BC99, 0x04EECD, 0x8C22D2, 0x48785B, 0xCC13F3, 0x40B570, 0xB0FF0D,
    /* Zhejiang Dahua Technology Co., Ltd. */
    0x08EDED, 0x14A78B, 0x38AF29, 0x3CE36B, 0x3CEF8C, 0x5CF51A, 0x64FD29,
    0x74C929, 0x8CE9B4, 0x9C1463, 0xA0BD1D, 0xBC325F, 0xC0395A, 0xC4AAC4,
    0xD4430E, 0xE0508B, 0xE4246C, 0xF8CE07, 0x30DDAA, 0xF4B1C2, 0xFCB69D,
    0x24526A, 0x4C11BF, 0x6C1C71, 0x9002A9, 0x98F9CC, 0xB44C3B, 0xE02EFE,
    0xFC5F49, 0x4C99E8, 0x407AA4,
    /* Amcrest Technologies (MA-L blocks only; the MA-M 34:46:63 is excluded). */
    0x00651E, 0x9C8ECD, 0xA06032,
    /* Hangzhou Ezviz Software Co., Ltd. (Hikvision consumer-camera brand). */
    0x94EC13, 0x20BBBC, 0x588FCF, 0x64F2FB, 0x78C1AE, 0x34C6DD, 0xEC97E0,
    0x54D60D, 0x78A6A0, 0x0CA64C, 0xAC1C26, 0x64244D, 0xF47018, 0xFC2422,
    0x38F25D,
};

/* Shared parent-company OUIs → LOW. None verified yet (see header). */
static const uint32_t gz_camera_oui_low[] = { 0 };   /* sentinel; count is 0 */
#define GZ_CAMERA_OUI_LOW_N 0

/* Confidence for a bare 24-bit OUI: HIGH if a dedicated maker, LOW if a shared
 * parent, NONE otherwise. */
static inline int gz_camera_oui_conf(uint32_t oui)
{
    for (unsigned i = 0; i < sizeof gz_camera_oui_high / sizeof gz_camera_oui_high[0]; ++i)
        if (gz_camera_oui_high[i] == oui) return GZ_CAMERA_HIGH;
    for (int i = 0; i < GZ_CAMERA_OUI_LOW_N; ++i)
        if (gz_camera_oui_low[i] == oui) return GZ_CAMERA_LOW;
    return GZ_CAMERA_NONE;
}

/* Classify a station/AP MAC. A randomized (locally-administered) address is not
 * attributed to any vendor. */
static inline int gz_camera_classify_mac(const uint8_t mac[6])
{
    if (gz_mac_is_local(mac)) return GZ_CAMERA_NONE;
    return gz_camera_oui_conf(gz_mac_oui(mac));
}
