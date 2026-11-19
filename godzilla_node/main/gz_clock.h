/*
 * gz_clock.h — the node's wall clock, shared across tasks (P3).
 *
 * The LVGL task owns the RTC (gz_ui_watch.cpp reads and sets the PCF85063 and
 * keeps the software clock). Other tasks — the Recon writer — need the time
 * too, but must not add a second I2C reader. So the UI publishes its clock
 * base here after every sync, and anyone may read the current time from it.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gz_watch.h"

/* UI task: publish the software clock after a sync or a manual set. */
void gz_clock_publish(const gz_watch_clock_t *c);

/* Any task: current wall time. False until the first publish. */
bool gz_clock_now(uint32_t now_ms, gz_datetime_t *out);
