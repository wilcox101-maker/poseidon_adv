/*
 * gz_ui_alert.cpp — priority ALERT overlay (detection §4 item 6). See
 * gz_ui_alert.h. The decision logic is the pure gz_alert.h; this draws the
 * overlay and keeps the per-device re-alert cool-down.
 *
 * Layout is sized from gz_geom.h (#88 §3.2), so it is shape-safe on the round
 * 1.85B, the 410x502 AMOLED-2.06 and the 240x320 ES3C28P. [render] — Claudius
 * render-verifies the three boards.
 */
#include "gz_ui_alert.h"
#include "gz_alert.h"          /* pure gating + priority + rate-limit helpers */
#include "gz_radar_store.h"    /* gz_radar_snapshot */
#include "gz_radar.h"          /* severity, family name, row geometry */
#include "gz_geom.h"           /* the one geometry source */

#include <stdio.h>
#include <string.h>

/* Deepwater palette + severity colours (shared values with gz_ui_radar.cpp). */
#define COL_ABYSS    0x031627
#define COL_FOAM     0xBDEBEB
#define COL_DIM      0x5E8CA8
#define COL_INK      0x02101C
#define COL_SURVEIL  0xFF4D5A   /* red   — surveillance / recording */
#define COL_TRACKER  0xFFB020   /* amber — trackers                 */

/* Overlay objects (children of the top layer). */
static lv_obj_t *s_panel;          /* full-screen cover, hidden when idle */
static lv_obj_t *s_bar;            /* severity header strip               */
static lv_obj_t *s_bar_lbl;        /* "PRIORITY ALERT"                    */
static lv_obj_t *s_fam;            /* family name (large)                 */
static lv_obj_t *s_sev;            /* SURVEILLANCE / TRACKER              */
static lv_obj_t *s_addr;           /* MAC                                 */
static lv_obj_t *s_sig;            /* RSSI + channel                      */
static lv_obj_t *s_hint;           /* "tap to dismiss"                    */
static lv_obj_t *s_mini;           /* mini radar ring                     */
static lv_obj_t *s_mini_dot;       /* the one device dot                  */

static bool     s_open;
static uint32_t s_raised_ms;
static uint8_t  s_dev_kind;
static uint8_t  s_dev_addr[6];

/* Per-device re-alert cool-down table (gz_alert_rate_ok): remember when each
 * device was last raised so the same one cannot re-pop within the window. */
#define RL_SLOTS 8
static struct { uint8_t kind, addr[6]; uint32_t last_ms; bool used; } s_rl[RL_SLOTS];

static bool rl_recent(uint8_t kind, const uint8_t addr[6], uint32_t now_ms)
{
    for (int i = 0; i < RL_SLOTS; ++i)
        if (s_rl[i].used && s_rl[i].kind == kind &&
            memcmp(s_rl[i].addr, addr, 6) == 0)
            return !gz_alert_rate_ok(s_rl[i].last_ms, now_ms, /*ever=*/true);
    return false;   /* never alerted -> not rate-limited */
}

static void rl_record(uint8_t kind, const uint8_t addr[6], uint32_t now_ms)
{
    /* Reuse this device's slot, else a free slot, else evict the oldest. */
    int slot = -1;
    for (int i = 0; i < RL_SLOTS; ++i)
        if (s_rl[i].used && s_rl[i].kind == kind &&
            memcmp(s_rl[i].addr, addr, 6) == 0) { slot = i; break; }
    if (slot < 0)
        for (int i = 0; i < RL_SLOTS; ++i)
            if (!s_rl[i].used) { slot = i; break; }
    if (slot < 0) {
        slot = 0;
        uint32_t oldest = now_ms - s_rl[0].last_ms;
        for (int i = 1; i < RL_SLOTS; ++i) {
            uint32_t age = now_ms - s_rl[i].last_ms;
            if (age > oldest) { oldest = age; slot = i; }
        }
    }
    s_rl[slot].used = true;
    s_rl[slot].kind = kind;
    memcpy(s_rl[slot].addr, addr, 6);
    s_rl[slot].last_ms = now_ms;
}

/* ---- helpers ------------------------------------------------------------- */
static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, uint32_t rgb)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, "");
    return l;
}

static uint32_t sev_colour(uint8_t family)
{
    return gz_radar_severity(family) == GZ_SEV_SURVEIL ? COL_SURVEIL : COL_TRACKER;
}

