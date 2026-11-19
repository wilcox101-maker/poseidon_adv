/*
 * gz_hunt.h — Hunt view model (P3): an RSSI trace for ONE selected contact.
 *
 * Pick a dot on the radar (tap it, then long-press the card) and the node
 * keeps the last GZ_HUNT_SAMPLES published RSSI readings for that one
 * (kind, address). Walk around with the node: the trace and the trend say
 * whether the signal is getting stronger or weaker. That is the whole tool —
 * a received-signal meter, not a direction finder (one omni antenna).
 *
 * Feed: the same published batches as the radar, noted inside the radar
 * store's critical section (gz_radar_store.cpp), so the trace shows exactly
 * what went on the wire. Roughly one sample per publish tick of that radio
 * (~1 s), so the window is about a minute.
 *
 * Pure: stdint/string only, no Arduino/LVGL. Compiles as C and C++.
 * Passive: bookkeeping of what the receive-only scanners already saw.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "gz_radar.h"   /* GZ_RADAR_*, RSSI clamp range, TTL */

#define GZ_HUNT_SAMPLES   60     /* ring length: ~1 min at one sample/s   */
#define GZ_HUNT_TREND_N   5      /* samples per half of the trend compare */
#define GZ_HUNT_TREND_DB  3      /* mean change that counts as a trend    */

enum { GZ_HUNT_TREND_NONE = 0,   /* too few samples to say               */
       GZ_HUNT_TREND_STEADY,
       GZ_HUNT_TREND_STRONGER,   /* mean RSSI rose: probably closer       */
       GZ_HUNT_TREND_WEAKER };   /* mean RSSI fell: probably further away */

typedef struct {
    bool     active;             /* a target is selected                  */
    uint8_t  kind;               /* GZ_RADAR_WIFI / GZ_RADAR_BLE          */
    uint8_t  addr[6];
    uint8_t  aux;                /* latest: Wi-Fi channel · BLE addr_type */
    uint8_t  family;             /* latest POSEI_DET_FAM_* (sticky in the radar row) */
    int8_t   rssi[GZ_HUNT_SAMPLES];
    uint8_t  n;                  /* samples held, <= GZ_HUNT_SAMPLES      */
    uint8_t  head;               /* next write slot                       */
    uint32_t last_ms;            /* time of the newest sample             */
} gz_hunt_t;

static inline void gz_hunt_clear(gz_hunt_t *h) { memset(h, 0, sizeof *h); }

static inline bool gz_hunt_is(const gz_hunt_t *h, uint8_t kind, const uint8_t addr[6])
{
    return h->active && h->kind == kind && memcmp(h->addr, addr, 6) == 0;
}

/* Append one reading (oldest is overwritten when full). */
static inline void gz_hunt_push(gz_hunt_t *h, int8_t rssi, uint32_t now_ms)
{
    h->rssi[h->head] = rssi;
    h->head = (uint8_t)((h->head + 1u) % GZ_HUNT_SAMPLES);
    if (h->n < GZ_HUNT_SAMPLES) h->n++;
    h->last_ms = now_ms;
}

/* Start hunting `row`: a fresh trace seeded with the row's latest reading. */
static inline void gz_hunt_select(gz_hunt_t *h, const gz_radar_row_t *row)
{
    gz_hunt_clear(h);
    h->active = true;
    h->kind   = row->kind;
    memcpy(h->addr, row->addr, 6);
    h->aux    = row->aux;
    h->family = row->family;
    gz_hunt_push(h, row->rssi, row->last_seen_ms);
}

/* Note one published sighting; only the selected target is kept. `family`
 * should be the radar row's (sticky) family. Returns true if it matched. */
static inline bool gz_hunt_note(gz_hunt_t *h, uint8_t kind, const uint8_t addr[6],
                                uint8_t aux, int8_t rssi, uint8_t family,
                                uint32_t now_ms)
{
    if (!gz_hunt_is(h, kind, addr)) return false;
    h->aux    = aux;
    h->family = family;
    gz_hunt_push(h, rssi, now_ms);
    return true;
}

/* Sample i in time order, 0 = oldest held, n-1 = newest. */
static inline int8_t gz_hunt_at(const gz_hunt_t *h, int i)
{
    int start = (h->n < GZ_HUNT_SAMPLES) ? 0 : h->head;
    return h->rssi[(start + i) % GZ_HUNT_SAMPLES];
}

static inline int8_t gz_hunt_latest(const gz_hunt_t *h)
{
    return h->n ? gz_hunt_at(h, h->n - 1) : (int8_t)GZ_RADAR_RSSI_WEAK;
}

/* Min / max over the held window. False (outputs untouched) when empty. */
static inline bool gz_hunt_minmax(const gz_hunt_t *h, int8_t *lo, int8_t *hi)
{
    if (h->n == 0) return false;
    int8_t a = gz_hunt_at(h, 0), b = a;
    for (int i = 1; i < h->n; ++i) {
        int8_t v = gz_hunt_at(h, i);
        if (v < a) a = v;
        if (v > b) b = v;
    }
    *lo = a; *hi = b;
    return true;
}

/* Mean of the newest GZ_HUNT_TREND_N samples against the GZ_HUNT_TREND_N
 * before them. A change of GZ_HUNT_TREND_DB dB or more is a trend. */
static inline int gz_hunt_trend(const gz_hunt_t *h)
{
    if (h->n < 2 * GZ_HUNT_TREND_N) return GZ_HUNT_TREND_NONE;
    int recent = 0, prior = 0;
    for (int k = 0; k < GZ_HUNT_TREND_N; ++k) {
        recent += gz_hunt_at(h, h->n - 1 - k);
        prior  += gz_hunt_at(h, h->n - 1 - GZ_HUNT_TREND_N - k);
    }
    /* Compare sums: d >= TREND_DB * N  <=>  mean change >= TREND_DB. */
    int d = recent - prior;
    if (d >=  GZ_HUNT_TREND_DB * GZ_HUNT_TREND_N) return GZ_HUNT_TREND_STRONGER;
    if (d <= -GZ_HUNT_TREND_DB * GZ_HUNT_TREND_N) return GZ_HUNT_TREND_WEAKER;
    return GZ_HUNT_TREND_STEADY;
}

/* Lost: nothing new for as long as the radar keeps a dot (unsigned age). */
static inline bool gz_hunt_lost(const gz_hunt_t *h, uint32_t now_ms)
{
    return h->n == 0 || (uint32_t)(now_ms - h->last_ms) >= GZ_RADAR_TTL_MS;
}

/* Trace y for an RSSI, strong at `top`, weak at `bottom`, clamped to the
 * radar's range so the trace and the radar agree on "strong". */
static inline int gz_hunt_y(int8_t rssi, int top, int bottom)
{
    int v = rssi;
    if (v > GZ_RADAR_RSSI_STRONG) v = GZ_RADAR_RSSI_STRONG;
    if (v < GZ_RADAR_RSSI_WEAK)   v = GZ_RADAR_RSSI_WEAK;
    const int span = GZ_RADAR_RSSI_STRONG - GZ_RADAR_RSSI_WEAK;   /* 60 */
    return top + ((bottom - top) * (GZ_RADAR_RSSI_STRONG - v) + span / 2) / span;
}

/* x for sample i of n across [left, right]; the newest sits at `right`, so
 * the trace grows in from the right edge until the window fills. */
static inline int gz_hunt_x(int i, int n, int left, int right)
{
    const int slots = GZ_HUNT_SAMPLES - 1;
    int pos = i + (GZ_HUNT_SAMPLES - n);          /* right-align */
    return left + ((right - left) * pos + slots / 2) / slots;
}
