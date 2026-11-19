/*
 * proto.h — POSEIDON ESP-NOW wire protocol v5.
 *
 * Shared between the S3 (POSEIDON) and the C5 (this node). This file and
 * src/c5_cmd.h must stay byte-identical for the message header + payload
 * structs; the native proto test (test/test_proto) pins that for the C5 side
 * and the static_asserts in both files pin it per-build.
 *
 * v5 vs v4 (ESP-NOW v2 flag day — raises the payload ceiling):
 *   - POSEI_PAYLOAD_MAX 230 → 1024. ESP-NOW v2 (auto-negotiated on ESP-IDF
 *     >= 5.4, which both chips run) lifts the single-frame cap from 250 to
 *     ESP_NOW_MAX_DATA_LEN_V2 (1490) bytes, so the whole posei_msg_t now fits
 *     one frame. Unblocks untruncated EAPOL/WPA3-SAE handshakes, raw-frame
 *     forwarding, and many more records per RESP batch.
 *   - posei_msg_t is now VARIABLE LENGTH on the wire: payload_len moved ahead
 *     of payload and widened u8 → u16, and senders transmit only
 *     POSEI_HEADER_SZ + payload_len bytes (posei_wire_len()). Raising the
 *     ceiling therefore does NOT bloat small control frames — a PONG is still
 *     ~10 bytes on air, only large responses use the headroom.
 *   - POSEI_VERSION 4 → 5.
 *
 * v3→v4 history: RESP_* renumbered 20-25 → 40-45; +10 CMD types; +4 RESP
 * types; posei_hs_t slimmed to 174B. Wardrive intentionally still NOT in.
 *
 * FLAG DAY: both sides must run v5 together. A v4 peer is rejected on the
 * version check (on_recv), not misrouted.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <assert.h>   /* static_assert: C11 macro here, keyword in C++ — proto.h
                       * is compiled as C (c5_node) and as C++ (test/test_proto) */

#define POSEI_MAGIC   0x504F5345  /* "POSE" */
#define POSEI_VERSION 5

enum {
    POSEI_TYPE_HELLO              = 1,

    /* Commands: S3 → C5 (10–29) */
    POSEI_TYPE_CMD_PING           = 10,
    POSEI_TYPE_CMD_SCAN_5G        = 11,
    POSEI_TYPE_CMD_SCAN_ZB        = 12,
    POSEI_TYPE_CMD_SCAN_2G        = 13,
    POSEI_TYPE_CMD_DEAUTH         = 14,
    POSEI_TYPE_CMD_STOP           = 15,
    POSEI_TYPE_CMD_PMKID          = 16,
    POSEI_TYPE_CMD_HS_CAPTURE     = 17,
    POSEI_TYPE_CMD_CLIENTS_HUNT   = 18,  /* all-channel STA hunt           */
    POSEI_TYPE_CMD_CLIENTS_AP     = 19,  /* per-AP STA list (channel-lock) */
    POSEI_TYPE_CMD_BEACON_SPAM    = 20,  /* 5G beacon spam                 */
    POSEI_TYPE_CMD_PROBE_SNIFF    = 21,  /* probe-req logger               */
    POSEI_TYPE_CMD_DEAUTH_DETECT  = 22,  /* passive deauth-frame detector  */
    POSEI_TYPE_CMD_KARMA          = 23,  /* sniff probe → SoftAP that SSID */
    POSEI_TYPE_CMD_APCLONE        = 24,  /* manual SoftAP with given SSID  */
    POSEI_TYPE_CMD_SPECTRUM       = 25,  /* per-channel peak RSSI sweep    */
    POSEI_TYPE_CMD_CIW            = 26,  /* SSID-injection beacon spam     */

    /* Responses: C5 → S3 (40–49) */
    POSEI_TYPE_RESP_PONG          = 40,
    POSEI_TYPE_RESP_AP            = 41,
    POSEI_TYPE_RESP_ZB            = 42,
    POSEI_TYPE_RESP_STATUS        = 43,
    POSEI_TYPE_RESP_PMKID         = 44,
    POSEI_TYPE_RESP_HS            = 45,
    POSEI_TYPE_RESP_STA           = 46,  /* one STA-BSSID pair             */
    POSEI_TYPE_RESP_PROBE         = 47,  /* one probe request              */
    POSEI_TYPE_RESP_DEAUTH_HIT    = 48,  /* one observed deauth frame      */
    POSEI_TYPE_RESP_SPECTRUM      = 49,  /* batched per-channel peak RSSI  */

