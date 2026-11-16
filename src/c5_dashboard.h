/*
 * c5_dashboard — shared C5 live-dashboard widget.
 *
 * Standardizes the "poll a C5 RESP_STATUS stream and draw a live screen
 * until ESC" pattern that every C5 feature screen repeats (frame count +
 * channel, dashboard chrome + radar, ESC-to-stop). A new C5 feature
 * supplies only its metric-area draw callback; the widget owns the body
 * clear, the "`=stop" footer, the poll loop, the ~200 ms tick, the
 * activity pulse and teardown.
 *
 * Usage (e.g. a future diagnostics / client-tracking screen):
 *
 *     static void my_body(void *ctx) {
 *         auto &d = M5Cardputer.Display;
 *         d.setTextColor(T_FG, T_BG);
 *         d.setCursor(4, BODY_Y + 16);
 *         d.printf("peers: %d", (int)(intptr_t)ctx);
 *         // ... draw this feature's live fields ...
 *     }
 *     void feat_c5_mything(void) {
 *         // send the start command, then:
 *         c5_live_dashboard("C5 MY THING", my_body, (void*)n, nullptr, true);
 *     }
 *
 * loopTask only (it draws). The body callback likewise runs on loopTask
 * and must not block or draw the status bar (single-display-owner rule).
 */
#pragma once

#include <Arduino.h>

/* Draws the feature's metric fields into the body each ~200 ms tick,
 * after the metric region (BODY_Y+14 .. footer) has been cleared. Read
 * live state via the c5_status_*() accessors or feature globals; `ctx`
 * carries per-call data, so no captures or file-statics are needed. */
typedef void (*c5_dash_body_fn)(void *ctx);

/* Optional activity probe for the chrome pulse: return true when there is
 * new activity this tick. Pass nullptr to use the built-in heuristic (the
 * RESP_STATUS frame counter advanced since the last tick). */
typedef bool (*c5_dash_tick_fn)(void *ctx);

/* Run a C5 live dashboard until the user presses ESC:
 *   - clears the body and draws the "`=stop" footer once,
 *   - every ~200 ms: draws ui_dashboard_chrome(title, active), clears the
 *     metric region, calls body(ctx), draws the radar,
 *   - on ESC: calls c5_cmd_stop() when stop_on_exit, then returns.
 * `tick` decides the chrome pulse (nullptr => frame-counter heuristic). */
void c5_live_dashboard(const char *title,
                       c5_dash_body_fn body, void *ctx,
                       c5_dash_tick_fn tick,
                       bool stop_on_exit);
