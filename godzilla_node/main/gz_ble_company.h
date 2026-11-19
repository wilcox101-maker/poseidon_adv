/*
 * gz_ble_company.h — BLE SIG company-ID -> vendor naming for the BLE Lab
 * (detection-breadth orders, 2026-11-19, §1 item 2). Pure: stdint/string
 * only, host-tested by test/test_godzilla_ble_company.
 *
 * The manufacturer-specific AD structure (type 0xFF) begins with a 16-bit
 * company identifier assigned by the Bluetooth SIG (Assigned Numbers, as
 * mirrored in Zephyr's bluetooth/company_id and Nordic's SDK). gz_ble_adv.h
 * already extracts that ID (gz_adv_t.company); this file turns it into a
 * vendor name and composes the "vendor — <type>" label the BLE Lab detail
 * shows. Decode-only: naming the maker of a passively heard advert, nothing
 * that connects.
 *
 * Only long-established, well-known IDs are named; anything else is shown as
 * its hex value ("mfr XXXX"), never guessed. This supersedes the short table
 * in gz_ble_adv.h (gz_adv_company_name) while staying consistent with it.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Vendor name for a BLE SIG 16-bit company identifier; NULL if not in the
 * table. Values are the canonical SIG assignments. */
static inline const char *gz_ble_company_name(uint16_t c)
{
    switch (c) {
    case 0x0001: return "Nokia";
    case 0x0002: return "Intel";
    case 0x0003: return "IBM";
    case 0x0006: return "Microsoft";
    case 0x000D: return "Texas Instruments";
    case 0x000F: return "Broadcom";
    case 0x004C: return "Apple";
    case 0x0059: return "Nordic";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    default:     return NULL;
    }
}

/* True when the company ID is one we can name. */
static inline bool gz_ble_company_known(uint16_t c)
{
    return gz_ble_company_name(c) != NULL;
}

/* Compose the BLE Lab detail label into out[n]:
 *   known vendor + type  -> "Apple — keyboard"
 *   known vendor, no type -> "Apple"
 *   unknown vendor + type -> "mfr 1234 — keyboard"
 *   unknown vendor, no type -> "mfr 1234"
 * `type` may be NULL or empty to omit the suffix. Returns snprintf length.
 * The separator is an ASCII " - " so it renders on every panel font. */
static inline int gz_ble_vendor_label(uint16_t company, const char *type,
                                       char *out, size_t n)
{
    const char *v = gz_ble_company_name(company);
    char hex[12];
    if (!v) { snprintf(hex, sizeof hex, "mfr %04X", (unsigned)company); v = hex; }
    if (type && type[0]) return snprintf(out, n, "%s - %s", v, type);
    return snprintf(out, n, "%s", v);
}
