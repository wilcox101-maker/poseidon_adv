/*
 * gps — NMEA parser for the M5Stack LoRa-GNSS HAT.
 *
 * Hardware: HAT sits on the Cardputer's expansion header and exposes
 * the u-blox GPS module on UART1. Cardputer ADV pinout:
 *   GPIO 15 = GPS TX  (module → MCU)   MCU RX. Hydra does not use this pad.
 *   GPIO 13 = GPS RX  (MCU → module)   MCU TX. Hydra CC1101 CS is this pad.
 *   115200 baud, 8N1 on the CAP-LoRa1262 module
 *
 * The PINGEQUA Hydra RF Cap is a different hat (CC1101 CS=13, GDO0=5,
 * nRF24 CS=6, CE=4). Only GPIO 13 is shared with GPS, so CC1101 and
 * nRF24 bring-up call gps_end() before driving CC1101 CS. GDO0 does
 * not conflict with this UART.
 *
 * We poll the UART every 100ms, parse GGA + RMC sentences, keep the
 * last valid fix in a global struct. Non-blocking — safe to call from
 * any task.
 */
#pragma once

#include <Arduino.h>

#define GPS_UART_RX_PIN 15
#define GPS_UART_TX_PIN 13
#define GPS_BAUD        115200

struct gps_fix_t {
    bool     valid;          /* true when we have a 3D fix */
    double   lat_deg;        /* signed degrees */
    double   lon_deg;
    float    alt_m;
    float    speed_kts;
    float    course_deg;
    uint8_t  sats;
    float    hdop;
    uint32_t time_ms;        /* millis() when last updated */
    char     utc[12];        /* HHMMSS.ss */
    char     date[8];        /* DDMMYY */
};

bool gps_begin(void);
void gps_end(void);
void gps_poll(void);

/* rf-015: cheap "is the UART up?" probe so callers that conditionally
 * tear down GPS (cc1101_park_others) can skip the destructive
 * s_uart.end()+pinMode dance when GPS isn't running anyway. */
bool gps_is_up(void);

/* OPSEC gate. GPS is OFF by default at boot — the user must
 * explicitly opt in via the menu before WiFi captures / wardrive logs
 * carry observer coordinates. Persisted in NVS namespace "gps", key
 * "enabled" (uint8_t, 0=off, 1=on). See sys-015 / POS-AUDIT entry. */
bool gps_user_enabled(void);
void gps_set_user_enabled(bool on);

/* Treat "opening the GPS / Wardrive page" as opt-in: turns the user
 * flag on (persists to NVS), calls gps_begin() if not already up, and
 * spawns the background polling task if not already running. Idempotent
 * — safe to call from any menu entry. Returns true if GPS is now live. */
bool gps_ensure_running(void);
/* Restart the UART at a different baud. Returns the new baud. The
 * cycle covers every rate the ATGM336H/AT6668 datasheets list. */
uint32_t gps_cycle_baud(void);
uint32_t gps_current_baud(void);
const gps_fix_t &gps_get(void);

/* Convenience: snapshot for CSV writers. Returns false if no fix yet. */
bool gps_snapshot(gps_fix_t *out);

/* Diagnostics: bytes / lines / sentences seen since boot, plus the most
 * recent NMEA line. Used by the GPS fix page so the user can tell
 * whether the UART is wired right vs no sky view. */
struct gps_diag_t {
    uint32_t bytes;
    uint32_t lines;
    uint32_t gga;
    uint32_t rmc;
    uint32_t overflows;     /* gps-003: lines > 128 B truncated */
    int      last_gga_quality;
    char     last_rmc_status;
    char     last[96];
    char     last_gga[96];
    char     last_rmc[96];
};
const gps_diag_t &gps_diag(void);
