/*
 * gz_recon_task.h — microSD card + Recon session writer (P3).
 *
 * One low-priority task owns the SD card (SDMMC 4-bit, pins in gz_board.h). A
 * session is started and stopped from the UI (long-press on the radar); while
 * one is running the task drains the Recon queue (gz_radar_store) into a CSV
 * file — format in gz_recon.h. Nothing is written unless a session is running,
 * and the card is never formatted.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GZ_RECON_IDLE = 0,     /* no session                              */
    GZ_RECON_REC,          /* session running, rows being written     */
    GZ_RECON_NO_CARD,      /* a start was asked for but no card mounted */
    GZ_RECON_ERROR,        /* the card stopped accepting writes       */
} gz_recon_state_t;

typedef struct {
    gz_recon_state_t state;
    uint32_t state_ms;     /* millis() when `state` was entered       */
    uint32_t rows;         /* rows written this session               */
    uint32_t dropped;      /* rows lost to a full queue this session  */
    uint32_t card_mb;      /* card size, 0 if none                    */
} gz_recon_status_t;

/* Mount the card (if present) and start the writer task. Call once at boot. */
void gz_recon_start(void);

/* Ask the task to start a session, or stop the running one. Returns at once;
 * the result shows up in gz_recon_status() within about half a second. */
void gz_recon_toggle(void);

/* Any task: a consistent copy of the current status. */
void gz_recon_status(gz_recon_status_t *out);
