/*
 * gz_radar.h — node-local detection table + radar geometry (P2b).
 *
 * The radar screen shows the node's OWN recent detections. This is a small
 * local table in the node tree, following the pattern of src/godzilla_det.h
 * (which stays read-only): one row per (kind, address), latest RSSI, sticky
 * specific family, hit count, last-seen time, least-recently-seen eviction.
 *
 * Feed: every published batch (gz_ble_scan / gz_wifi_scan publish ticks) is
 * noted here, so the radar shows exactly what went on the wire — no second
 * capture path, nothing in the promiscuous/BLE callbacks.
 *
 * Geometry (all pure, native-tested):
 *   - radius from RSSI: strong = near the centre, weak = near the rim,
 *     clamped to [GZ_RADAR_RSSI_WEAK, GZ_RADAR_RSSI_STRONG];
 *   - angle from an FNV-1a + fmix32 hash of (kind, address): stable per device across
 *     refreshes and reboots, so a dot never jumps around the dial;
 *   - severity from family, mirroring the Cardputer GODZILLA view (#54,
 *     src/features/c5_godzilla.cpp fam_colour): Flock/Raven/Remote ID red,
 *     Find My/AirTag/Tile/SmartTag amber, everything else neutral;
 *   - hit test: nearest live dot within a finger-sized radius.
 *
 * Pure: stdint/string/math only, no Arduino/LVGL. Compiles as C and C++.
 * The spinlocked store shared by the loop task and the LVGL task is in
 * gz_radar_store.{h,cpp}.
 *
 * Passive: bookkeeping of what the receive-only scanners already saw.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "../../c5_node/main/proto.h"   /* POSEI_DET_FAM_* */

#define GZ_RADAR_MAX          48       /* rows kept (48 x 24 B)            */
#define GZ_RADAR_TTL_MS       60000u   /* a dot leaves the dial after 60 s */
#define GZ_RADAR_FADE_MS      30000u   /* ...and dims after 30 s           */
#define GZ_RADAR_RSSI_STRONG  (-35)    /* at/above: innermost radius       */
#define GZ_RADAR_RSSI_WEAK    (-95)    /* at/below: outer rim              */

enum { GZ_RADAR_WIFI = 0, GZ_RADAR_BLE = 1 };

/* Severity tiers; the UI maps them to red / amber / neutral. */
enum { GZ_SEV_NEUTRAL = 0, GZ_SEV_TRACKER = 1, GZ_SEV_SURVEIL = 2 };

typedef struct {
    uint8_t  addr[6];
    uint8_t  kind;          /* GZ_RADAR_WIFI / GZ_RADAR_BLE            */
    uint8_t  aux;           /* Wi-Fi: channel · BLE: addr_type         */
    int8_t   rssi;          /* latest                                  */
    uint8_t  family;        /* POSEI_DET_FAM_*, sticky once specific   */
    uint16_t hits;          /* published sightings, saturating         */
    uint32_t last_seen_ms;
    uint32_t log_ms;        /* Recon: when this row was last logged    */
    bool     logged;        /* Recon: logged in the current session    */
} gz_radar_row_t;

typedef struct {
    gz_radar_row_t e[GZ_RADAR_MAX];
    int            n;
} gz_radar_table_t;

/* ---- table --------------------------------------------------------------- */

static inline void gz_radar_clear(gz_radar_table_t *t) { t->n = 0; }

static inline int gz_radar_find(const gz_radar_table_t *t, uint8_t kind,
                                const uint8_t addr[6])
{
    for (int i = 0; i < t->n; ++i)
        if (t->e[i].kind == kind && memcmp(t->e[i].addr, addr, 6) == 0) return i;
    return -1;
}

static inline bool gz_radar_family_specific(uint8_t f)
{
    return f != POSEI_DET_FAM_UNKNOWN && f != POSEI_DET_FAM_BLE_GENERIC &&
           f != POSEI_DET_FAM_WIFI_AP && f != POSEI_DET_FAM_WIFI_STA;
}