    /* GODZILLA node detection publish (60–69). A full-UI 2.4 GHz sensor node
     * (Waveshare 1.85B) pushes its own Wi-Fi/BLE detections to the S3 for
     * aggregation. These are ADDITIVE: the C5 never emits them, so its on-air
     * byte stream is unchanged (approach B — header/version frozen, no flag
     * day). node_id travels in the payload, not the header. */
    POSEI_TYPE_RESP_WIFI_DET      = 60,  /* GODZILLA→S3: Wi-Fi detection batch */
    POSEI_TYPE_RESP_BLE_DET       = 61,  /* GODZILLA→S3: BLE detection batch   */
};

/* Classifier family — shared vocabulary for detection-publish entries. Numeric
 * values are wire state; append only, never renumber. */
enum {
    POSEI_DET_FAM_UNKNOWN     = 0,
    POSEI_DET_FAM_WIFI_AP     = 1,
    POSEI_DET_FAM_WIFI_STA    = 2,
    POSEI_DET_FAM_BLE_GENERIC = 3,
    POSEI_DET_FAM_FINDMY      = 4,
    POSEI_DET_FAM_AIRTAG      = 5,
    POSEI_DET_FAM_IBEACON     = 6,
    POSEI_DET_FAM_REMOTEID    = 7,
    POSEI_DET_FAM_FLOCK       = 8,
};

/* v5: 1024 (was 230). The whole frame (POSEI_HEADER_SZ + this) must stay
 * within ESP-NOW v2's ESP_NOW_MAX_DATA_LEN_V2 (1490); 1024 leaves comfortable
 * margin and is plenty for untruncated EAPOL, raw mgmt frames, and large RESP
 * batches. Can be raised toward ~1480 later if a need appears. */
#define POSEI_PAYLOAD_MAX 1024

/* v5 wire layout: payload_len moved ahead of payload and widened to u16 so the
 * frame is variable length — senders transmit only POSEI_HEADER_SZ +
 * payload_len bytes (posei_wire_len), not the whole struct. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;
    uint16_t seq;
    uint16_t payload_len;
    uint8_t  payload[POSEI_PAYLOAD_MAX];
} posei_msg_t;

/* Fixed header size (everything before payload) = 10 bytes. */
#define POSEI_HEADER_SZ  (offsetof(posei_msg_t, payload))

/* Bytes actually put on air for a message: header + used payload only. */
static inline size_t posei_wire_len(const posei_msg_t *m)
{
    uint16_t pl = m->payload_len;
    if (pl > POSEI_PAYLOAD_MAX) pl = POSEI_PAYLOAD_MAX;
    return POSEI_HEADER_SZ + pl;
}

static_assert(POSEI_HEADER_SZ == 10, "v5 header must be 10 bytes");
/* ESP_NOW_MAX_DATA_LEN_V2 == 1490 in ESP-IDF; assert against the literal so
 * this header stays includable without esp_now.h (native test / S3 side). */
static_assert(sizeof(posei_msg_t) <= 1490, "posei_msg_t must fit one ESP-NOW v2 frame");

/* ---- existing payloads (unchanged) ---- */

typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  auth;
    uint8_t  is_5g;       /* 0 = 2.4, 1 = 5, 2 = 6 */
    char     ssid[33];
} posei_ap_t;

typedef struct __attribute__((packed)) {
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  frame_type;
    uint16_t pan_id;
    uint16_t src_short;
    uint16_t dst_short;
    uint8_t  seq;
} posei_zb_t;

typedef struct __attribute__((packed)) {
    char     name[12];
    uint32_t heap_kb;
    uint8_t  role;
    uint8_t  has_5g;
    uint8_t  has_ieee802154;
} posei_hello_t;

typedef struct __attribute__((packed)) {
    uint16_t duration_ms;
} posei_scan_req_t;

typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint8_t  bcast_all;
    uint16_t duration_ms;
} posei_deauth_req_t;

typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
} posei_pmkid_req_t;

typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  sta[6];
    uint8_t  pmkid[16];
    uint8_t  ssid_len;
    char     ssid[33];
} posei_pmkid_t;

typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
} posei_hs_req_t;

/* v4: slimmed to what a hashcat-22000 line actually needs. The MIC, SNonce and
 * replay counter are all recoverable from eapol_m2, and the S3 supplies the
 * ESSID from its scan target — so those fields were pure wire redundancy that
 * pushed the struct past the 230-byte ESP-NOW payload. */
typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  sta[6];
    uint8_t  anonce[32];
    uint16_t eapol_m2_len;
    uint8_t  eapol_m2[128];   /* full M2 802.1X frame (from the 802.1X header) */
} posei_hs_t;                 /* 174 bytes */
static_assert(sizeof(posei_hs_t) <= POSEI_PAYLOAD_MAX, "posei_hs_t must fit payload");

/* ---- NEW payloads (v3) ---- */

