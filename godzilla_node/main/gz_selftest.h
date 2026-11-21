/*
 * gz_selftest.h — pure model for the GODZILLA bench self-test page (item J). PURE.
 *
 * First hardware bring-up needs a single screen that says, per board capability
 * flag, whether each on-board part answered and — where one is fitted — one live
 * reading. The firmware page (gz_ui.cpp) gathers a snapshot from the live
 * subsystems (display, touch, RTC, IMU, gauge, mic, SD, the two radios and the
 * ESP-NOW link) and hands it here; this header owns everything that can be
 * reasoned about on the host:
 *   - the fixed line order and their ASCII labels;
 *   - the pass / fail / skip verdict per line (SKIP = the part is not fitted on
 *     this board, so the line does not apply);
 *   - the per-line value text shown next to the verdict;
 *   - the capture-rate helper (frames or adverts per second, from a count delta
 *     over an elapsed window, integer-only — no float on the host or target);
 *   - the one-line serial summary printed at boot behind GZ_SELFTEST_SERIAL.
 *
 * Read-only by construction: nothing here writes a device, NVS, RTC or SD, and
 * the page that uses it only reads. ASCII only — the node's LVGL build carries
 * just the built-in Montserrat fonts, so any non-ASCII renders as a box
 * (#147/#150).
 *
 * Pure: stdint/stdbool/stdio/string only; compiles as C and C++. Native-tested
 * in test/test_godzilla_selftest.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Per-line verdict. SKIP: the capability flag is 0 on this board, so the part is
 * absent and the line is not applicable (never a failure). */
typedef enum { GZ_ST_SKIP = 0, GZ_ST_PASS = 1, GZ_ST_FAIL = 2 } gz_st_result_t;

/* The self-test lines, in the order the page lists them. Display and touch and
 * the two radios and the link exist on every board; RTC / IMU / gauge / mic / SD
 * are gated by their board capability flag. */
enum {
    GZ_ST_DISPLAY = 0,
    GZ_ST_TOUCH,
    GZ_ST_RTC,
    GZ_ST_IMU,
    GZ_ST_GAUGE,
    GZ_ST_MIC,
    GZ_ST_SD,
    GZ_ST_WIFI,
    GZ_ST_BLE,
    GZ_ST_LINK,
    GZ_ST_LINE_COUNT            /* 10 lines */
};

/* The snapshot the firmware page fills from the live subsystems and hands to the
 * formatters below. Capability flags come straight from the board header
 * (GZ_BOARD_HAS_*); the rest are the readings taken this refresh. */
typedef struct {
    /* board capability flags (compile-time; 1 = part fitted) */
    bool cap_rtc, cap_imu, cap_gauge, cap_mic, cap_sd;

    /* display: the panel completed its first flush (it is driving pixels) */
    bool disp_ok;

    /* touch: controller answered on the bus, and the last tap coordinates */
    bool     touch_found;
    bool     touch_seen;        /* at least one tap registered since boot */
    uint16_t touch_x, touch_y;

    /* RTC: the part answered, and the oscillator-stopped flag from that read */
    bool rtc_ok;
    bool rtc_osc_stopped;

    /* IMU: WHO_AM_I matched and one live motion reading */
    bool imu_ok;
    bool imu_moving;

    /* fuel gauge: a battery voltage was read (millivolts) */
    bool     gauge_ok;
    uint16_t gauge_mv;

    /* mic ADC: the codec answered on the boot I2C scan */
    bool mic_ok;

    /* microSD: a card is mounted */
    bool sd_mounted;

    /* radios: capture-rate window — counts accumulated over rate_window_ms.
     * wifi_any / ble_any stay true once any frame/advert has ever been seen, so
     * a momentarily quiet window still reads PASS. */
    uint32_t wifi_delta, ble_delta, rate_window_ms;
    bool     wifi_any, ble_any;

    /* ESP-NOW link to the S3: a HELLO has been sent (link up) and the age of the
     * newest detection publish. */
    bool     link_up;
    bool     has_published;
    uint32_t pub_age_ms;
} gz_selftest_t;

/* Capture rate as frames (or adverts) per second, scaled by 10 so the integer
 * carries one decimal place: 123 means 12.3/s. Rounded; 0 when the window is 0.
 * All uint32 math — the product fits (a plausible delta is well under 2^22 and
 * 10000 < 2^14, so delta*10000 stays within uint64 only for extreme deltas, so
 * clamp the delta first). */