/* Note one published sighting. Returns the row index. All-zero addresses are
 * ignored (returns -1). When full, the least recently seen row is replaced
 * (unsigned age, so millis() wrap is safe). */
static inline int gz_radar_note(gz_radar_table_t *t, uint8_t kind,
                                const uint8_t addr[6], uint8_t aux,
                                int8_t rssi, uint8_t family, uint32_t now_ms)
{
    static const uint8_t zero[6] = { 0, 0, 0, 0, 0, 0 };
    if (memcmp(addr, zero, 6) == 0) return -1;

    int i = gz_radar_find(t, kind, addr);
    if (i < 0) {
        if (t->n < GZ_RADAR_MAX) {
            i = t->n++;
        } else {
            i = 0;
            uint32_t oldest = now_ms - t->e[0].last_seen_ms;
            for (int j = 1; j < t->n; ++j) {
                uint32_t age = now_ms - t->e[j].last_seen_ms;
                if (age > oldest) { oldest = age; i = j; }
            }
        }
        memcpy(t->e[i].addr, addr, 6);
        t->e[i].kind   = kind;
        t->e[i].family = family;
        t->e[i].hits   = 0;
        t->e[i].logged = false;
        t->e[i].log_ms = 0;
    }
    gz_radar_row_t *r = &t->e[i];
    r->aux  = aux;
    r->rssi = rssi;
    if (gz_radar_family_specific(family) || !gz_radar_family_specific(r->family))
        r->family = family;
    r->last_seen_ms = now_ms;
    if (r->hits < 0xFFFF) r->hits++;
    return i;
}

/* Note every entry of a published batch (shared 10-byte entry layout:
 * addr[6], aux, rssi, family, flags). */
static inline void gz_radar_note_batch(gz_radar_table_t *t, uint8_t kind,
                                       const uint8_t *entries, uint8_t count,
                                       uint32_t now_ms)
{
    if (count > POSEI_DET_MAX) count = POSEI_DET_MAX;
    for (uint8_t k = 0; k < count; ++k) {
        const uint8_t *p = entries + (size_t)k * 10u;
        gz_radar_note(t, kind, p, p[6], (int8_t)p[7], p[8], now_ms);
    }
}

static inline bool gz_radar_live(const gz_radar_row_t *r, uint32_t now_ms)
{
    return (uint32_t)(now_ms - r->last_seen_ms) < GZ_RADAR_TTL_MS;
}

/* ---- classification ------------------------------------------------------ */

/* Mirrors fam_colour() in src/features/c5_godzilla.cpp (#54). */
static inline uint8_t gz_radar_severity(uint8_t family)
{
    switch (family) {
    case POSEI_DET_FAM_FLOCK:
    case POSEI_DET_FAM_RAVEN:
    case POSEI_DET_FAM_REMOTEID:
    case POSEI_DET_FAM_SKIMMER:    /* §4: card skimmer     */
    case POSEI_DET_FAM_LE_EQUIP:   /* §4: body-cam / LE gear */
    case POSEI_DET_FAM_ALPR:       /* §4: non-Flock ALPR   */
                                  return GZ_SEV_SURVEIL;   /* surveillance-red */
    case POSEI_DET_FAM_FINDMY:
    case POSEI_DET_FAM_AIRTAG:
    case POSEI_DET_FAM_TILE:
    case POSEI_DET_FAM_SMARTTAG:
    case POSEI_DET_FAM_CAMERA:     /* §4: covert / IP camera */
    case POSEI_DET_FAM_SMARTGLASS: /* §4: recording glasses  */
                                  return GZ_SEV_TRACKER;   /* amber tier       */
    default:                      return GZ_SEV_NEUTRAL;
    }
}

