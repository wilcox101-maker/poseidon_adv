/*
 * gz_clock.cpp — spinlocked copy of the UI's software clock. See gz_clock.h.
 */
#include "gz_clock.h"

#include <freertos/FreeRTOS.h>

static gz_watch_clock_t s_clock;            /* zero-initialised: not synced */
static portMUX_TYPE     s_mux = portMUX_INITIALIZER_UNLOCKED;

void gz_clock_publish(const gz_watch_clock_t *c)
{
    portENTER_CRITICAL(&s_mux);
    s_clock = *c;
    portEXIT_CRITICAL(&s_mux);
}

bool gz_clock_now(uint32_t now_ms, gz_datetime_t *out)
{
    gz_watch_clock_t c;
    portENTER_CRITICAL(&s_mux);
    c = s_clock;
    portEXIT_CRITICAL(&s_mux);
    if (!c.synced) return false;
    gz_watch_now(&c, now_ms, out);          /* calendar maths outside the lock */
    return true;
}
