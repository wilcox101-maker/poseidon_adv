/*
 * gz_faces.h — watch-faces registry model for the GODZILLA node UI
 * (watch-faces orders, unit 1).
 *
 * This is the PURE half of the faces subsystem: enums, the capability model,
 * the persisted settings struct, and the inline logic that the firmware glue
 * (gz_face_subjects.*, gz_face_store.*) and the faces themselves build on —
 *   - which faces / complications a board may show (capability filter),
 *   - which face to select at boot (requested, else first that fits),
 *   - the NVS settings codec (serialize -> deserialize == identity; an
 *     unknown, short or corrupt blob falls back to defaults),
 *   - sanitising a loaded settings struct against the board's real caps so a
 *     slot a board can't source is dropped, never faked.
 *
 * Pure: plain C, no LVGL, no NVS, no Arduino — so the whole model is exercised
 * on the host for all three boards in one translation unit
 * (test/test_godzilla_faces), exactly as gz_geom.h / gz_status.h are. The
 * board's own capability mask is built from the GZ_BOARD_HAS_* macros by
 * GZ_FACE_BOARD_CAPS below; the pure functions take a mask argument so the
 * host test can check every board without a board header.
 *
 * Honest data only: a complication whose source a board lacks is hidden. The
 * binding table lives in the orders (docs/proposals/2026-11-19-godzilla-
 * watch-faces-orders.md); gz_comp_caps[] below is its machine form.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---- capability mask ----------------------------------------------------- */
/* One bit per sourced subsystem. Radios (BLE/Wi-Fi), the publish path and the
 * Wi-Fi channel are present on every board, so they need no bit. */
enum {
    GZ_CAP_RTC   = 1u << 0,   /* PCF85063 real-time clock (clock/date/moon) */
    GZ_CAP_IMU   = 1u << 1,   /* QMI8658 tilt / rotation                    */
    GZ_CAP_BATT  = 1u << 2,   /* a battery readout (gauge, PMIC or ADC)     */
    GZ_CAP_SD    = 1u << 3,   /* TF slot + Recon writer                     */
};

/* ---- faces --------------------------------------------------------------- */
typedef enum {
    GZ_FACE_CLASSIC = 0,      /* antique analog dial (RTC parts gated)      */
    GZ_FACE_COMMAND = 1,      /* detection radar + complication pods        */
    GZ_FACE_MODULAR = 2,      /* 2x2 slot grid over a digital time          */
    GZ_FACE_MINIMAL = 3,      /* big time + one status line                 */
    GZ_FACE_COUNT   = 4,
} gz_face_id_t;

/* Caps a face needs just to appear. Dale's rule (2026-11-20): "no RTC, no clock
 * faces." CLASSIC (antique analog dial), MODULAR (digital time + slot grid) and
 * MINIMAL (big time + status line) are clocks — each is gated out whole on a
 * board without a GZ_CAP_RTC, not shown with the time blanked and no soft clock.
 * COMMAND (detection radar + complication pods) is not a clock, needs no RTC, and
 * stays on every board — so every board always has at least one available face.
 * (Per-element GZ_COMP_CLOCK gating inside a non-clock face is separate, in
 * gz_comp_caps[].) */
static const uint8_t gz_face_caps[GZ_FACE_COUNT] = {
    /* CLASSIC */ GZ_CAP_RTC,
    /* COMMAND */ 0,
    /* MODULAR */ GZ_CAP_RTC,
    /* MINIMAL */ GZ_CAP_RTC,
};

