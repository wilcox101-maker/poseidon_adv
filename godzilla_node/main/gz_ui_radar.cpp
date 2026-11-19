/*
 * gz_ui_radar.cpp — round radar of the node's own recent detections (P2b).
 *
 * Geometry and colour rules are the pure, native-tested gz_radar.h: radius
 * from RSSI (strong = centre), a stable hashed bearing per address, and family
 * severity mirroring the Cardputer GODZILLA view (#54) — surveillance / Remote
 * ID red, trackers amber, the rest neutral. The bearing is a layout slot, not
 * a direction: one omni antenna cannot tell where a device is.
 *
 * Objects: three range rings, a crosshair, a slow sweep line, an original
 * trident glyph at the centre (the node), a fixed pool of GZ_RADAR_MAX dots,
 * a footer count line and one detail card. Dots and decorations are not
 * clickable, so taps reach the screen and gz_ui_radar_tap() hit-tests them
 * with a finger-sized tolerance (a 10 px dot is too small to aim at).
 *
 * Recon (P3): a long-press starts or stops a session log on the SD card
 * (gz_recon_task). Hunt (P3): a long-press while a card is open picks that
 * contact for the Hunt screen instead. While one runs, a red "REC n" line sits above the footer;
 * "no SD card" / "SD error" show there for a few seconds when a start fails.
 *
 * Deepwater palette, original artwork only.
 */
#include "gz_ui_radar.h"
#include "gz_radar_store.h"
#include "gz_recon_task.h"
#include "gz_board.h"
#include "gz_geom.h"   /* the one geometry source (#88 §3.2) */

#include <stdio.h>
#include <string.h>

/* Deepwater palette (shared values with gz_ui.cpp) + severity colours. */
#define COL_ABYSS    0x031627
#define COL_RING     0x123F5C
#define COL_TEAL     0x2EC4B6
#define COL_FOAM     0xBDEBEB
#define COL_DIM      0x5E8CA8
#define COL_CARD     0x0A2A40
#define COL_SURVEIL  0xFF4D5A   /* red   — Flock / Raven / Remote ID   */
#define COL_TRACKER  0xFFB020   /* amber — Find My / AirTag / Tile / SmartTag */
#define COL_NEUTRAL  0x7FD1DE   /* pale sea — everything else          */

#define CX        GZ_UI_CX
#define CY        GZ_UI_CY
#define R_MIN     GZ_GEOM_RADAR_R_MIN                     /* clear of the trident */
#define R_MAX     gz_geom_radar_r_max(GZ_UI_W, GZ_UI_H)   /* 158 on the 1.85B      */
#define HIT_TOL   24            /* finger tolerance, px                 */
#define SWEEP_STEP_DEG 9        /* per ~250 ms refresh: ~10 s / turn    */
#define REC_NOTE_MS 4000u       /* how long a failed-start note stays   */

static lv_obj_t *s_dots[GZ_RADAR_MAX];
static uint8_t   s_dot_sev[GZ_RADAR_MAX];     /* last styled severity, 0xFF = none */
static uint8_t   s_dot_opa[GZ_RADAR_MAX];
static lv_obj_t *s_sweep, *s_footer, *s_rec;
static lv_point_precise_t s_sweep_pts[2];
static uint16_t  s_sweep_deg;

static lv_obj_t *s_card, *s_card_fam, *s_card_sig, *s_card_addr, *s_card_age;
static bool      s_card_open;
static uint8_t   s_card_kind;
static uint8_t   s_card_addr_key[6];

/* Snapshot the refresh drew; the tap hit-tests against the same rows. */
static gz_radar_row_t s_rows[GZ_RADAR_MAX];
static int            s_n;

/* ---- helpers ------------------------------------------------------------- */

static uint32_t sev_colour(uint8_t sev)
{
    return sev == GZ_SEV_SURVEIL ? COL_SURVEIL
         : sev == GZ_SEV_TRACKER ? COL_TRACKER : COL_NEUTRAL;
}

static int sev_size(uint8_t sev) { return sev == GZ_SEV_NEUTRAL ? 10 : 14; }

