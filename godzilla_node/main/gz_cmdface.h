/*
 * gz_cmdface.h — Command-face model: pod bindings, round-safe pod geometry and
 * the feature-shortcut quadrant map (watch-faces orders, unit 3).
 *
 * The PURE half of the Command face (central detection radar + analog hands,
 * four complication pods, feature-shortcut tap zones, status ticker):
 *   - the pod -> live-field binding table: which of the four pods shows which
 *     complication, and which `lv_subject` that complication honestly reads.
 *     The "navigation" pod is IMU tilt on an IMU board and Wi-Fi channel
 *     activity otherwise (orders' honest-bindings table) — never GPS, never a
 *     bearing/DOA, which the node cannot source;
 *   - round-safe pod placement: the four pods sit in the four corners of the
 *     screen's safe area — the inscribed square of a round panel, an inset
 *     rectangle otherwise — so a pod of radius `pod_r` never clips the glass on
 *     the 1.85B (round) or the rectangular 2.06 / ES3C28P;
 *   - the quadrant feature-shortcut map: a tap maps to one of four features
 *     (radar / hunt / BLE-lab / status) by which quadrant of the dial it fell
 *     in, so the face can hand the UI a screen to open.
 *
 * Geometry comes from the one source, gz_geom.h (#88 §3.2); the field/pod model
 * builds on the unit-1 capability enums in ui/faces/gz_faces.h. Pure: the pod
 * and quadrant functions take plain (w, h, round) / (x, y) ints, so the host
 * test checks all three boards without a board header, exactly like
 * test_godzilla_geom. No LVGL, no Arduino. Compiles as C and C++.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gz_geom.h"            /* gz_geom_safe_r — the one geometry source */
#include "ui/faces/gz_faces.h" /* gz_comp_id_t, GZ_CAP_IMU, GZ_FACE_SLOTS   */

/* ---- live fields a pod can read ------------------------------------------ */
/* The subjects published by unit 1 (gz_face_subjects.h), as an id so the pure
 * binding table names a field without depending on LVGL. GZ_SUBJ_NONE is an
 * unbound pod (e.g. a complication whose only source is a string, or none). */
typedef enum {
    GZ_SUBJ_NONE = 0,
    GZ_SUBJ_HEAP_KB,
    GZ_SUBJ_CONTACTS,
    GZ_SUBJ_RSSI_BEST,
    GZ_SUBJ_PUB_AGE_S,
    GZ_SUBJ_CHANNEL,
    GZ_SUBJ_IMU_TILT,
    GZ_SUBJ_BATT_PCT,
    GZ_SUBJ_RECON_ON,
} gz_subj_field_t;

/* The honest field a complication reads (its primary live value). A
 * complication the node can only source as a string (DECODE) or from the RTC
 * (CLOCK) has no int subject here, so it binds to GZ_SUBJ_NONE. */
static inline gz_subj_field_t gz_comp_subject(gz_comp_id_t c)
{
    switch (c) {
    case GZ_COMP_STATUS:   return GZ_SUBJ_HEAP_KB;
    case GZ_COMP_CONTACTS: return GZ_SUBJ_CONTACTS;
    case GZ_COMP_PUBLISH:  return GZ_SUBJ_PUB_AGE_S;
    case GZ_COMP_CHANNEL:  return GZ_SUBJ_CHANNEL;
    case GZ_COMP_IMU:      return GZ_SUBJ_IMU_TILT;
    case GZ_COMP_BATTERY:  return GZ_SUBJ_BATT_PCT;
    case GZ_COMP_RECON:    return GZ_SUBJ_RECON_ON;
    default:               return GZ_SUBJ_NONE;   /* NONE / DECODE / CLOCK */
    }
}

/* ---- the Command face's four default pods -------------------------------- */
/* Pods 0..2 are fixed all-board complications; pod 3 is the navigation pod —
 * IMU tilt where the board has an IMU, Wi-Fi channel activity otherwise. The
 * result is always a complication this board can source (gz_comp_available),
 * so the face never shows a faked pod. */
