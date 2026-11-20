/*
 * gz_alert.h — priority ALERT overlay gating (detection §4, unit 6). PURE.
 *
 * Decides, from the node-local radar table (gz_radar.h), whether a detection
 * deserves a full-screen priority alert and which one wins when several do. The
 * overlay itself (gz_ui_alert.*) is firmware; this header is the host-tested
 * decision core and carries no LVGL/Arduino.
 *
 * Confidence without a wire change: the node assigns a surveillance / recording
 * family ONLY on a confident classify — gz_ble_scan / gz_wifi_scan set SKIMMER
 * and CAMERA only on a *_HIGH match, SMARTGLASS only on an exact assigned value,
 * while LE_EQUIP and ALPR are LOW families by definition (gz_le_equip.h /
 * gz_alpr.h). So the family a radar row carries already encodes its confidence;
 * the gate reads family + hit count only and the v5 wire entry (flags reserved)
 * is untouched.
 *
 * Rules (orders, detection §4 item 6):
 *   - HIGH surveillance / recording families raise at once: Flock, Raven,
 *     Remote ID, card skimmer, covert camera, recording smart glasses.
 *   - A LOW-confidence family NEVER raises on its own — LE_EQUIP and ALPR are
 *     LOW by definition and need corroboration the node cannot supply, so they
 *     never raise. (native-tested)
 *   - Trackers (Find My / AirTag / Tile / Samsung SmartTag) raise only when seen
 *     repeatedly over time: hits >= GZ_ALERT_TRACKER_HITS.
 *   - Each alerting family (trackers as one group) has an enable bit in the
 *     persisted alert mask; a cleared bit suppresses it.
 *   - Per device the overlay owner re-alerts no sooner than GZ_ALERT_RELIMIT_MS
 *     (gz_alert_rate_ok) and auto-clears after GZ_ALERT_TTL_MS.
 *
 * Pure: stdint/bool only; compiles as C and C++. Native-tested in
 * test/test_godzilla_alert.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "gz_radar.h"   /* gz_radar_row_t, POSEI_DET_FAM_*, severity, live test */

/* A tracker must be published at least this many times before it is treated as
 * "following" and allowed to raise the overlay (orders: seen repeatedly over
 * time). Surveillance / recording families raise on the first sighting. */
#define GZ_ALERT_TRACKER_HITS 6u

/* Per-device re-alert cool-down and the overlay's own auto-clear, both 60 s+
 * so a single device cannot pin the screen (orders: rate-limited per device,
 * auto-clear after 60 s). */
#define GZ_ALERT_RELIMIT_MS 120000u
#define GZ_ALERT_TTL_MS      60000u

/* One enable bit per alerting family; the four trackers share one bit (they
 * share the repeat rule and the amber tier). Stored as gz_ui_settings_t.alert_mask.
 * Append-only: a new alerting family takes the next free bit. */
enum {
    GZ_ALERT_BIT_FLOCK      = 1u << 0,
    GZ_ALERT_BIT_RAVEN      = 1u << 1,
    GZ_ALERT_BIT_REMOTEID   = 1u << 2,
    GZ_ALERT_BIT_SKIMMER    = 1u << 3,
    GZ_ALERT_BIT_CAMERA     = 1u << 4,
    GZ_ALERT_BIT_SMARTGLASS = 1u << 5,
    GZ_ALERT_BIT_TRACKER    = 1u << 6,   /* Find My / AirTag / Tile / SmartTag */
};
#define GZ_ALERT_MASK_ALL 0x7Fu          /* every alerting family enabled (default) */

/* The enable bit that governs `family`, or 0 if the family never raises an
 * alert. LE_EQUIP and ALPR (LOW-only) and every neutral family return 0, so
 * they can never raise the overlay whatever the mask says. */
