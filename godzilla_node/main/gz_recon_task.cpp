/*
 * gz_recon_task.cpp — SD card + Recon session writer task (P3).
 *
 * Card: SD_MMC 4-bit or SD over SPI, per board, through gz_sd.h (both bundled
 * with the Arduino core, no new library), on the gz_board.h pins. Mounted at boot and, if that failed, tried again
 * whenever a session is asked for — so a card inserted after boot works on the
 * next long-press. format_if_mount_failed is false: this code never formats.
 *
 * Writing: rows are popped from the Recon queue every 500 ms, formatted with
 * the wall time from gz_clock, and written through the open File. The file is
 * flushed every 5 s and on stop, so pulling power loses at most about 5 s of
 * rows. A failed write ends the session and shows ERROR; it is not retried.
 *
 * The task is the only code that touches the card. It runs at priority 1 on
 * core 1 (with the UI); SD waits block only this task, never the publish
 * ticks on the Arduino loop or the radio callbacks.
 *
 * Passive: storage of what the receive-only scanners already saw.
 */
#include "gz_recon_task.h"
#include "gz_recon.h"
#include "gz_radar_store.h"
#include "gz_clock.h"
#include "gz_board.h"

#include <Arduino.h>
/* The card libraries are included HERE, unconditionally, so PlatformIO's LDF
 * (chain mode) sees them: it does not follow includes behind gz_sd.h's board
 * #if, and without these lines the bundled SD_MMC / SD were never linked in
 * (CI: "SD_MMC.h: No such file or directory"). All four ship with the
 * Arduino core on every board; gz_sd.h then picks the one this board uses. */
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <SD_MMC.h>
#include "gz_sd.h"         /* GZ_SD_FS: SDMMC or SPI, per board */

#define RECON_TASK_STACK   6144
#define RECON_TASK_PRIO    1
#define RECON_TASK_CORE    1
#define RECON_POLL_MS      500
#define RECON_FLUSH_MS     5000u
#define RECON_BATCH        16

static portMUX_TYPE       s_mux = portMUX_INITIALIZER_UNLOCKED;
static gz_recon_status_t  s_status;          /* guarded by s_mux */
static volatile bool      s_toggle_req = false;

/* Task-only state. */
static bool     s_mounted = false;
static File     s_file;
static uint32_t s_last_flush_ms;

static void set_state(gz_recon_state_t st)
{
    uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    s_status.state    = st;
    s_status.state_ms = now;
    portEXIT_CRITICAL(&s_mux);
}

static bool try_mount(void)
{
    if (s_mounted) return true;
    /* Never formats. A failed begin() leaves the driver clean to retry. */
    if (!gz_sd_begin() || GZ_SD_FS.cardType() == CARD_NONE) {
        gz_sd_end();
        return false;
    }
    uint32_t mb = (uint32_t)(GZ_SD_FS.cardSize() / (1024ULL * 1024ULL));
    portENTER_CRITICAL(&s_mux);
    s_status.card_mb = mb;
    portEXIT_CRITICAL(&s_mux);
    s_mounted = true;
    Serial.printf("  SD: card mounted, %lu MB (%s)\n", (unsigned long)mb, GZ_SD_BUS_NAME);
    return true;
}

static void session_stop(gz_recon_state_t end_state)
{
    gz_radar_store_recon(false);
    if (s_file) {
        s_file.flush();
        s_file.close();
    }
    set_state(end_state);
}

static void session_start(void)
{
    if (!try_mount()) {
        Serial.println("  recon: no SD card");
        set_state(GZ_RECON_NO_CARD);
        return;
    }
    gz_datetime_t now;
    bool have_time = gz_clock_now(millis(), &now);
    char dir[GZ_RECON_PATH_MAX], path[GZ_RECON_PATH_MAX];
    if (!gz_recon_paths(have_time ? &now : nullptr, dir, sizeof dir, path, sizeof path)) {
        set_state(GZ_RECON_ERROR);
        return;
    }
    if (!GZ_SD_FS.exists("/recon")) GZ_SD_FS.mkdir("/recon");
    if (!GZ_SD_FS.exists(dir))      GZ_SD_FS.mkdir(dir);

    s_file = GZ_SD_FS.open(path, FILE_APPEND);
    if (!s_file) {
        Serial.printf("!! recon: cannot open %s\n", path);
        s_mounted = false;                    /* card pulled? remount next time */
        gz_sd_end();
        set_state(GZ_RECON_ERROR);
        return;
    }
    if (s_file.size() == 0) s_file.print(GZ_RECON_CSV_HEADER);

    portENTER_CRITICAL(&s_mux);
    s_status.rows    = 0;
    s_status.dropped = 0;
    portEXIT_CRITICAL(&s_mux);
    s_last_flush_ms = millis();
    gz_radar_store_recon(true);               /* fresh session: log everything in view */
    set_state(GZ_RECON_REC);
    Serial.printf("  recon: session started -> %s\n", path);
}

/* Write whatever is queued. Returns false if the card refused a write. */
static bool session_drain(void)
{
    gz_radar_row_t rows[RECON_BATCH];
    char line[GZ_RECON_ROW_MAX];
    for (;;) {
        uint32_t dropped = 0;
        int n = gz_radar_store_recon_pop(rows, RECON_BATCH, &dropped);
        uint32_t wrote = 0;
        gz_datetime_t now;
        bool have_time = gz_clock_now(millis(), &now);
        for (int i = 0; i < n; ++i) {
            int len = gz_recon_format_row(line, sizeof line,
                                          have_time ? &now : nullptr, &rows[i]);
            if (len <= 0) continue;
            if (s_file.write((const uint8_t *)line, (size_t)len) != (size_t)len)
                return false;
            ++wrote;
        }
        portENTER_CRITICAL(&s_mux);
        s_status.rows   += wrote;
        s_status.dropped = dropped;
        portEXIT_CRITICAL(&s_mux);
        if (n < RECON_BATCH) break;
    }
    if ((uint32_t)(millis() - s_last_flush_ms) >= RECON_FLUSH_MS) {
        s_file.flush();
        s_last_flush_ms = millis();
    }
    return true;
}

static void recon_task(void *arg)
{
    (void)arg;
    if (!try_mount()) Serial.println("  SD: no card (insert one, then long-press the radar)");

    bool recording = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(RECON_POLL_MS));

        if (s_toggle_req) {
            s_toggle_req = false;
            if (recording) {
                session_drain();
                session_stop(GZ_RECON_IDLE);
                recording = false;
                Serial.println("  recon: session stopped");
            } else {
                session_start();
                gz_recon_status_t st;
                gz_recon_status(&st);
                recording = (st.state == GZ_RECON_REC);
            }
            continue;
        }

        if (recording && !session_drain()) {
            Serial.println("!! recon: SD write failed - session ended");
            session_stop(GZ_RECON_ERROR);
            s_mounted = false;
            gz_sd_end();
            recording = false;
        }
    }
}

void gz_recon_start(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(recon_task, "gz_recon", RECON_TASK_STACK,
                                            nullptr, RECON_TASK_PRIO, nullptr,
                                            RECON_TASK_CORE);
    if (ok != pdPASS) Serial.println("!! recon: task create failed - no SD logging");
}

void gz_recon_toggle(void) { s_toggle_req = true; }

void gz_recon_status(gz_recon_status_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_status;
    portEXIT_CRITICAL(&s_mux);
}