static inline uint32_t gz_st_rate_x10(uint32_t delta, uint32_t window_ms)
{
    if (window_ms == 0u) return 0u;
    if (delta > 400000u) delta = 400000u;      /* clamp: keeps delta*10000 in uint32 range */
    return (delta * 10000u + window_ms / 2u) / window_ms;
}

/* "12.3/s" from a count delta over a window. Bounded: snprintf truncates and
 * always NUL-terminates. */
static inline void gz_st_fmt_rate(uint32_t delta, uint32_t window_ms,
                                  char *out, size_t n)
{
    if (n == 0u) return;
    uint32_t r = gz_st_rate_x10(delta, window_ms);
    snprintf(out, n, "%lu.%lu/s", (unsigned long)(r / 10u), (unsigned long)(r % 10u));
}

/* The ASCII label for a line (left column). */
static inline const char *gz_selftest_label(int line)
{
    switch (line) {
    case GZ_ST_DISPLAY: return "Display";
    case GZ_ST_TOUCH:   return "Touch";
    case GZ_ST_RTC:     return "RTC";
    case GZ_ST_IMU:     return "IMU";
    case GZ_ST_GAUGE:   return "Gauge";
    case GZ_ST_MIC:     return "Mic";
    case GZ_ST_SD:      return "SD";
    case GZ_ST_WIFI:    return "Wi-Fi";
    case GZ_ST_BLE:     return "BLE";
    case GZ_ST_LINK:    return "Link";
    default:            return "?";
    }
}

/* The pass / fail / skip verdict for a line, from the snapshot. A part whose
 * capability flag is clear reads SKIP (not applicable on this board). */
static inline gz_st_result_t gz_selftest_result(const gz_selftest_t *s, int line)
{
    switch (line) {
    case GZ_ST_DISPLAY: return s->disp_ok ? GZ_ST_PASS : GZ_ST_FAIL;
    case GZ_ST_TOUCH:   return s->touch_found ? GZ_ST_PASS : GZ_ST_FAIL;
    case GZ_ST_RTC:
        if (!s->cap_rtc) return GZ_ST_SKIP;
        /* answered and running = PASS; a stopped oscillator (cold boot, no
         * backup cell) is a real finding, so it reads FAIL until seeded. */
        return (s->rtc_ok && !s->rtc_osc_stopped) ? GZ_ST_PASS : GZ_ST_FAIL;
    case GZ_ST_IMU:     return !s->cap_imu   ? GZ_ST_SKIP : (s->imu_ok     ? GZ_ST_PASS : GZ_ST_FAIL);
    case GZ_ST_GAUGE:   return !s->cap_gauge ? GZ_ST_SKIP : (s->gauge_ok   ? GZ_ST_PASS : GZ_ST_FAIL);
    case GZ_ST_MIC:     return !s->cap_mic   ? GZ_ST_SKIP : (s->mic_ok     ? GZ_ST_PASS : GZ_ST_FAIL);
    case GZ_ST_SD:      return !s->cap_sd    ? GZ_ST_SKIP : (s->sd_mounted ? GZ_ST_PASS : GZ_ST_FAIL);
    case GZ_ST_WIFI:    return s->wifi_any ? GZ_ST_PASS : GZ_ST_FAIL;
    case GZ_ST_BLE:     return s->ble_any  ? GZ_ST_PASS : GZ_ST_FAIL;
    case GZ_ST_LINK:    return (s->link_up && s->has_published) ? GZ_ST_PASS : GZ_ST_FAIL;
    default:            return GZ_ST_FAIL;
    }
}

/* The short tag shown with the verdict: "ok" / "FAIL" / "--" (skip). */
static inline const char *gz_st_tag(gz_st_result_t r)
{
    return r == GZ_ST_PASS ? "ok" : (r == GZ_ST_FAIL ? "FAIL" : "--");
}

/* The value text for a line (right column): the live reading, or why the line
 * failed / why it does not apply. ASCII only, bounded. Returns the line's
 * verdict so the caller can colour the row in one call. */
