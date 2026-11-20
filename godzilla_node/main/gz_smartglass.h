/*
 * gz_smartglass.h — passive recording-smart-glasses classifier by Bluetooth SIG
 * company identifier (detection §4, unit 5). Pure: builds on gz_ble_adv.h's AD
 * parser; stdint only, no Arduino/LVGL/NimBLE. Native-tested in
 * test/test_godzilla_smartglass.
 *
 * The manufacturer-specific AD structure (type 0xFF) begins with the 16-bit
 * Bluetooth SIG company identifier; gz_ble_adv.h already extracts it as
 * gz_adv_t.company. A recording smart-glass device advertises under its maker's
 * SIG company ID, so the ID is the passive signal — never a local name (name-
 * only matching produces false positives, the lesson src/sigdb_surveillance.h
 * learned and gz_skimmer.h/gz_camera.h both honour).
 *
 * Confidence (every signature is HIGH or LOW):
 *   - HIGH: a company ID assigned to a DEDICATED smart-glasses / AR maker whose
 *     entire product line is camera-equipped head-worn wearables, so the ID
 *     alone is a strong signal the contact is a recording smart glass.
 *   - LOW ("?"): a company ID of a maker that ships recording smart glasses but
 *     ALSO many non-glasses BLE products (phones, VR headsets, app beacons), so
 *     the ID cannot by itself tell the glasses from the rest. LOW needs
 *     corroboration and never raises the §4 alert overlay on its own — the node
 *     scan hook promotes only HIGH onto the wire.
 *   - NONE: no manufacturer data, a malformed advert, or an unlisted company.
 *
 * Sources (primary): the Bluetooth SIG Assigned Numbers "Company Identifiers"
 * list (https://www.bluetooth.com/specifications/assigned-numbers/), the
 * authoritative registry mapping each 16-bit company ID to the exact member
 * company that registered it. Only one signature ships here, verified against
 * the SIG "Company Identifiers by Name" section (read verbatim, twice):
 *   - 0x060C "Vuzix Corporation"  — dedicated AR/smart-glasses maker   (HIGH)
 *
 * LOW family DEFERRED (no entries): the exact SIG company-ID values for the
 * conglomerate makers of recording smart glasses (Meta / Ray-Ban Meta, Snap /
 * Spectacles) could not be pinned to a single value I trust — independent
 * verbatim reads of the SIG list disagreed on the hex (e.g. Snapchat Inc read
 * as both 0x03C2 and 0x07DF). Rather than ship an unverified registrant, the
 * LOW list is empty until Dale/Claudius confirm the values from the
 * authoritative source. A LOW entry has no runtime effect regardless — the node
 * scan hook promotes only HIGH onto the wire — so deferring it costs nothing.
 *
 * Registrant corrections confirmed for the next verifier (SIG by-name list,
 * 2-of-2 verbatim reads; these supersede an earlier #149 review note):
 *   - 0x01AB = "Meta Watch Ltd." (a defunct Fossil smartwatch brand) — NOT
 *     "Facebook, Inc."; there is NO "Facebook" entry in the list at all. A
 *     name collision on the word "Meta"; never list 0x01AB here.
 *   - "Meso international" = 0x0657, NOT 0x00B6.
 *   - 0x00B6 = "Meta Platforms Technologies, LLC"; 0x058E = "Meta Platforms,
 *     Inc." (both real Meta entities, but the LOW family is deferred as above).
 *
 * Deliberately NOT listed, to avoid misattribution:
 *   - Amazon Echo Frames (0x0171 "Amazon.com Services LLC") — audio-only
 *     eyewear with no camera, so not a recording device.
 *   - Rokid, Xreal (formerly Nreal) and Even Realities — no Bluetooth SIG
 *     company-identifier assignment at the list's version, so not matchable by
 *     company ID; added only once a registrant is verified.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_ble_adv.h"   /* gz_adv_t + gz_adv_parse (the AD-structure parser) */

/* Confidence of a §4 signature. LOW shows with a "?" and never raises the alert
 * overlay by itself; HIGH is an exact assigned-value match. */
enum { GZ_SMARTGLASS_NONE = 0, GZ_SMARTGLASS_LOW = 1, GZ_SMARTGLASS_HIGH = 2 };

/* SIG company IDs of dedicated smart-glasses / AR makers (whole product line is
 * recording head-worn wearables). HIGH. */
static const uint16_t gz_smartglass_cid_high[] = {
    0x060C,   /* Vuzix Corporation */
};

/* SIG company IDs of makers that ship recording smart glasses alongside many
 * non-glasses products (LOW). DEFERRED — empty until a value is verified (see
 * header). Sentinel element keeps the array valid C; the count stays 0. */
static const uint16_t gz_smartglass_cid_low[] = { 0 };
#define GZ_SMARTGLASS_CID_LOW_N 0

/* Confidence for a bare 16-bit company identifier. */
static inline int gz_smartglass_cid_conf(uint16_t cid)
{
    for (unsigned i = 0; i < sizeof gz_smartglass_cid_high / sizeof gz_smartglass_cid_high[0]; ++i)
        if (gz_smartglass_cid_high[i] == cid) return GZ_SMARTGLASS_HIGH;
    for (int i = 0; i < GZ_SMARTGLASS_CID_LOW_N; ++i)
        if (gz_smartglass_cid_low[i] == cid) return GZ_SMARTGLASS_LOW;
    return GZ_SMARTGLASS_NONE;
}

/* Classify a parsed advert by its manufacturer company ID. An advert with no
 * manufacturer-specific data, or a malformed one, is never flagged. */
static inline int gz_smartglass_classify(const gz_adv_t *a)
{
    if (a->malformed) return GZ_SMARTGLASS_NONE;
    if (!a->has_mfr)  return GZ_SMARTGLASS_NONE;   /* company ID is the only signal */
    return gz_smartglass_cid_conf(a->company);
}

/* Parse a raw advertising payload and classify it (the live-path entry point). */
static inline int gz_smartglass_classify_payload(const uint8_t *payload, int len)
{
    gz_adv_t a;
    gz_adv_parse(payload, len, &a);
    return gz_smartglass_classify(&a);
}