/* ---- complications ------------------------------------------------------- */
typedef enum {
    GZ_COMP_NONE      = 0,    /* empty slot                                 */
    GZ_COMP_STATUS    = 1,    /* heap / PSRAM / uptime           (all)      */
    GZ_COMP_CONTACTS  = 2,    /* contact counts + strongest RSSI (all)      */
    GZ_COMP_PUBLISH   = 3,    /* BLE/Wi-Fi sent + last-pub age   (all)      */
    GZ_COMP_CHANNEL   = 4,    /* Wi-Fi hop channel 1-13          (all)      */
    GZ_COMP_IMU       = 5,    /* tilt / orientation              (IMU)      */
    GZ_COMP_BATTERY   = 6,    /* battery %/V                     (BATT)     */
    GZ_COMP_RECON     = 7,    /* recording on/off -> SD file     (SD)       */
    GZ_COMP_DECODE    = 8,    /* last iBeacon/Eddystone/vendor   (all)      */
    GZ_COMP_CLOCK     = 9,    /* digital time                    (RTC)      */
    GZ_COMP_COUNT     = 10,
} gz_comp_id_t;

/* Caps each complication needs (0 = available on every board). Machine form of
 * the orders' honest-bindings table. */
static const uint8_t gz_comp_caps[GZ_COMP_COUNT] = {
    /* NONE     */ 0,
    /* STATUS   */ 0,
    /* CONTACTS */ 0,
    /* PUBLISH  */ 0,
    /* CHANNEL  */ 0,
    /* IMU      */ GZ_CAP_IMU,
    /* BATTERY  */ GZ_CAP_BATT,
    /* RECON    */ GZ_CAP_SD,
    /* DECODE   */ 0,
    /* CLOCK    */ GZ_CAP_RTC,
};

/* ---- accents / background ------------------------------------------------ */
#define GZ_ACCENT_COUNT 9     /* nine accent colours in the customiser      */
typedef enum {
    GZ_BG_BLACK   = 0,
    GZ_BG_TEXTURE = 1,
    GZ_BG_COUNT   = 2,
} gz_bg_t;

/* ---- persisted settings -------------------------------------------------- */
#define GZ_FACE_SLOTS 4       /* four complication pods on the slotted faces */

typedef struct {
    uint8_t face;                 /* gz_face_id_t                           */
    uint8_t accent;               /* 0 .. GZ_ACCENT_COUNT-1                 */
    uint8_t background;           /* gz_bg_t                                */
    uint8_t always_on;            /* 0 / 1                                  */
    uint8_t slots[GZ_FACE_SLOTS]; /* gz_comp_id_t per pod                   */
} gz_ui_settings_t;

/* Factory defaults: the Classic face, first accent, black ground, always-on
 * off, and the four all-board complications in the pods. */
static inline void gz_ui_settings_defaults(gz_ui_settings_t *s)
{
    s->face       = GZ_FACE_CLASSIC;
    s->accent     = 0;
    s->background  = GZ_BG_BLACK;
    s->always_on  = 0;
    s->slots[0]   = GZ_COMP_STATUS;
    s->slots[1]   = GZ_COMP_CONTACTS;
    s->slots[2]   = GZ_COMP_PUBLISH;
    s->slots[3]   = GZ_COMP_CHANNEL;
}

/* ---- capability filter --------------------------------------------------- */
/* A face/complication is allowed on a board iff the board sources every cap it
 * needs. */
static inline bool gz_caps_ok(uint8_t need, uint8_t board_caps)
{
    return (need & board_caps) == need;
}

static inline bool gz_face_available(gz_face_id_t f, uint8_t board_caps)
{
    if ((unsigned)f >= GZ_FACE_COUNT) return false;
    return gz_caps_ok(gz_face_caps[f], board_caps);
}

static inline bool gz_comp_available(gz_comp_id_t c, uint8_t board_caps)
{
    if ((unsigned)c >= GZ_COMP_COUNT) return false;
    if (c == GZ_COMP_NONE) return true;           /* an empty slot is always ok */
    return gz_caps_ok(gz_comp_caps[c], board_caps);
}

/* ---- registry selection -------------------------------------------------- */
/* The face to show: the requested one if it exists and fits this board, else
 * the first registered face that fits. CLASSIC fits only RTC boards now, so the
 * walk lands on COMMAND (caps 0) on an RTC-less board — the default face there.
 * Returns a valid gz_face_id_t. */