static inline gz_comp_id_t gz_cmd_pod_comp(int slot, uint8_t board_caps)
{
    gz_comp_id_t c;
    switch (slot) {
    case 0:  c = GZ_COMP_STATUS;   break;
    case 1:  c = GZ_COMP_CONTACTS; break;
    case 2:  c = GZ_COMP_PUBLISH;  break;
    case 3:  c = (board_caps & GZ_CAP_IMU) ? GZ_COMP_IMU : GZ_COMP_CHANNEL; break;
    default: return GZ_COMP_NONE;
    }
    return gz_comp_available(c, board_caps) ? c : GZ_COMP_NONE;
}

/* The field pod `slot` reads on this board (binding-table shortcut). */
static inline gz_subj_field_t gz_cmd_pod_field(int slot, uint8_t board_caps)
{
    return gz_comp_subject(gz_cmd_pod_comp(slot, board_caps));
}

/* ---- round-safe pod placement -------------------------------------------- */
/* Half-width of the largest axis-aligned box that fits the safe area: the
 * inscribed square's half-side (safe_r / sqrt(2)) on a round panel, else the
 * panel half less a margin. Integer; sqrt(2) ~ 1.41421356 is 4142/2929. */
static inline int gz_cmd_safe_half(int w, int h, bool round, int margin)
{
    if (round) {
        int r = gz_geom_safe_r(w, h);          /* min(w,h)/2 */
        return (r * 2929) / 4142;              /* r / sqrt(2), rounded down  */
    }
    int half = gz_geom_min(w, h) / 2;          /* short-axis half */
    return half - margin;
}

/* Centre of pod `i` (0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right),
 * inset so a pod of radius `pod_r` stays inside the safe area. The pods share
 * the dial centre (w/2, h/2). `margin` is the edge keep-out on a rect panel. */
static inline void gz_cmd_pod_pos(int i, int w, int h, bool round, int pod_r,
                                  int margin, int *x, int *y)
{
    int d = gz_cmd_safe_half(w, h, round, margin) - pod_r;
    if (d < 0) d = 0;
    /* On a round panel a corner pod sits at distance sqrt(2)*d from centre; cap
     * d so sqrt(2)*d + pod_r still fits the safe radius. */
    if (round) {
        int lim = gz_geom_safe_r(w, h) - pod_r;
        int reach = (d * 4142) / 2929;         /* sqrt(2)*d */
        if (reach > lim) d = (lim * 2929) / 4142;
        if (d < 0) d = 0;
    }
    int cx = w / 2, cy = h / 2;
    *x = (i & 1) ? cx + d : cx - d;            /* right on odd i  */
    *y = (i & 2) ? cy + d : cy - d;            /* bottom on i>=2  */
}

/* True iff a pod of radius `pod_r` centred at (x, y) lies wholly inside the
 * safe area — the safe circle on a round panel, the inset rect otherwise. Used
 * by the native test to prove no pod clips on any board. */
static inline bool gz_cmd_pod_inside(int x, int y, int pod_r, int w, int h,
                                     bool round, int margin)
{
    if (round) {
        int cx = w / 2, cy = h / 2;
        long dx = x - cx, dy = y - cy;
        long reach = (long)(gz_geom_safe_r(w, h) - pod_r);
        return dx * dx + dy * dy <= reach * reach;
    }
    return x - pod_r >= margin && x + pod_r <= w - margin &&
           y - pod_r >= margin && y + pod_r <= h - margin;
}

/* ---- feature-shortcut quadrant map --------------------------------------- */
/* The four features a quadrant tap opens. No GPS, no bearing — the node has
 * neither (orders). */
typedef enum {
    GZ_CMD_FEAT_RADAR  = 0,   /* top-left                                   */
    GZ_CMD_FEAT_HUNT   = 1,   /* top-right                                  */
    GZ_CMD_FEAT_LAB    = 2,   /* bottom-left  (BLE lab)                     */
    GZ_CMD_FEAT_STATUS = 3,   /* bottom-right                               */
} gz_cmd_feature_t;

/* Which feature a tap at (x, y) opens, by the quadrant of the dial centre it
 * fell in. A tap exactly on a centre line resolves up/left. */
static inline gz_cmd_feature_t gz_cmd_quadrant(int x, int y, int cx, int cy)
{
    int right = (x > cx) ? 1 : 0;
    int down  = (y > cy) ? 1 : 0;
    return (gz_cmd_feature_t)((down << 1) | right);
}