/* ---- build --------------------------------------------------------------- */
void gz_ui_alert_build(void)
{
    lv_obj_t *top = lv_layer_top();

    s_panel = plain_obj(top);
    lv_obj_set_size(s_panel, GZ_UI_W, GZ_UI_H);
    lv_obj_set_pos(s_panel, 0, 0);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    /* Severity header strip across the top of the content band. */
    int band_y = gz_geom_content_dy(GZ_UI_H);
    if (band_y < 0) band_y = 0;
    s_bar = plain_obj(s_panel);
    lv_obj_set_size(s_bar, GZ_UI_W, GZ_UI_S(34));
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(26));
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(COL_SURVEIL), 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    s_bar_lbl = mk_label(s_bar, &lv_font_montserrat_14, COL_INK);
    lv_label_set_text(s_bar_lbl, "PRIORITY ALERT");
    lv_obj_center(s_bar_lbl);

    /* Family + severity word, centred. */
    s_fam = mk_label(s_panel, &lv_font_montserrat_18, COL_FOAM);
    lv_obj_align(s_fam, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(74));
    s_sev = mk_label(s_panel, &lv_font_montserrat_14, COL_SURVEIL);
    lv_obj_align(s_sev, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(100));

    /* Identity lines. */
    s_addr = mk_label(s_panel, &lv_font_montserrat_14, COL_FOAM);
    lv_obj_align(s_addr, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(128));
    s_sig = mk_label(s_panel, &lv_font_montserrat_12, COL_DIM);
    lv_obj_align(s_sig, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(150));

    /* Mini radar: a ring with one dot for the device's hashed bearing. */
    int mr = GZ_UI_S(34);
    s_mini = plain_obj(s_panel);
    lv_obj_set_size(s_mini, 2 * mr, 2 * mr);
    lv_obj_align(s_mini, LV_ALIGN_TOP_MID, 0, band_y + GZ_UI_S(180));
    lv_obj_set_style_radius(s_mini, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_mini, 2, 0);
    lv_obj_set_style_border_color(s_mini, lv_color_hex(COL_DIM), 0);
    lv_obj_set_style_border_opa(s_mini, LV_OPA_COVER, 0);
    s_mini_dot = plain_obj(s_mini);
    lv_obj_set_size(s_mini_dot, GZ_UI_S(8), GZ_UI_S(8));
    lv_obj_set_style_radius(s_mini_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_mini_dot, LV_OPA_COVER, 0);

    s_hint = mk_label(s_panel, &lv_font_montserrat_12, COL_DIM);
    lv_label_set_text(s_hint, "tap to dismiss");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -GZ_UI_S(18));
}

/* ---- show / hide --------------------------------------------------------- */
static void populate(const gz_radar_row_t *r)
{
    uint32_t col = sev_colour(r->family);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(col), 0);
    lv_obj_set_style_text_color(s_sev, lv_color_hex(col), 0);

    lv_label_set_text(s_fam, gz_radar_family_name(r->family));
    lv_label_set_text(s_sev, gz_radar_severity(r->family) == GZ_SEV_SURVEIL
                                 ? "SURVEILLANCE" : "TRACKER NEARBY");

    char buf[32];
    snprintf(buf, sizeof buf, "%02X:%02X:%02X:%02X:%02X:%02X",
             r->addr[0], r->addr[1], r->addr[2], r->addr[3], r->addr[4], r->addr[5]);
    lv_label_set_text(s_addr, buf);

    if (r->kind == GZ_RADAR_WIFI)
        snprintf(buf, sizeof buf, "%d dBm  ·  CH %u", (int)r->rssi, (unsigned)r->aux);
    else
        snprintf(buf, sizeof buf, "%d dBm  ·  BLE", (int)r->rssi);
    lv_label_set_text(s_sig, buf);

    /* Place the mini-radar dot by the device's stable bearing + RSSI radius. */
    int mr = GZ_UI_S(34);
    int rr = gz_radar_radius(r->rssi, GZ_UI_S(6), mr - GZ_UI_S(6));
    int dx, dy;
    gz_radar_xy(gz_radar_angle(r->kind, r->addr), rr, mr, mr, &dx, &dy);
    lv_obj_set_style_bg_color(s_mini_dot, lv_color_hex(col), 0);
    lv_obj_set_pos(s_mini_dot, dx - GZ_UI_S(4), dy - GZ_UI_S(4));
}

static void show(const gz_radar_row_t *r, bool always_on, uint32_t now_ms)
{
    populate(r);
    s_dev_kind = r->kind;
    memcpy(s_dev_addr, r->addr, 6);
    s_raised_ms = now_ms;
    s_open = true;
    /* Dimmed in always-on so it does not glare on a persistent screen. */
    lv_obj_set_style_bg_opa(s_panel, always_on ? LV_OPA_70 : LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
}

static void hide(void)
{
    s_open = false;
    if (s_panel) lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
}

/* ---- tick ---------------------------------------------------------------- */
void gz_ui_alert_tick(uint16_t alert_mask, bool always_on, uint32_t now_ms)
{
    if (!s_panel) return;

    static gz_radar_row_t rows[GZ_RADAR_MAX];
    int n = gz_radar_snapshot(rows, GZ_RADAR_MAX);
    int sel = gz_alert_select(rows, n, alert_mask, now_ms);

    if (s_open) {
        /* Auto-clear after the TTL even if the device is still present. */
        if (gz_alert_expired(s_raised_ms, now_ms)) { hide(); return; }
        /* Keep the current device's readout fresh while it is still selected. */
        if (sel >= 0 && rows[sel].kind == s_dev_kind &&
            memcmp(rows[sel].addr, s_dev_addr, 6) == 0) {
            populate(&rows[sel]);
            lv_obj_set_style_bg_opa(s_panel, always_on ? LV_OPA_70 : LV_OPA_COVER, 0);
        }
        return;
    }

    /* Don't re-pop a device still inside its cool-down (e.g. just dismissed),
     * but don't let it mask a different eligible device either: skip cooling
     * rows and take the next best. */
    static uint8_t skip[GZ_RADAR_MAX];
    for (int i = 0; i < n; ++i) skip[i] = rl_recent(rows[i].kind, rows[i].addr, now_ms) ? 1u : 0u;
    sel = gz_alert_select_skip(rows, n, alert_mask, now_ms, skip);
    if (sel < 0) return;
    rl_record(rows[sel].kind, rows[sel].addr, now_ms);
    show(&rows[sel], always_on, now_ms);
}

bool gz_ui_alert_tap(void)
{
    if (!s_open) return false;
    /* Record the dismiss time so it stays down for the cool-down. */
    rl_record(s_dev_kind, s_dev_addr, lv_tick_get());
    hide();
    return true;
}

bool gz_ui_alert_active(void) { return s_open; }
