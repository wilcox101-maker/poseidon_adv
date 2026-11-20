/*
 * gz_alerts_page.h — pure model for the Alerts settings subpage (watch-faces
 * customiser, unit 4; detection §4 item 6). PURE.
 *
 * The customiser's single "Alerts" row opens a subpage that toggles the
 * per-family alert-enable bits held in gz_ui_settings_t.alert_mask. This header
 * is the host-tested core the firmware page draws on:
 *   - the row <-> enable-bit map for the seven alerting families, in the order
 *     the subpage lists them, then a Back row;
 *   - toggling one row's bit in the mask;
 *   - the "n/7 on" count shown on the customiser's Alerts row;
 *   - per-row on/off and ASCII row labels.
 *
 * ONE source of the bit meanings: the GZ_ALERT_BIT_* values come from
 * gz_alert.h, never redefined here, so the subpage can never drift from the
 * gate that honours the mask. LE_EQUIP and ALPR are LOW-only families with no
 * enable bit (gz_alert.h), so they have no row here and never appear — the
 * subpage's footer says so ("LE gear / ALPR: radar only").
 *
 * Pure: stdint/bool only; compiles as C and C++. The page drawing itself is
 * firmware (gz_face_customiser.cpp). Native-tested in test_godzilla_alerts_page.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_alert.h"   /* GZ_ALERT_BIT_* / GZ_ALERT_MASK_ALL — the one source */

/* The subpage rows: seven alerting families in display order, then Back.
 * GZ_ALERTS_FAMILY_COUNT is the toggle-row count (also the "n/N on" total);
 * GZ_ALERTS_ROW_COUNT includes the Back row. */
enum {
    GZ_ALERTS_ROW_FLOCK = 0,
    GZ_ALERTS_ROW_RAVEN,
    GZ_ALERTS_ROW_REMOTEID,
    GZ_ALERTS_ROW_SKIMMER,
    GZ_ALERTS_ROW_CAMERA,
    GZ_ALERTS_ROW_SMARTGLASS,
    GZ_ALERTS_ROW_TRACKER,
    GZ_ALERTS_FAMILY_COUNT,              /* 7 toggle rows */
    GZ_ALERTS_ROW_BACK = GZ_ALERTS_FAMILY_COUNT,
    GZ_ALERTS_ROW_COUNT                  /* 8 rows incl. Back */
};

/* The "n/N on" denominator shown on the customiser's Alerts row. */
#define GZ_ALERTS_TOTAL GZ_ALERTS_FAMILY_COUNT

/* The enable bit a row governs, or 0 for Back / out of range. Mirrors
 * gz_alert_family_bit() but keyed by subpage row rather than radar family. */
static inline uint8_t gz_alerts_row_bit(int row)
{
    switch (row) {
    case GZ_ALERTS_ROW_FLOCK:      return (uint8_t)GZ_ALERT_BIT_FLOCK;
    case GZ_ALERTS_ROW_RAVEN:      return (uint8_t)GZ_ALERT_BIT_RAVEN;
    case GZ_ALERTS_ROW_REMOTEID:   return (uint8_t)GZ_ALERT_BIT_REMOTEID;
    case GZ_ALERTS_ROW_SKIMMER:    return (uint8_t)GZ_ALERT_BIT_SKIMMER;
    case GZ_ALERTS_ROW_CAMERA:     return (uint8_t)GZ_ALERT_BIT_CAMERA;
    case GZ_ALERTS_ROW_SMARTGLASS: return (uint8_t)GZ_ALERT_BIT_SMARTGLASS;
    case GZ_ALERTS_ROW_TRACKER:    return (uint8_t)GZ_ALERT_BIT_TRACKER;
    default:                       return 0u;   /* Back / out of range */
    }
}

/* A togglable family row (not Back, in range). */
static inline bool gz_alerts_row_is_family(int row)
{
    return row >= 0 && row < GZ_ALERTS_FAMILY_COUNT;
}

/* Is this family's alert currently enabled in `mask`? False for a row with no
 * bit (Back / out of range). */
static inline bool gz_alerts_row_on(uint8_t mask, int row)
{
    uint8_t bit = gz_alerts_row_bit(row);
    return bit != 0u && (mask & bit) != 0u;
}

/* Flip one row's enable bit in `mask` and return the new mask. A row with no
 * bit (Back) leaves the mask unchanged. */
static inline uint8_t gz_alerts_toggle(uint8_t mask, int row)
{
    return (uint8_t)(mask ^ gz_alerts_row_bit(row));
}

/* How many of the seven alerting families are enabled in `mask` (the numerator
 * of "n/7 on"). Only the defined family bits are counted; any stray high bit is
 * ignored. */
static inline int gz_alerts_count_on(uint8_t mask)
{
    int n = 0;
    for (int r = 0; r < GZ_ALERTS_FAMILY_COUNT; ++r)
        if (gz_alerts_row_on(mask, r)) ++n;
    return n;
}

/* The on-screen label for a row. ASCII only (the node's LVGL build carries just
 * the built-in Montserrat fonts — any non-ASCII renders as a box, #147/#150). */
static inline const char *gz_alerts_row_name(int row)
{
    switch (row) {
    case GZ_ALERTS_ROW_FLOCK:      return "Flock";
    case GZ_ALERTS_ROW_RAVEN:      return "Raven";
    case GZ_ALERTS_ROW_REMOTEID:   return "Remote ID";
    case GZ_ALERTS_ROW_SKIMMER:    return "Skimmer";
    case GZ_ALERTS_ROW_CAMERA:     return "Camera";
    case GZ_ALERTS_ROW_SMARTGLASS: return "Glasses";
    case GZ_ALERTS_ROW_TRACKER:    return "Trackers";
    case GZ_ALERTS_ROW_BACK:       return "Back";
    default:                       return "?";
    }
}