static inline gz_face_id_t gz_face_select(int requested, uint8_t board_caps)
{
    if (requested >= 0 && requested < GZ_FACE_COUNT &&
        gz_face_available((gz_face_id_t)requested, board_caps))
        return (gz_face_id_t)requested;
    for (int f = 0; f < GZ_FACE_COUNT; ++f)
        if (gz_face_available((gz_face_id_t)f, board_caps))
            return (gz_face_id_t)f;
    return GZ_FACE_COMMAND;                        /* unreachable: COMMAND (caps 0) always fits */
}

/* Drop any choice this board can't honour: a face it lacks -> gz_face_select;
 * a slot complication it can't source -> GZ_COMP_NONE; out-of-range accent /
 * background -> default. Leaves a struct safe to render with no faked data. */
static inline void gz_ui_settings_sanitise(gz_ui_settings_t *s, uint8_t board_caps)
{
    s->face = (uint8_t)gz_face_select(s->face, board_caps);
    if (s->accent >= GZ_ACCENT_COUNT) s->accent = 0;
    if (s->background >= GZ_BG_COUNT) s->background = GZ_BG_BLACK;
    s->always_on = s->always_on ? 1 : 0;
    for (int i = 0; i < GZ_FACE_SLOTS; ++i)
        if (!gz_comp_available((gz_comp_id_t)s->slots[i], board_caps))
            s->slots[i] = GZ_COMP_NONE;
}

/* ---- NVS settings codec -------------------------------------------------- */
/* A tiny fixed blob: magic 'G', a version byte, then the five fields. Keeping
 * it fixed-length lets deserialise reject anything that isn't exactly this
 * shape and fall back to defaults, so an old or corrupt blob never half-loads. */
#define GZ_SETTINGS_MAGIC   0x47u   /* 'G' */
#define GZ_SETTINGS_VERSION 1u

/* Layout (10 bytes): [0]=magic [1]=version [2]=face [3]=accent [4]=background
 * [5]=always_on [6..9]=slots[0..3]. */
#define GZ_SETTINGS_WIRE_LEN (6 + GZ_FACE_SLOTS)

/* Serialise into out[GZ_SETTINGS_WIRE_LEN]. Returns the byte count written. */
static inline int gz_ui_settings_encode(const gz_ui_settings_t *s, uint8_t *out)
{
    out[0] = GZ_SETTINGS_MAGIC;
    out[1] = GZ_SETTINGS_VERSION;
    out[2] = s->face;
    out[3] = s->accent;
    out[4] = s->background;
    out[5] = s->always_on ? 1u : 0u;
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) out[6 + i] = s->slots[i];
    return GZ_SETTINGS_WIRE_LEN;
}

/* Deserialise `len` bytes at `in`. On a good blob fills *s and returns true; on
 * any mismatch (null, wrong length, bad magic or version) fills defaults and
 * returns false. Field values are copied verbatim — call gz_ui_settings_sanitise
 * against the board caps afterwards. */
static inline bool gz_ui_settings_decode(const uint8_t *in, int len, gz_ui_settings_t *s)
{
    if (!in || len != GZ_SETTINGS_WIRE_LEN ||
        in[0] != GZ_SETTINGS_MAGIC || in[1] != GZ_SETTINGS_VERSION) {
        gz_ui_settings_defaults(s);
        return false;
    }
    s->face       = in[2];
    s->accent     = in[3];
    s->background = in[4];
    s->always_on  = in[5] ? 1u : 0u;
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) s->slots[i] = in[6 + i];
    return true;
}

/* The firmware's board capability mask (built from GZ_BOARD_HAS_* at the
 * selected board) lives in gz_face_caps.h, which includes gz_board.h first so
 * the macro is defined whatever the include order. This header stays pure and
 * board-agnostic: the host test supplies masks by hand. */