static inline uint16_t gz_alert_family_bit(uint8_t family)
{
    switch (family) {
    case POSEI_DET_FAM_FLOCK:      return GZ_ALERT_BIT_FLOCK;
    case POSEI_DET_FAM_RAVEN:      return GZ_ALERT_BIT_RAVEN;
    case POSEI_DET_FAM_REMOTEID:   return GZ_ALERT_BIT_REMOTEID;
    case POSEI_DET_FAM_SKIMMER:    return GZ_ALERT_BIT_SKIMMER;
    case POSEI_DET_FAM_CAMERA:     return GZ_ALERT_BIT_CAMERA;
    case POSEI_DET_FAM_SMARTGLASS: return GZ_ALERT_BIT_SMARTGLASS;
    case POSEI_DET_FAM_FINDMY:
    case POSEI_DET_FAM_AIRTAG:
    case POSEI_DET_FAM_TILE:
    case POSEI_DET_FAM_SMARTTAG:   return GZ_ALERT_BIT_TRACKER;
    default:                       return 0u;   /* incl. LE_EQUIP, ALPR: never alone */
    }
}

/* A repeat-gated tracker family (raises only once seen repeatedly). */
static inline bool gz_alert_is_tracker(uint8_t family)
{
    return gz_alert_family_bit(family) == GZ_ALERT_BIT_TRACKER;
}

/* Would this detection, on its own, raise the overlay under `mask`?
 *   - not an alerting family (0 bit, incl. LE_EQUIP / ALPR)  -> never;
 *   - its bit cleared in the mask                            -> suppressed;
 *   - a tracker                                              -> only at hits >= threshold;
 *   - a HIGH surveillance / recording family                 -> yes.
 * Pure in (family, hits, mask); rate-limit and TTL are the owner's. */
static inline bool gz_alert_row_eligible(uint8_t family, uint16_t hits, uint16_t mask)
{
    uint16_t bit = gz_alert_family_bit(family);
    if (bit == 0u) return false;
    if ((mask & bit) == 0u) return false;
    if (gz_alert_is_tracker(family)) return hits >= GZ_ALERT_TRACKER_HITS;
    return true;
}

/* Rank an eligible row so the selector can pick the most important: surveillance
 * (red) over tracker (amber), then stronger RSSI, then more hits. Higher wins. */
static inline long gz_alert_rank(const gz_radar_row_t *r)
{
    long sev  = gz_radar_severity(r->family);        /* 2 surveil > 1 tracker */
    long near = (long)r->rssi + 128;                 /* -negative dBm -> 0..  */
    return (sev << 24) | ((near & 0xFF) << 16) | (long)(r->hits & 0xFFFF);
}

/* Pick the highest-priority radar row that is alert-eligible under `mask` AND
 * still live (seen within the radar TTL), or -1 if none. Pure over a snapshot;
 * the owner applies the per-device rate-limit and the overlay TTL on top. */
static inline int gz_alert_select(const gz_radar_row_t *rows, int n,
                                  uint16_t mask, uint32_t now_ms)
{
    int best = -1;
    long best_rank = -1;
    for (int i = 0; i < n; ++i) {
        const gz_radar_row_t *r = &rows[i];
        if (!gz_radar_live(r, now_ms)) continue;
        if (!gz_alert_row_eligible(r->family, r->hits, mask)) continue;
        long rk = gz_alert_rank(r);
        if (rk > best_rank) { best_rank = rk; best = i; }
    }
    return best;
}

/* Per-device re-alert gate: true if this device may alert now — it has never
 * alerted (ever=false) or the cool-down has elapsed. Unsigned subtraction, so
 * millis() wrap is safe. */
static inline bool gz_alert_rate_ok(uint32_t last_alert_ms, uint32_t now_ms, bool ever)
{
    return !ever || (uint32_t)(now_ms - last_alert_ms) >= GZ_ALERT_RELIMIT_MS;
}

/* True once an open overlay has outlived its 60 s auto-clear window. */
static inline bool gz_alert_expired(uint32_t raised_ms, uint32_t now_ms)
{
    return (uint32_t)(now_ms - raised_ms) >= GZ_ALERT_TTL_MS;
}