static inline gz_st_result_t gz_selftest_fmt_value(const gz_selftest_t *s, int line,
                                                   char *out, size_t n)
{
    gz_st_result_t r = gz_selftest_result(s, line);
    if (n == 0u) return r;
    switch (line) {
    case GZ_ST_DISPLAY:
        snprintf(out, n, s->disp_ok ? "driving pixels" : "no flush");
        break;
    case GZ_ST_TOUCH:
        if (!s->touch_found)   snprintf(out, n, "not found");
        else if (!s->touch_seen) snprintf(out, n, "no tap yet");
        else snprintf(out, n, "%u,%u", (unsigned)s->touch_x, (unsigned)s->touch_y);
        break;
    case GZ_ST_RTC:
        if (!s->cap_rtc)              snprintf(out, n, "n/a");
        else if (!s->rtc_ok)         snprintf(out, n, "no answer");
        else if (s->rtc_osc_stopped) snprintf(out, n, "osc stopped");
        else                         snprintf(out, n, "running");
        break;
    case GZ_ST_IMU:
        if (!s->cap_imu)      snprintf(out, n, "n/a");
        else if (!s->imu_ok)  snprintf(out, n, "no answer");
        else                  snprintf(out, n, s->imu_moving ? "moving" : "still");
        break;
    case GZ_ST_GAUGE:
        if (!s->cap_gauge)      snprintf(out, n, "n/a");
        else if (!s->gauge_ok)  snprintf(out, n, "no answer");
        else snprintf(out, n, "%u.%02u V", (unsigned)(s->gauge_mv / 1000u),
                      (unsigned)((s->gauge_mv % 1000u) / 10u));
        break;
    case GZ_ST_MIC:
        if (!s->cap_mic)     snprintf(out, n, "n/a");
        else                 snprintf(out, n, s->mic_ok ? "present" : "no answer");
        break;
    case GZ_ST_SD:
        if (!s->cap_sd)      snprintf(out, n, "n/a");
        else                 snprintf(out, n, s->sd_mounted ? "mounted" : "no card");
        break;
    case GZ_ST_WIFI:
        gz_st_fmt_rate(s->wifi_delta, s->rate_window_ms, out, n);
        break;
    case GZ_ST_BLE:
        gz_st_fmt_rate(s->ble_delta, s->rate_window_ms, out, n);
        break;
    case GZ_ST_LINK:
        if (!s->link_up)            snprintf(out, n, "down");
        else if (!s->has_published) snprintf(out, n, "up, no publish");
        else {
            uint32_t sec = s->pub_age_ms / 1000u;
            if (sec < 60u) snprintf(out, n, "pub %lus", (unsigned long)sec);
            else snprintf(out, n, "pub %lum%02lus", (unsigned long)(sec / 60u),
                          (unsigned long)(sec % 60u));
        }
        break;
    default:
        snprintf(out, n, "?");
        break;
    }
    return r;
}

/* A full "Label  value (tag)" row, for the serial dump and simple callers.
 * Bounded. Returns the verdict. */
static inline gz_st_result_t gz_selftest_fmt_line(const gz_selftest_t *s, int line,
                                                  char *out, size_t n)
{
    char val[24];
    gz_st_result_t r = gz_selftest_fmt_value(s, line, val, sizeof val);
    if (n) snprintf(out, n, "%-7s %s (%s)", gz_selftest_label(line), val, gz_st_tag(r));
    return r;
}

/* The one-line serial summary printed once at boot behind GZ_SELFTEST_SERIAL:
 * "SELFTEST disp=ok touch=ok rtc=ok imu=ok gauge=ok mic=-- sd=ok wifi=ok ble=ok
 *  link=ok". Keys mirror the page's line labels; a skipped line reads "--".
 * Bounded: truncates and always NUL-terminates. */
static inline void gz_selftest_serial(const gz_selftest_t *s, char *out, size_t n)
{
    static const char *key[GZ_ST_LINE_COUNT] = {
        "disp", "touch", "rtc", "imu", "gauge", "mic", "sd", "wifi", "ble", "link"
    };
    if (n == 0u) return;
    size_t at = 0;
    int w = snprintf(out, n, "SELFTEST");
    if (w > 0) at = (size_t)w < n ? (size_t)w : n - 1;
    for (int i = 0; i < GZ_ST_LINE_COUNT; ++i) {
        gz_st_result_t r = gz_selftest_result(s, i);
        int k = snprintf(out + at, n - at, " %s=%s", key[i], gz_st_tag(r));
        if (k <= 0) break;
        at += (size_t)k < (n - at) ? (size_t)k : (n - at - 1);
        if (at >= n - 1) break;
    }
}
