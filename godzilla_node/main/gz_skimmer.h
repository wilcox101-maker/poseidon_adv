/*
 * gz_skimmer.h — passive Bluetooth card-skimmer classifier (detection §4,
 * unit 1). Pure: builds on gz_ble_adv.h's AD parser; stdint/string only, no
 * Arduino/LVGL/NimBLE. Native-tested in test/test_godzilla_skimmer.
 *
 * What this catches: cheap BLE "transparent UART" serial-bridge modules — the
 * CC2541-class HM-10 family and its clones — which are the radios most commonly
 * soldered into Bluetooth card skimmers to siphon track data. They all expose a
 * vendor serial service with the 16-bit UUID 0xFFE0 (notify/write characteristic
 * 0xFFE1) and ship with a fixed default advertised local name. Signals:
 *   - HIGH: a known default module name AND the 0xFFE0 serial service both
 *     present (name + UUID agree) — a stock serial module advertising as itself;
 *   - LOW ("?"): the 0xFFE0 serial service with no known default name — a
 *     generic BLE-UART device (could be a legitimate hobby project), so it needs
 *     corroboration and never raises the alert overlay on its own;
 *   - NONE: no 0xFFE0 service. A known name WITHOUT the service is NOT flagged —
 *     name-only matching produces false positives and is deliberately avoided
 *     (the same lesson src/sigdb_surveillance.h learned dropping SSID patterns).
 *
 * Hardware limit — out of scope: HC-05, HC-06 and RN42 are Bluetooth *Classic*
 * (BR/EDR SPP), not BLE. Seeing a Classic device at all requires an active
 * inquiry (which transmits), and the Cardputer/S3 has no Classic radio, so these
 * classic skimmer modules cannot be detected passively and are not matched here.
 *
 * Sources (primary): the vendor module datasheets, which document the 0xFFE0 /
 * 0xFFE1 serial service and the default local name — DSD TECH / JNHuaMao HM-10
 * (CC2541) datasheet (service FFE0, characteristic FFE1, AT+NAME default
 * "HMSoft"); the HC-08 BLE UART module user guide (FFE0/FFE1 "transparent
 * transmission", default name "HC-08"); and the HM-10-clone modules BT04-A,
 * MLT-BT05 and JDY-08, which expose the same FFE0 service under their own default
 * names. 0xFFE0/0xFFE1 is a vendor service these modules define, NOT a Bluetooth
 * SIG assigned number — so it is a LOW signal on its own and only HIGH once a
 * known default name agrees with it.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "gz_ble_adv.h"   /* gz_adv_t + gz_adv_parse (the AD-structure parser) */

/* Confidence of a §4 signature. LOW shows with a "?" and never raises the alert
 * overlay by itself; HIGH is an exact, corroborated match. */
enum { GZ_SKIMMER_NONE = 0, GZ_SKIMMER_LOW = 1, GZ_SKIMMER_HIGH = 2 };

/* The vendor serial service the HM-10 family and its clones expose. */
#define GZ_SKIMMER_SVC_UUID 0xFFE0u

/* Default advertised local names of the common BLE-UART serial modules, as
 * documented in their datasheets. Exact match only (these are fixed defaults);
 * a skimmer that renamed its module falls to LOW on the service alone. */
static inline bool gz_skimmer_known_name(const char *name)
{
    static const char *const defaults[] = {
        "HMSoft",    /* HM-10 / HM-11 (JNHuaMao / DSD TECH), AT+NAME default */
        "HC-08",     /* HC-08 BLE UART module default                        */
        "BT04-A",    /* HM-10-clone serial module default                    */
        "MLT-BT05",  /* HM-10-clone serial module default                    */
        "JDY-08",    /* JDY-08 BLE UART module default                       */
    };
    for (unsigned i = 0; i < sizeof defaults / sizeof defaults[0]; ++i)
        if (strcmp(name, defaults[i]) == 0) return true;
    return false;
}

/* True iff the advert lists the 0xFFE0 vendor serial service (in its 16-bit
 * UUID list or as service data). */
static inline bool gz_skimmer_has_serial_svc(const gz_adv_t *a)
{
    for (int i = 0; i < a->n_uuid16; ++i)
        if (a->uuid16[i] == GZ_SKIMMER_SVC_UUID) return true;
    return a->has_svc_data && a->svc_data_uuid == GZ_SKIMMER_SVC_UUID;
}

/* Classify a parsed advert. GZ_SKIMMER_HIGH when a known default module name and
 * the 0xFFE0 serial service agree; GZ_SKIMMER_LOW for the service alone;
 * GZ_SKIMMER_NONE otherwise (no service, or a malformed advert). */
static inline int gz_skimmer_classify(const gz_adv_t *a)
{
    if (a->malformed) return GZ_SKIMMER_NONE;
    if (!gz_skimmer_has_serial_svc(a)) return GZ_SKIMMER_NONE;   /* name-only never flags */
    if (a->has_name && gz_skimmer_known_name(a->name)) return GZ_SKIMMER_HIGH;
    return GZ_SKIMMER_LOW;
}

/* Parse a raw advertising payload and classify it (the live-path entry point). */
static inline int gz_skimmer_classify_payload(const uint8_t *payload, int len)
{
    gz_adv_t a;
    gz_adv_parse(payload, len, &a);
    return gz_skimmer_classify(&a);
}