static lv_obj_t *plain_obj(lv_obj_t *parent)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void ring(lv_obj_t *scr, int r)
{
    lv_obj_t *o = plain_obj(scr);
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_pos(o, CX - r, CY - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(COL_RING), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
}

static lv_obj_t *line(lv_obj_t *scr, const lv_point_precise_t *pts, uint32_t n,
                      uint32_t rgb, int width)
{
    lv_obj_t *l = lv_line_create(scr);
    lv_line_set_points(l, pts, n);
    lv_obj_set_style_line_color(l, lv_color_hex(rgb), 0);
    lv_obj_set_style_line_width(l, width, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

/* Original trident glyph, 17 x 24 px, centred on the node position. */
static void trident(lv_obj_t *scr)
{
    static const lv_point_precise_t shaft[] = { { 8, 0 }, { 8, 24 } };
    static const lv_point_precise_t fork[]  = { { 0, 1 }, { 0, 9 }, { 16, 9 }, { 16, 1 } };
    static const lv_point_precise_t barbL[] = { { 0, 1 }, { 2, 4 } };
    static const lv_point_precise_t barbR[] = { { 16, 1 }, { 14, 4 } };
    static const lv_point_precise_t barbM[] = { { 8, 0 }, { 6, 3 } };
    const lv_point_precise_t *parts[] = { shaft, fork, barbL, barbR, barbM };
    const uint32_t counts[] = { 2, 4, 2, 2, 2 };
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *l = line(scr, parts[i], counts[i], COL_TEAL, 2);
        lv_obj_set_pos(l, CX - 8, CY - 12);
    }
}

static lv_obj_t *card_label(const lv_font_t *font, uint32_t rgb)
{
    lv_obj_t *l = lv_label_create(s_card);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(rgb), 0);
    lv_label_set_text(l, "");
    return l;
}

static void fmt_age(uint32_t age_ms, char *buf, size_t n)
{
    uint32_t s = age_ms / 1000u;
    if (s < 60) snprintf(buf, n, "%lus", (unsigned long)s);
    else        snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

/* ---- build --------------------------------------------------------------- */

void gz_ui_radar_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_ABYSS), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ring(scr, R_MAX);
    ring(scr, R_MIN + (R_MAX - R_MIN) * 2 / 3);
    ring(scr, R_MIN + (R_MAX - R_MIN) / 3);

    static const lv_point_precise_t hx[] = { { CX - R_MAX, CY }, { CX + R_MAX, CY } };
    static const lv_point_precise_t vy[] = { { CX, CY - R_MAX }, { CX, CY + R_MAX } };
    line(scr, hx, 2, COL_RING, 1);
    line(scr, vy, 2, COL_RING, 1);

    s_sweep = line(scr, s_sweep_pts, 2, COL_TEAL, 2);
    lv_obj_set_style_line_opa(s_sweep, LV_OPA_50, 0);

    trident(scr);

    for (int i = 0; i < GZ_RADAR_MAX; ++i) {
        lv_obj_t *d = plain_obj(scr);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
        s_dots[i] = d;
        s_dot_sev[i] = 0xFF;
        s_dot_opa[i] = 0;
    }

    s_footer = lv_label_create(scr);
    lv_obj_set_style_text_font(s_footer, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(COL_DIM), 0);
    lv_label_set_text(s_footer, "");
    lv_obj_align(s_footer, LV_ALIGN_BOTTOM_MID, 0, -34);   /* inside the glass */

    s_rec = lv_label_create(scr);
    lv_obj_set_style_text_font(s_rec, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_rec, lv_color_hex(COL_SURVEIL), 0);
    lv_label_set_text(s_rec, "");
    lv_obj_align(s_rec, LV_ALIGN_BOTTOM_MID, 0, -54);

    /* Detail card: hidden until a dot is tapped; any tap closes it. */
    s_card = plain_obj(scr);
    lv_obj_set_size(s_card, gz_geom_card_w(GZ_UI_W), 146);
    lv_obj_center(s_card);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_bg_opa(s_card, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_card, 18, 0);
    lv_obj_set_style_border_width(s_card, 2, 0);
    lv_obj_set_style_pad_all(s_card, 12, 0);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_card, 4, 0);
    s_card_fam  = card_label(&lv_font_montserrat_20, COL_FOAM);
    s_card_sig  = card_label(&lv_font_montserrat_16, COL_FOAM);
    s_card_addr = card_label(&lv_font_montserrat_14, COL_FOAM);
    s_card_age  = card_label(&lv_font_montserrat_14, COL_DIM);
    lv_obj_t *hint = card_label(&lv_font_montserrat_12, COL_DIM);
    lv_label_set_text(hint, "hold: hunt  |  tap: close");
    lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
}

/* ---- refresh ------------------------------------------------------------- */

static void refresh_card(uint32_t now)
{
    if (!s_card_open) return;
    const gz_radar_row_t *r = nullptr;
    for (int i = 0; i < s_n; ++i)
        if (s_rows[i].kind == s_card_kind && memcmp(s_rows[i].addr, s_card_addr_key, 6) == 0) {
            r = &s_rows[i];
            break;
        }
    if (!r) {                       /* evicted from the table */
        lv_label_set_text(s_card_age, "no longer tracked");
        return;
    }
    uint8_t sev = gz_radar_severity(r->family);
    lv_obj_set_style_border_color(s_card, lv_color_hex(sev_colour(sev)), 0);
    lv_obj_set_style_text_color(s_card_fam, lv_color_hex(sev_colour(sev)), 0);
    lv_label_set_text(s_card_fam, gz_radar_family_name(r->family));
    if (r->kind == GZ_RADAR_WIFI)
        lv_label_set_text_fmt(s_card_sig, "Wi-Fi ch %u  |  %d dBm",
                              (unsigned)r->aux, (int)r->rssi);
    else
        lv_label_set_text_fmt(s_card_sig, "BLE %s  |  %d dBm",
                              r->aux ? "random" : "public", (int)r->rssi);
    lv_label_set_text_fmt(s_card_addr, "%02X:%02X:%02X:%02X:%02X:%02X",
                          r->addr[0], r->addr[1], r->addr[2],
                          r->addr[3], r->addr[4], r->addr[5]);
    char age[16];
    fmt_age(now - r->last_seen_ms, age, sizeof age);
    lv_label_set_text_fmt(s_card_age, "seen %s ago  |  %u hits", age, (unsigned)r->hits);
}

