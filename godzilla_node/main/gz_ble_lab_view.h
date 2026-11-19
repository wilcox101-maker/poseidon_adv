/*
 * gz_ble_lab_view.h — layout and text for the BLE Lab screen (#98 §2.4b).
 * Pure: no LVGL, host-tested by test/test_godzilla_ble_lab_view. The screen
 * itself (gz_ui_lab.cpp) only places these strings.
 *
 * List: the recent-advert table, strongest first, one row per device:
 * "-55  BT Keybd | keyboard | HID", or the address when the advert carries
 * nothing nameable. Tap a row for its decoded detail; tap again to go back.
 *
 * Geometry is on the 360 design canvas like the other screens: y positions
 * take GZ_UI_DY (gz_geom.h) so the block sits centred on every panel. On the
 * round 1.85B the list stops early enough that every row's full width stays
 * inside the glass.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "gz_ble_adv.h"
#include "gz_ble_lab.h"

#define GZ_LAB_VIEW_MAX   12     /* most rows any panel shows          */
#define GZ_LAB_ROW_H      24     /* montserrat 14 + gap                */
#define GZ_LAB_TITLE_Y    36     /* design canvas; add GZ_UI_DY        */
#define GZ_LAB_LIST_Y     68

static inline int gz_lab_dy(int h) { return (h - 360) / 2; }

static inline int gz_lab_list_top(int h) { return GZ_LAB_LIST_Y + gz_lab_dy(h); }

/* The hint line under the list. */
static inline int gz_lab_hint_y(int h, bool round) { return round ? h - 58 : h - 24; }

/* Rows that fit between the list top and the hint line, 1..GZ_LAB_VIEW_MAX. */
static inline int gz_lab_rows_fit(int h, bool round)
{
    int bottom = round ? h - 60 : h - 30;
    int r = (bottom - gz_lab_list_top(h)) / GZ_LAB_ROW_H;
    if (r < 1) r = 1;
    if (r > GZ_LAB_VIEW_MAX) r = GZ_LAB_VIEW_MAX;
    return r;
}

/* Which shown row a tap at screen y hits: 0..shown-1, or -1. */
static inline int gz_lab_row_hit(int y, int h, int shown)
{
    int top = gz_lab_list_top(h);
    if (y < top) return -1;
    int i = (y - top) / GZ_LAB_ROW_H;
    return i < shown ? i : -1;
}

static inline void gz_lab_fmt_addr(const uint8_t a[6], char *out, size_t n)
{
    snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}

static inline const char *gz_lab_atype_name(uint8_t t)
{
    switch (t) {                    /* NimBLE / HCI address types */
    case 0: return "public";
    case 1: return "random";
    case 2: return "public (RPA)";
    case 3: return "random (RPA)";
    default: return "other";
    }
}

/* One list row. Returns true when the device announces HID. */
static inline bool gz_lab_row_text(const gz_lab_row_t *r, char *out, size_t n)
{
    gz_adv_t a;
    gz_adv_parse(r->payload, r->len, &a);
    char s[96];
    gz_adv_summary(&a, s, sizeof s);
    if (strcmp(s, "(no data)") == 0) gz_lab_fmt_addr(r->addr, s, sizeof s);
    snprintf(out, n, "%4d  %s", (int)r->rssi, s);
    return gz_adv_is_hid(&a);
}

/* Index of `addr` in rows[0..n), or -1. */
static inline int gz_lab_find(const gz_lab_row_t *rows, int n, const uint8_t addr[6])
{
    for (int i = 0; i < n; ++i) if (memcmp(rows[i].addr, addr, 6) == 0) return i;
    return -1;
}

static inline void gz_lab_fmt_age(uint32_t age_ms, char *buf, size_t n)
{
    uint32_t s = age_ms / 1000u;
    if (s < 60) snprintf(buf, n, "%lus", (unsigned long)s);
    else        snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

/* The detail page: one line per decoded field, '\n'-separated. Lines for
 * fields the advert does not carry are left out. */
static inline void gz_lab_detail_text(const gz_lab_row_t *r, uint32_t now_ms, char *out, size_t n)
{
    gz_adv_t a;
    gz_adv_parse(r->payload, r->len, &a);
    size_t k = 0;
#define GZ_LAB_LINE(...) do { if (k < n) { \
        int w_ = snprintf(out + k, n - k, __VA_ARGS__); \
        if (w_ > 0) k += (size_t)w_; \
        if (k >= n) k = n - 1; } } while (0)
    out[0] = '\0';
    char addr[18], age[16];
    gz_lab_fmt_addr(r->addr, addr, sizeof addr);
    gz_lab_fmt_age(now_ms - r->last_ms, age, sizeof age);
    GZ_LAB_LINE("%s\n%s\n", addr, gz_lab_atype_name(r->atype));
    GZ_LAB_LINE("%d dBm | seen %u | %s ago", (int)r->rssi, (unsigned)r->seen, age);
    if (a.has_name)
        GZ_LAB_LINE("\nname: %s%s", a.name, a.name_complete ? "" : " (short)");
    if (a.has_appearance) {
        const char *s = gz_adv_appearance_name(a.appearance);
        if (s) GZ_LAB_LINE("\nkind: %s", s);
        else   GZ_LAB_LINE("\nkind: 0x%04X", (unsigned)a.appearance);
    }
    if (gz_adv_is_hid(&a)) GZ_LAB_LINE("\nannounces HID");
    if (a.n_uuid16 || a.n_uuid32 || a.n_uuid128) {
        GZ_LAB_LINE("\nsvc:");
        for (int i = 0; i < a.n_uuid16; ++i) {
            const char *s = gz_adv_uuid16_name(a.uuid16[i]);
            if (s) GZ_LAB_LINE(" %s", s);
            else   GZ_LAB_LINE(" %04X", (unsigned)a.uuid16[i]);
        }
        if (a.n_uuid32)  GZ_LAB_LINE(" +%u x32", (unsigned)a.n_uuid32);
        if (a.n_uuid128) GZ_LAB_LINE(" +%u x128", (unsigned)a.n_uuid128);
    }
    if (a.has_svc_data) {
        const char *s = gz_adv_uuid16_name(a.svc_data_uuid);
        if (s) GZ_LAB_LINE("\ndata: %s, %u B", s, (unsigned)a.svc_data_len);
        else   GZ_LAB_LINE("\ndata: %04X, %u B", (unsigned)a.svc_data_uuid, (unsigned)a.svc_data_len);
    }
    if (a.has_mfr) {
        const char *c = gz_adv_company_name(a.company);
        if (c) GZ_LAB_LINE("\nmaker: %s, %u B", c, (unsigned)a.mfr_len);
        else   GZ_LAB_LINE("\nmaker: %04X, %u B", (unsigned)a.company, (unsigned)a.mfr_len);
    }
    if (a.has_tx)    GZ_LAB_LINE("\ntx power: %d dBm", (int)a.tx_dbm);
    if (a.has_flags) GZ_LAB_LINE("\nflags: %02X", (unsigned)a.flags);
    GZ_LAB_LINE("\n%u B%s%s", (unsigned)r->len, r->truncated ? " (cut)" : "",
                a.malformed ? " | malformed" : "");
#undef GZ_LAB_LINE
}
