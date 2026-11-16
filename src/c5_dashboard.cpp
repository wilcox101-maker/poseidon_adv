/*
 * c5_dashboard — shared C5 live-dashboard widget. See c5_dashboard.h.
 *
 * Generalized from the c5_tx_dashboard pattern in features/c5_tier2.cpp:
 * the loop, chrome, radar, ESC handling and metric-region clear are owned
 * here; only the metric fields are feature-specific (the body callback).
 */
#include "c5_dashboard.h"

#include "app.h"
#include "theme.h"
#include "ui.h"
#include "input.h"
#include "c5_cmd.h"
#include <ui_layout.h>

void c5_live_dashboard(const char *title,
                       c5_dash_body_fn body, void *ctx,
                       c5_dash_tick_fn tick,
                       bool stop_on_exit)
{
    auto &d = M5Cardputer.Display;
    ui_clear_body();
    ui_draw_footer("`=stop");

    uint32_t last = 0, last_frames = 0;
    while (true) {
        uint16_t k = input_poll();
        if (k == PK_ESC) {
            if (stop_on_exit) c5_cmd_stop();
            break;
        }
        if (millis() - last > 200) {
            last = millis();

            bool active;
            if (tick) {
                active = tick(ctx);
            } else {
                uint32_t now_frames = c5_status_frames();
                active = (now_frames != last_frames);
                last_frames = now_frames;
            }

            ui_dashboard_chrome(title, active);
            d.fillRect(0, BODY_Y + 14, SCR_W, BODY_H - 28, T_BG);
            if (body) body(ctx);
            ui_dashboard_radar();
        }
        delay(20);
    }
}
