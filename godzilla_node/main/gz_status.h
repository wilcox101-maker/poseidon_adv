/*
 * gz_status.h — status-screen model for the GODZILLA node (P2a).
 *
 * What the bench status screen shows, mirroring the Cardputer's C5 -> x view:
 * per-radio frames published, entries in the last batch, and the age of the
 * newest publish; plus an uptime string. Fixed identity fields (name, link
 * channel, PSRAM, I2C scan count) ride along so the UI renders from one
 * snapshot.
 *
 * Pure: no Arduino, no locks. The on-target owner (gz_stats.cpp) guards a
 * single instance with a spinlock — the radio publish ticks write it on the
 * Arduino loop, the LVGL task reads a copy. Pinned by test/test_godzilla_status.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum { GZ_RADIO_BLE = 0, GZ_RADIO_WIFI = 1 } gz_radio_t;

typedef struct {
    /* fixed at boot */
    char     node_name[12];
    uint8_t  link_channel;
    uint32_t psram_kb;
    uint8_t  i2c_found;
    bool     rtc_ok;        /* PCF85063 answered at boot                  */
    bool     rtc_seeded;    /* clock was seeded from build time this boot */
    /* live counters */
    uint32_t ble_frames;
    uint8_t  ble_last_entries;
    uint32_t wifi_frames;
    uint8_t  wifi_last_entries;
    uint32_t last_pub_ms;
    bool     has_published;
} gz_status_t;

static inline void gz_status_init(gz_status_t *s)
{
    memset(s, 0, sizeof(*s));
}

/* Record one non-empty publish of `entries` detections on `radio` at now_ms. */
static inline void gz_status_note_publish(gz_status_t *s, gz_radio_t radio,
                                          uint8_t entries, uint32_t now_ms)
{
    if (radio == GZ_RADIO_BLE) { s->ble_frames++;  s->ble_last_entries  = entries; }
    else                       { s->wifi_frames++; s->wifi_last_entries = entries; }
    s->last_pub_ms   = now_ms;
    s->has_published = true;
}

/* "H:MM:SS", or "Nd H:MM:SS" past a day. Bounded: snprintf truncates and
 * always NUL-terminates. */
static inline void gz_fmt_uptime(uint32_t ms, char *out, size_t n)
{
    if (n == 0) return;
    uint32_t t = ms / 1000u;
    uint32_t d = t / 86400u; t %= 86400u;
    uint32_t h = t / 3600u;  t %= 3600u;
    uint32_t m = t / 60u;
    uint32_t sec = t % 60u;
    if (d) snprintf(out, n, "%lud %lu:%02lu:%02lu", (unsigned long)d,
                    (unsigned long)h, (unsigned long)m, (unsigned long)sec);
    else   snprintf(out, n, "%lu:%02lu:%02lu",
                    (unsigned long)h, (unsigned long)m, (unsigned long)sec);
}

/* Age of the newest publish: "never", "Ns" under a minute, else "MmSSs".
 * Unsigned subtraction keeps it right across the millis() wrap. */
static inline void gz_fmt_pub_age(const gz_status_t *s, uint32_t now_ms,
                                  char *out, size_t n)
{
    if (n == 0) return;
    if (!s->has_published) { snprintf(out, n, "never"); return; }
    uint32_t sec = (uint32_t)(now_ms - s->last_pub_ms) / 1000u;
    if (sec < 60u) snprintf(out, n, "%lus", (unsigned long)sec);
    else           snprintf(out, n, "%lum%02lus", (unsigned long)(sec / 60u),
                            (unsigned long)(sec % 60u));
}
