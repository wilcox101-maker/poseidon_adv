/*
 * flock_frame.h — Flock / surveillance check on one 802.11 management frame.
 *
 * GODZILLA reuse step 3: the frame logic from surveillance_hunter.cpp's
 * promiscuous callback, as a pure function the Cardputer hunter and the
 * GODZILLA node both call. Signatures come from sigdb_surveillance.h.
 *
 * Input: the captured frame and its length INCLUDING the 4-byte FCS (what
 * rx_ctrl.sig_len reports). Handles beacon (8), probe response (5) and probe
 * request (4); anything else returns -1.
 *   - Address: probe request -> addr2 (source STA); beacon/probe resp -> addr3
 *     (BSSID).
 *   - SSID: first tag, only if it is element 0, <= 32 bytes and fully inside
 *     the captured tags.
 *   - Class, exactly as the hunter decided it: vendor prefix (T1/T2) first;
 *     an empty-SSID probe from a listed prefix is SURV_FLOCK_PROBE; an SSID
 *     pattern upgrades UNKNOWN or T2 to SURV_FLOCK_SSID.
 *
 * Wire family (flock_frame_family): FLOCK for T1, SSID and PROBE. A tier-2
 * prefix alone is reported as a plain AP/STA: per sigdb_surveillance.h those
 * are contract manufacturers that need corroboration, and the list includes
 * common module vendors.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sigdb_surveillance.h"
#include "det_classify.h"

typedef struct {
    uint8_t addr[6];
    char    ssid[33];
    uint8_t subtype;     /* 4 probe req, 5 probe resp, 8 beacon */
    uint8_t cls;         /* surv_class_t */
} flock_frame_t;

/* Returns the surv_class_t (SURV_UNKNOWN for an ordinary frame), or -1 if
 * the frame is not a parseable beacon / probe response / probe request. */
static inline int flock_frame_classify(const uint8_t *p, int len, flock_frame_t *r)
{
    if (!p || !r || len < 24 + 4) return -1;
    uint8_t type    = (uint8_t)((p[0] >> 2) & 0x3);
    uint8_t subtype = (uint8_t)((p[0] >> 4) & 0xF);
    if (type != 0) return -1;                                    /* management only */
    if (subtype != 0x8 && subtype != 0x5 && subtype != 0x4) return -1;

    int tag_offset = (subtype == 0x4) ? 24 : 36;
    if (len < tag_offset + 4) return -1;

    memset(r, 0, sizeof(*r));
    r->subtype = subtype;
    memcpy(r->addr, (subtype == 0x4) ? (p + 10) : (p + 16), 6);

    const uint8_t *tags = p + tag_offset;
    int tag_len = len - tag_offset - 4;
    if (tag_len > 1 && tags[0] == 0 && tags[1] <= 32 && tags[1] <= tag_len - 2) {
        memcpy(r->ssid, tags + 2, tags[1]);
        r->ssid[tags[1]] = '\0';
    }

    surv_class_t cls = flock_classify_oui(r->addr);
    if (subtype == 0x4 && r->ssid[0] == '\0' && cls != SURV_UNKNOWN)
        cls = SURV_FLOCK_PROBE;
    if (cls == SURV_UNKNOWN || cls == SURV_FLOCK_T2) {
        surv_class_t s = flock_classify_ssid(r->ssid);
        if (s != SURV_UNKNOWN) cls = s;
    }
    r->cls = (uint8_t)cls;
    return (int)cls;
}

/* Wire family for a classified frame (see header note on tier 2). */
static inline uint8_t flock_frame_family(const flock_frame_t *r)
{
    if (r->cls == SURV_FLOCK_T1 || r->cls == SURV_FLOCK_SSID || r->cls == SURV_FLOCK_PROBE)
        return DET_FAM_FLOCK;
    return (r->subtype == 0x4) ? DET_FAM_WIFI_STA : DET_FAM_WIFI_AP;
}
