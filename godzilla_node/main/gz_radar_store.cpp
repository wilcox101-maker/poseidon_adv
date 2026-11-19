/*
 * gz_radar_store.cpp — spinlocked radar table shared by the loop (writer) and
 * the LVGL task (reader). See gz_radar_store.h.
 */
#include "gz_radar_store.h"
#include "gz_recon.h"

#include <freertos/FreeRTOS.h>
#include <string.h>

static gz_radar_table_t s_table;            /* zero-initialised: n = 0 */
static gz_recon_q_t     s_recon_q;
static bool             s_recon_on = false;
static portMUX_TYPE     s_mux = portMUX_INITIALIZER_UNLOCKED;

void gz_radar_feed(uint8_t kind, const uint8_t *entries, uint8_t count, uint32_t now_ms)
{
    if (!entries || count == 0) return;
    if (count > POSEI_DET_MAX) count = POSEI_DET_MAX;
    portENTER_CRITICAL(&s_mux);
    /* Same walk as gz_radar_note_batch(), plus the Recon queue. */
    for (uint8_t k = 0; k < count; ++k) {
        const uint8_t *p = entries + (size_t)k * 10u;
        int i = gz_radar_note(&s_table, kind, p, p[6], (int8_t)p[7], p[8], now_ms);
        if (i < 0 || !s_recon_on) continue;
        gz_radar_row_t *r = &s_table.e[i];
        if (gz_recon_due(r, now_ms) && gz_recon_q_push(&s_recon_q, r)) {
            r->logged = true;
            r->log_ms = now_ms;
        }
    }
    portEXIT_CRITICAL(&s_mux);
}

void gz_radar_store_recon(bool on)
{
    portENTER_CRITICAL(&s_mux);
    if (on) {
        gz_recon_q_clear(&s_recon_q);
        for (int i = 0; i < s_table.n; ++i) s_table.e[i].logged = false;
    }
    s_recon_on = on;
    portEXIT_CRITICAL(&s_mux);
}

int gz_radar_store_recon_pop(gz_radar_row_t *out, int max, uint32_t *dropped)
{
    portENTER_CRITICAL(&s_mux);
    int n = gz_recon_q_pop(&s_recon_q, out, max);
    if (dropped) *dropped = s_recon_q.dropped;
    portEXIT_CRITICAL(&s_mux);
    return n;
}

int gz_radar_snapshot(gz_radar_row_t *out, int max)
{
    portENTER_CRITICAL(&s_mux);
    int n = s_table.n < max ? s_table.n : max;
    memcpy(out, s_table.e, (size_t)n * sizeof(gz_radar_row_t));
    portEXIT_CRITICAL(&s_mux);
    return n;
}