/* Same strings as det_family_name() in src/det_classify.h. */
static inline const char *gz_radar_family_name(uint8_t family)
{
    switch (family) {
    case POSEI_DET_FAM_WIFI_AP:     return "Wi-Fi AP";
    case POSEI_DET_FAM_WIFI_STA:    return "Wi-Fi STA";
    case POSEI_DET_FAM_BLE_GENERIC: return "BLE";
    case POSEI_DET_FAM_FINDMY:      return "Find My";
    case POSEI_DET_FAM_AIRTAG:      return "AirTag";
    case POSEI_DET_FAM_IBEACON:     return "iBeacon";
    case POSEI_DET_FAM_REMOTEID:    return "Remote ID";
    case POSEI_DET_FAM_FLOCK:       return "Flock";
    case POSEI_DET_FAM_RAVEN:       return "Raven";
    case POSEI_DET_FAM_TILE:        return "Tile";
    case POSEI_DET_FAM_SMARTTAG:    return "SmartTag";
    case POSEI_DET_FAM_SKIMMER:     return "BT skimmer";
    case POSEI_DET_FAM_CAMERA:      return "IP camera";
    case POSEI_DET_FAM_LE_EQUIP:    return "LE gear";
    case POSEI_DET_FAM_ALPR:        return "ALPR";
    case POSEI_DET_FAM_SMARTGLASS:  return "Smart glasses";
    default:                        return "Unknown";
    }
}

/* ---- geometry ------------------------------------------------------------ */

/* Radius in px: strong RSSI near r_min, weak near r_max, linear between. */
static inline int gz_radar_radius(int8_t rssi, int r_min, int r_max)
{
    int v = rssi;
    if (v > GZ_RADAR_RSSI_STRONG) v = GZ_RADAR_RSSI_STRONG;
    if (v < GZ_RADAR_RSSI_WEAK)   v = GZ_RADAR_RSSI_WEAK;
    const int span = GZ_RADAR_RSSI_STRONG - GZ_RADAR_RSSI_WEAK;   /* 60 */
    return r_min + ((r_max - r_min) * (GZ_RADAR_RSSI_STRONG - v) + span / 2) / span;
}

/* Stable angle 0..359 degrees from FNV-1a over (kind, addr) + fmix32. */
static inline uint16_t gz_radar_angle(uint8_t kind, const uint8_t addr[6])
{
    uint32_t h = 2166136261u;
    h = (h ^ kind) * 16777619u;
    for (int i = 0; i < 6; ++i) h = (h ^ addr[i]) * 16777619u;
    /* fmix32 avalanche: plain FNV clusters MACs that differ only in the last
     * byte (sequential vendor MACs) into a few sectors. */
    h ^= h >> 16; h *= 0x85EBCA6Bu;
    h ^= h >> 13; h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return (uint16_t)(h % 360u);
}

/* Screen position: 0 deg = up (12 o'clock), clockwise, y grows downward. */
static inline void gz_radar_xy(uint16_t angle_deg, int radius, int cx, int cy,
                               int *x, int *y)
{
    const float a = (float)angle_deg * 0.017453292519943f;
    *x = cx + (int)lroundf((float)radius * sinf(a));
    *y = cy - (int)lroundf((float)radius * cosf(a));
}

/* Where row r sits on a dial centred at (cx, cy). */
static inline void gz_radar_place(const gz_radar_row_t *r, int cx, int cy,
                                  int r_min, int r_max, int *x, int *y)
{
    gz_radar_xy(gz_radar_angle(r->kind, r->addr),
                gz_radar_radius(r->rssi, r_min, r_max), cx, cy, x, y);
}

/* Index of the live row whose dot is nearest (px, py) within `tol` px, or -1. */
static inline int gz_radar_hit(const gz_radar_row_t *rows, int n, uint32_t now_ms,
                               int cx, int cy, int r_min, int r_max,
                               int px, int py, int tol)
{
    int best = -1;
    long best_d2 = (long)tol * tol + 1;
    for (int i = 0; i < n; ++i) {
        if (!gz_radar_live(&rows[i], now_ms)) continue;
        int x, y;
        gz_radar_place(&rows[i], cx, cy, r_min, r_max, &x, &y);
        long dx = x - px, dy = y - py;
        long d2 = dx * dx + dy * dy;
        if (d2 < best_d2) { best_d2 = d2; best = i; }
    }
    return best;
}