/* Clients hunt request. target_bssid=00..00 → hunt every AP.
 * hop_all=1 → cycle 36..165 on 5 GHz; =0 → channel-lock to `channel`. */
typedef struct __attribute__((packed)) {
    uint8_t  target_bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  hop_all;
} posei_clients_req_t;

/* One STA-BSSID observation. */
typedef struct __attribute__((packed)) {
    uint8_t  sta[6];
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
} posei_sta_t;

/* One probe request. */
typedef struct __attribute__((packed)) {
    uint8_t  src[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  ssid_len;
    char     ssid[33];
} posei_probe_t;

/* One observed deauth/disassoc frame. */
typedef struct __attribute__((packed)) {
    uint8_t  src[6];
    uint8_t  dst[6];
    uint8_t  channel;
    uint8_t  reason;
} posei_deauth_hit_t;

/* Beacon spam request.
 *   mode 0 = built-in meme SSID list (firmware-side strings)
 *   mode 1 = single rickroll loop
 *   mode 2 = caller-supplied SSIDs from ssids[] array
 *   ssid_n up to 5 caller SSIDs (further can be sent across multiple reqs) */
typedef struct __attribute__((packed)) {
    uint8_t  channel;          /* 36..165 5G, or 1..13 2G */
    uint16_t duration_ms;      /* 0 = run until STOP */
    uint8_t  mode;
    uint8_t  ssid_n;
    char     ssids[5][33];
} posei_beacon_spam_req_t;

typedef struct __attribute__((packed)) {
    uint8_t  channel;          /* 0 = hop, else lock */
    uint16_t duration_ms;
} posei_probe_sniff_req_t;

typedef struct __attribute__((packed)) {
    uint8_t  channel;
    uint16_t duration_ms;
} posei_deauth_detect_req_t;

/* Karma: sniff probe, spin up SoftAP on requested SSID. */
typedef struct __attribute__((packed)) {
    uint8_t  channel;          /* SoftAP channel */
    uint16_t duration_ms;      /* 0 = until STOP (5-min safety) */
} posei_karma_req_t;

/* Manual AP clone — SoftAP with user-supplied SSID. */
typedef struct __attribute__((packed)) {
    uint8_t  channel;
    char     ssid[33];
    uint8_t  open;             /* 1 = open auth, 0 = WPA2 (psk in pass) */
    char     pass[33];
} posei_apclone_req_t;

/* Per-channel peak RSSI batch. Spectrum sweep streams these every
 * ~200 ms. ch_count up to 24 channels per batch (UNII-1+2A+3 = 24). */
typedef struct __attribute__((packed)) {
    uint8_t  ch_count;
    struct __attribute__((packed)) {
        uint8_t ch;
        int8_t  peak_rssi;
    } ch[24];
} posei_spectrum_t;

typedef struct __attribute__((packed)) {
    uint16_t duration_ms;      /* 0 = until STOP */
} posei_spectrum_req_t;

/* CIW (SSID-injection beacon spam). Built-in payload categories
 * selected by `category` index, firmware iterates payloads. */
typedef struct __attribute__((packed)) {
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  category;         /* 0=cmd-inject, 1=fmt-string, 2=log4j, 3=xss, 4=path-trav, ... */
    uint16_t interval_ms;      /* SSID rotation cadence */
} posei_ciw_req_t;

/* ---- wire-size guards (every payload struct must fit one frame) ---- */
static_assert(sizeof(posei_ap_t)              <= POSEI_PAYLOAD_MAX, "posei_ap_t");
static_assert(sizeof(posei_zb_t)              <= POSEI_PAYLOAD_MAX, "posei_zb_t");
static_assert(sizeof(posei_hello_t)           <= POSEI_PAYLOAD_MAX, "posei_hello_t");
static_assert(sizeof(posei_scan_req_t)        <= POSEI_PAYLOAD_MAX, "posei_scan_req_t");
static_assert(sizeof(posei_deauth_req_t)      <= POSEI_PAYLOAD_MAX, "posei_deauth_req_t");
static_assert(sizeof(posei_pmkid_req_t)       <= POSEI_PAYLOAD_MAX, "posei_pmkid_req_t");
static_assert(sizeof(posei_pmkid_t)           <= POSEI_PAYLOAD_MAX, "posei_pmkid_t");
static_assert(sizeof(posei_hs_req_t)          <= POSEI_PAYLOAD_MAX, "posei_hs_req_t");
static_assert(sizeof(posei_clients_req_t)     <= POSEI_PAYLOAD_MAX, "posei_clients_req_t");
static_assert(sizeof(posei_sta_t)             <= POSEI_PAYLOAD_MAX, "posei_sta_t");
static_assert(sizeof(posei_probe_t)           <= POSEI_PAYLOAD_MAX, "posei_probe_t");
static_assert(sizeof(posei_deauth_hit_t)      <= POSEI_PAYLOAD_MAX, "posei_deauth_hit_t");
static_assert(sizeof(posei_beacon_spam_req_t) <= POSEI_PAYLOAD_MAX, "posei_beacon_spam_req_t");
static_assert(sizeof(posei_probe_sniff_req_t) <= POSEI_PAYLOAD_MAX, "posei_probe_sniff_req_t");
static_assert(sizeof(posei_deauth_detect_req_t)<= POSEI_PAYLOAD_MAX, "posei_deauth_detect_req_t");
static_assert(sizeof(posei_karma_req_t)       <= POSEI_PAYLOAD_MAX, "posei_karma_req_t");
static_assert(sizeof(posei_apclone_req_t)     <= POSEI_PAYLOAD_MAX, "posei_apclone_req_t");
static_assert(sizeof(posei_spectrum_t)        <= POSEI_PAYLOAD_MAX, "posei_spectrum_t");
static_assert(sizeof(posei_spectrum_req_t)    <= POSEI_PAYLOAD_MAX, "posei_spectrum_req_t");
static_assert(sizeof(posei_ciw_req_t)         <= POSEI_PAYLOAD_MAX, "posei_ciw_req_t");

/* ---- GODZILLA detection-publish payloads (multi-node, approach B) ---- */

/* Max detection entries per batch. 64 * 10B + 2B header = 642B, well inside
 * POSEI_PAYLOAD_MAX; a node with more sends several batches. */
#define POSEI_DET_MAX 64

/* One observed Wi-Fi device (AP or STA). 10 bytes, fixed. */
typedef struct __attribute__((packed)) {
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  family;     /* POSEI_DET_FAM_* */
    uint8_t  flags;      /* reserved, 0 in P0 */
} posei_wifi_det_entry_t;

/* One observed BLE device. 10 bytes, fixed. */
typedef struct __attribute__((packed)) {
    uint8_t  addr[6];
    uint8_t  addr_type;  /* 0 = public, 1 = random */
    int8_t   rssi;
    uint8_t  family;     /* POSEI_DET_FAM_* */
    uint8_t  flags;      /* reserved, 0 in P0 */
} posei_ble_det_entry_t;

/* Wi-Fi detection batch: publishing node_id, count, then `count` entries. */
typedef struct __attribute__((packed)) {
    uint8_t  node_id;
    uint8_t  count;
    posei_wifi_det_entry_t det[POSEI_DET_MAX];
} posei_wifi_det_t;

/* BLE detection batch. */
typedef struct __attribute__((packed)) {
    uint8_t  node_id;
    uint8_t  count;
    posei_ble_det_entry_t det[POSEI_DET_MAX];
} posei_ble_det_t;

static_assert(sizeof(posei_wifi_det_entry_t) == 10, "wifi det entry 10B");
static_assert(sizeof(posei_ble_det_entry_t)  == 10, "ble det entry 10B");
/* Literal size pins both headers to the same number (c5_cmd.h mirrors it). */
static_assert(sizeof(posei_wifi_det_t) == 2 + POSEI_DET_MAX * 10, "posei_wifi_det_t size");
static_assert(sizeof(posei_ble_det_t)  == 2 + POSEI_DET_MAX * 10, "posei_ble_det_t size");
static_assert(sizeof(posei_wifi_det_t) <= POSEI_PAYLOAD_MAX, "posei_wifi_det_t fits frame");
static_assert(sizeof(posei_ble_det_t)  <= POSEI_PAYLOAD_MAX, "posei_ble_det_t fits frame");

/* Bytes actually used by a batch of `count` entries — set payload_len to this
 * so only the populated prefix rides the wire. Clamps an over-max count. */
static inline size_t posei_wifi_det_bytes(uint8_t count)
{
    if (count > POSEI_DET_MAX) count = POSEI_DET_MAX;
    return offsetof(posei_wifi_det_t, det) +
           (size_t)count * sizeof(posei_wifi_det_entry_t);
}
static inline size_t posei_ble_det_bytes(uint8_t count)
{
    if (count > POSEI_DET_MAX) count = POSEI_DET_MAX;
    return offsetof(posei_ble_det_t, det) +
           (size_t)count * sizeof(posei_ble_det_entry_t);
}

/* ---- proto.c API ---- */
void proto_init_msg(posei_msg_t *m, uint8_t type);
void proto_send_broadcast(const posei_msg_t *m);
void proto_send_to(const uint8_t mac[6], const posei_msg_t *m);
int  proto_validate(const uint8_t *data, int len, posei_msg_t *out);
