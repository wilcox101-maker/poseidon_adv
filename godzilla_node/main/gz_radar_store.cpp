/*
 * gz_radar_store.cpp — spinlocked radar table shared by the loop (writer) and
 * the LVGL task (reader). See gz_radar_store.h.
 */
#include "gz_radar_store.h"

#include <freertos/FreeRTOS.h>
#include <string.h>

static gz_radar_table_t s_table;            /* zero-initialised: n = 0 */
static portMUX_TYPE     s_mux = portMUX_INITIALIZER_UNLOCKED;

void gz_radar_feed(uint8_t kind, const uint8_t *entries, uint8_t count, uint32_t now_ms)
{
    if (!entries || count == 0) return;
    portENTER_CRITICAL(&s_mux);
    gz_radar_note_batch(&s_table, kind, entries, count, now_ms);
    portEXIT_CRITICAL(&s_mux);
}

int gz_radar_snapshot(gz_radar_row_t *out, int max)
{
    portENTER_CRITICAL(&s_mux);
    int n = s_table.n < max ? s_table.n : max;
    memcpy(out, s_table.e, (size_t)n * sizeof(gz_radar_row_t));
    portEXIT_CRITICAL(&s_mux);
    return n;
}
