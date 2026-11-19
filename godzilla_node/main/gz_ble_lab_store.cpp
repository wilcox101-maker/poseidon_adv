/*
 * gz_ble_lab_store.cpp — spinlocked BLE Lab table. See gz_ble_lab_store.h.
 */
#include "gz_ble_lab_store.h"

#include <freertos/FreeRTOS.h>
#include <string.h>

static gz_lab_table_t s_table;              /* zero-initialised: n = 0 */
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;

void gz_ble_lab_feed(const uint8_t addr[6], uint8_t atype, int8_t rssi,
                     const uint8_t *payload, int len, uint32_t now_ms)
{
    if (!addr || (len > 0 && !payload)) return;
    portENTER_CRITICAL(&s_mux);
    gz_lab_note(&s_table, addr, atype, rssi, payload, len, now_ms);
    portEXIT_CRITICAL(&s_mux);
}

int gz_ble_lab_snapshot(gz_lab_row_t *out, int max)
{
    portENTER_CRITICAL(&s_mux);
    int n = s_table.n < max ? s_table.n : max;
    memcpy(out, s_table.e, (size_t)n * sizeof(gz_lab_row_t));
    portEXIT_CRITICAL(&s_mux);
    return n;
}
