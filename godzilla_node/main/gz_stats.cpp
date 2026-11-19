/*
 * gz_stats.cpp — spinlock-guarded owner of the status-screen counters (P2a).
 */
#include "gz_stats.h"

#include <Arduino.h>
#include <string.h>

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static gz_status_t  s_status;
static bool         s_inited = false;

static void ensure_init(void)
{
    if (!s_inited) { gz_status_init(&s_status); s_inited = true; }
}

void gz_stats_set_static(const char *node_name, uint8_t link_channel,
                         uint32_t psram_kb, uint8_t i2c_found)
{
    portENTER_CRITICAL(&s_mux);
    ensure_init();
    strncpy(s_status.node_name, node_name, sizeof(s_status.node_name) - 1);
    s_status.node_name[sizeof(s_status.node_name) - 1] = '\0';
    s_status.link_channel = link_channel;
    s_status.psram_kb     = psram_kb;
    s_status.i2c_found    = i2c_found;
    portEXIT_CRITICAL(&s_mux);
}

void gz_stats_note_publish(gz_radio_t radio, uint8_t entries)
{
    uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    ensure_init();
    gz_status_note_publish(&s_status, radio, entries, now);
    portEXIT_CRITICAL(&s_mux);
}

void gz_stats_snapshot(gz_status_t *out)
{
    portENTER_CRITICAL(&s_mux);
    ensure_init();
    *out = s_status;
    portEXIT_CRITICAL(&s_mux);
}