void gz_ui_radar_refresh(uint32_t now)
{
    s_n = gz_radar_snapshot(s_rows, GZ_RADAR_MAX);

    int live = 0, red = 0, amber = 0;
    for (int i = 0; i < GZ_RADAR_MAX; ++i) {
        lv_obj_t *d = s_dots[i];
        if (i >= s_n || !gz_radar_live(&s_rows[i], now)) {
            lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const gz_radar_row_t *r = &s_rows[i];
        uint8_t sev = gz_radar_severity(r->family);
        int size = sev_size(sev);
        if (s_dot_sev[i] != sev) {               /* restyle only on change */
            lv_obj_set_style_bg_color(d, lv_color_hex(sev_colour(sev)), 0);
            lv_obj_set_size(d, size, size);
            s_dot_sev[i] = sev;
        }
        uint8_t opa = ((uint32_t)(now - r->last_seen_ms) >= GZ_RADAR_FADE_MS)
                      ? LV_OPA_40 : LV_OPA_COVER;
        if (s_dot_opa[i] != opa) {
            lv_obj_set_style_bg_opa(d, opa, 0);
            s_dot_opa[i] = opa;
        }
        int x, y;
        gz_radar_place(r, CX, CY, R_MIN, R_MAX, &x, &y);
        lv_obj_set_pos(d, x - size / 2, y - size / 2);
        lv_obj_remove_flag(d, LV_OBJ_FLAG_HIDDEN);

        ++live;
        if (sev == GZ_SEV_SURVEIL) ++red;
        else if (sev == GZ_SEV_TRACKER) ++amber;
    }

    /* Sweep: centre -> rim, a step per refresh. Bounding-box positioned so
     * only the line's own strip is invalidated. */
    s_sweep_deg = (uint16_t)((s_sweep_deg + SWEEP_STEP_DEG) % 360u);
    int ex, ey;
    gz_radar_xy(s_sweep_deg, R_MAX - 2, CX, CY, &ex, &ey);
    int x0 = ex < CX ? ex : CX, y0 = ey < CY ? ey : CY;
    s_sweep_pts[0].x = CX - x0; s_sweep_pts[0].y = CY - y0;
    s_sweep_pts[1].x = ex - x0; s_sweep_pts[1].y = ey - y0;
    lv_line_set_points(s_sweep, s_sweep_pts, 2);
    lv_obj_set_pos(s_sweep, x0, y0);

    if (live == 0) lv_label_set_text(s_footer, "listening...");
    else lv_label_set_text_fmt(s_footer, "%d seen | %d red | %d amber", live, red, amber);

    /* Recon line: REC while a session runs, a short note if a start failed. */
    gz_recon_status_t rs;
    gz_recon_status(&rs);
    bool fresh = (uint32_t)(now - rs.state_ms) < REC_NOTE_MS;
    if (rs.state == GZ_RECON_REC) {
        if (rs.dropped) lv_label_set_text_fmt(s_rec, "REC %lu (%lu lost)",
                                              (unsigned long)rs.rows, (unsigned long)rs.dropped);
        else            lv_label_set_text_fmt(s_rec, "REC %lu", (unsigned long)rs.rows);
    } else if (rs.state == GZ_RECON_NO_CARD && fresh) {
        lv_label_set_text(s_rec, "no SD card");
    } else if (rs.state == GZ_RECON_ERROR && fresh) {
        lv_label_set_text(s_rec, "SD error");
    } else if (rs.state == GZ_RECON_IDLE && fresh && rs.rows > 0) {
        lv_label_set_text_fmt(s_rec, "saved %lu rows", (unsigned long)rs.rows);
    } else {
        lv_label_set_text(s_rec, "");
    }

    refresh_card(now);
}

bool gz_ui_radar_long_press(void)
{
    if (s_card_open) {                  /* hold on a card: hunt that contact */
        s_card_open = false;
        lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
        return gz_radar_store_hunt_select(s_card_kind, s_card_addr_key);
    }
    gz_recon_toggle();
    return false;
}

void gz_ui_radar_tap(int32_t x, int32_t y, uint32_t now)
{
    if (s_card_open) {
        s_card_open = false;
        lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    int i = gz_radar_hit(s_rows, s_n, now, CX, CY, R_MIN, R_MAX,
                         (int)x, (int)y, HIT_TOL);
    if (i < 0) return;
    s_card_kind = s_rows[i].kind;
    memcpy(s_card_addr_key, s_rows[i].addr, 6);
    s_card_open = true;
    refresh_card(now);
    lv_obj_remove_flag(s_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_card);
}
