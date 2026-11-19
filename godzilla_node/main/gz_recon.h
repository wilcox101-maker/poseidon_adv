/*
 * gz_recon.h — Recon session log model (P3, first Labs unit).
 *
 * A Recon session writes the node's own detections to a CSV file on the
 * microSD card: one row when a device is first seen in the session, then at
 * most one row every GZ_RECON_REPEAT_MS while it keeps being seen (so a
 * session records presence over time without a row per advertisement).
 *
 * This header is the pure half: the "is a row due" policy, a fixed ring queue
 * between the publish tick (producer) and the SD writer task (consumer), the
 * session path and the CSV text. The SD card, the task and the start/stop
 * requests are in gz_recon.cpp.
 *
 * CSV columns (RFC 4180 plain fields, no quoting needed — no field can hold a
 * comma):
 *   time        RTC wall time, ISO 8601 without zone (the RTC has none);
 *               empty if the clock was never set or read
 *   uptime_ms   millis() when the detection was published
 *   radio       wifi | ble
 *   address     AA:BB:CC:DD:EE:FF
 *   channel     Wi-Fi channel (empty for BLE)
 *   addr_type   public | random (empty for Wi-Fi)
 *   rssi_dbm    latest RSSI
 *   family      classifier family name, as on the radar card
 *   hits        published sightings so far
 *
 * Passive: bookkeeping of what the receive-only scanners already saw.
 * Pure: stdint/stdio + gz_radar.h / gz_rtc.h pure halves. C and C++.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "gz_radar.h"
#include "gz_rtc.h"

#define GZ_RECON_REPEAT_MS  30000u   /* re-log a device still in view every 30 s */
#define GZ_RECON_Q_LEN      64       /* rows buffered between publish and SD     */
#define GZ_RECON_ROW_MAX    96       /* longest CSV row incl. newline + NUL      */
#define GZ_RECON_PATH_MAX   40

#define GZ_RECON_CSV_HEADER \
    "time,uptime_ms,radio,address,channel,addr_type,rssi_dbm,family,hits\n"

/* Is a row due for this radar row? First sighting of the session, or the
 * repeat interval has passed (unsigned age: millis() wrap safe). */
static inline bool gz_recon_due(const gz_radar_row_t *r, uint32_t now_ms)
{
    return !r->logged || (uint32_t)(now_ms - r->log_ms) >= GZ_RECON_REPEAT_MS;
}

/* ---- ring queue ---------------------------------------------------------- */

typedef struct {
    gz_radar_row_t e[GZ_RECON_Q_LEN];
    uint16_t head;        /* next slot to pop  */
    uint16_t count;
    uint32_t dropped;     /* rows refused because the queue was full */
} gz_recon_q_t;

static inline void gz_recon_q_clear(gz_recon_q_t *q)
{
    q->head = 0;
    q->count = 0;
    q->dropped = 0;
}

/* Push a copy. When full the NEW row is refused (and counted): the rows
 * already queued are older and nearer to being written. */
static inline bool gz_recon_q_push(gz_recon_q_t *q, const gz_radar_row_t *r)
{
    if (q->count >= GZ_RECON_Q_LEN) {
        q->dropped++;
        return false;
    }
    q->e[(q->head + q->count) % GZ_RECON_Q_LEN] = *r;
    q->count++;
    return true;
}

/* Pop up to `max` rows, oldest first; returns how many. */
static inline int gz_recon_q_pop(gz_recon_q_t *q, gz_radar_row_t *out, int max)
{
    int n = 0;
    while (n < max && q->count > 0) {
        out[n++] = q->e[q->head];
        q->head = (uint16_t)((q->head + 1u) % GZ_RECON_Q_LEN);
        q->count--;
    }
    return n;
}

/* ---- session path -------------------------------------------------------- */

/* Directory and file for a session that starts at *t. 8.3-safe names, so it
 * works whether or not the FAT driver has long file names:
 *   /recon/2026/11190510.CSV   (month day hour minute)
 * A second session in the same minute appends to the same file. With no valid
 * time, sessions append to /recon/NODATE/SESSION.CSV. Returns false if either
 * buffer is too small. */
static inline bool gz_recon_paths(const gz_datetime_t *t, char *dir, size_t dn,
                                  char *file, size_t fn)
{
    int a, b;
    if (t && gz_rtc_valid(t)) {
        a = snprintf(dir, dn, "/recon/%04u", (unsigned)t->year);
        b = snprintf(file, fn, "/recon/%04u/%02u%02u%02u%02u.CSV", (unsigned)t->year,
                     (unsigned)t->month, (unsigned)t->day,
                     (unsigned)t->hour, (unsigned)t->minute);
    } else {
        a = snprintf(dir, dn, "/recon/NODATE");
        b = snprintf(file, fn, "/recon/NODATE/SESSION.CSV");
    }
    return a > 0 && (size_t)a < dn && b > 0 && (size_t)b < fn;
}

/* ---- CSV row ------------------------------------------------------------- */

/* Format one row (with trailing newline) into buf. `t` may be NULL or invalid:
 * the time field is then left empty. Returns the length written, or 0 if the
 * row did not fit. */
static inline int gz_recon_format_row(char *buf, size_t n, const gz_datetime_t *t,
                                      const gz_radar_row_t *r)
{
    char when[32] = "";
    if (t && gz_rtc_valid(t))
        snprintf(when, sizeof when, "%04u-%02u-%02uT%02u:%02u:%02u",
                 (unsigned)t->year, (unsigned)t->month, (unsigned)t->day,
                 (unsigned)t->hour, (unsigned)t->minute, (unsigned)t->second);

    char chan[6] = "";
    const char *atype = "";
    const char *radio;
    if (r->kind == GZ_RADAR_WIFI) {
        radio = "wifi";
        snprintf(chan, sizeof chan, "%u", (unsigned)r->aux);
    } else {
        radio = "ble";
        atype = r->aux ? "random" : "public";
    }

    int len = snprintf(buf, n,
                       "%s,%lu,%s,%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%d,%s,%u\n",
                       when, (unsigned long)r->last_seen_ms, radio,
                       r->addr[0], r->addr[1], r->addr[2],
                       r->addr[3], r->addr[4], r->addr[5],
                       chan, atype, (int)r->rssi,
                       gz_radar_family_name(r->family), (unsigned)r->hits);
    return (len > 0 && (size_t)len < n) ? len : 0;
}
