/*
 * c5_cmd — POSEIDON-side C5 remote-radio client.
 *
 * Sits on top of the existing ESP-NOW mesh. When a C5 node is seen
 * (HELLO with role=1 / has_5g=1), we auto-add it to the peer table
 * and expose an API for the UI to send commands and collect streamed
 * responses.
 *
 * Wire protocol v5 — shared with c5_node/main/proto.h, kept byte-identical.
 * FLAG DAY: both sides must run v5 together (ESP-NOW v2 variable-length
 * frames; a v4 peer is rejected on the version check). GODZILLA detection-
 * publish types (60–61) are additive — the C5's on-air format is unchanged.
 */
#pragma once

#include <Arduino.h>

#define C5_MAGIC   0x504F5345
#define C5_VERSION 5

/* HELLO roles. 1 = C5/TRIDENT command node. 2 = GODZILLA passive detection
 * node: it publishes types 60/61 and takes no commands, so it is listed in
 * Diagnostics but never counted as "a C5 online" nor sent commands. */
#define C5_ROLE_C5            1
#define C5_ROLE_DETECT_NODE   2

enum {
    C5_TYPE_HELLO              = 1,

    /* Commands: S3 → C5 (10–29) */
    C5_TYPE_CMD_PING           = 10,
    C5_TYPE_CMD_SCAN_5G        = 11,
    C5_TYPE_CMD_SCAN_ZB        = 12,
    C5_TYPE_CMD_SCAN_2G        = 13,
    C5_TYPE_CMD_DEAUTH         = 14,
    C5_TYPE_CMD_STOP           = 15,
    C5_TYPE_CMD_PMKID          = 16,
    C5_TYPE_CMD_HS             = 17,    /* matches POSEI_TYPE_CMD_HS_CAPTURE (c5_node/main/proto.h) — net-002 */
    C5_TYPE_CMD_CLIENTS_HUNT   = 18,
    C5_TYPE_CMD_CLIENTS_AP     = 19,
    C5_TYPE_CMD_BEACON_SPAM    = 20,
    C5_TYPE_CMD_PROBE_SNIFF    = 21,
    C5_TYPE_CMD_DEAUTH_DETECT  = 22,
    C5_TYPE_CMD_KARMA          = 23,
    C5_TYPE_CMD_APCLONE        = 24,
    C5_TYPE_CMD_SPECTRUM       = 25,
    C5_TYPE_CMD_CIW            = 26,
    C5_TYPE_CMD_TIMESET        = 27,   /* S3→node: set wall-clock (epoch+tz) */

    /* Responses: C5 → S3 (40–49) */
    C5_TYPE_RESP_PONG          = 40,
    C5_TYPE_RESP_AP            = 41,
    C5_TYPE_RESP_ZB            = 42,
    C5_TYPE_RESP_STATUS        = 43,
    C5_TYPE_RESP_PMKID         = 44,
    C5_TYPE_RESP_HS            = 45,
    C5_TYPE_RESP_STA           = 46,
    C5_TYPE_RESP_PROBE         = 47,
    C5_TYPE_RESP_DEAUTH_HIT    = 48,
    C5_TYPE_RESP_SPECTRUM      = 49,

    /* GODZILLA node detection publish (60–69) — mirrors POSEI_TYPE_RESP_*
     * in c5_node/main/proto.h. The S3 aggregates these; the C5 never sends
     * them, so its on-air format is unchanged (approach B). */
    C5_TYPE_RESP_WIFI_DET      = 60,
    C5_TYPE_RESP_BLE_DET       = 61,
};

/* Classifier family — must stay byte-identical to POSEI_DET_FAM_* in proto.h.
 * Numeric values are wire state; append only, never renumber. */
enum {
    C5_DET_FAM_UNKNOWN     = 0,
    C5_DET_FAM_WIFI_AP     = 1,
    C5_DET_FAM_WIFI_STA    = 2,
    C5_DET_FAM_BLE_GENERIC = 3,
    C5_DET_FAM_FINDMY      = 4,
    C5_DET_FAM_AIRTAG      = 5,
    C5_DET_FAM_IBEACON     = 6,
    C5_DET_FAM_REMOTEID    = 7,
    C5_DET_FAM_FLOCK       = 8,
    C5_DET_FAM_RAVEN       = 9,   /* Raven / ShotSpotter (BLE)     */
    C5_DET_FAM_TILE        = 10,  /* Tile tracker (BLE)            */
    C5_DET_FAM_SMARTTAG    = 11,  /* Samsung SmartTag (BLE)        */
    C5_DET_FAM_SKIMMER   = 12,  /* Bluetooth card skimmer (BLE/SPP)*/
    C5_DET_FAM_CAMERA    = 13,  /* covert / IP camera (Wi-Fi OUI) */
    C5_DET_FAM_LE_EQUIP  = 14,  /* LE body-cam / equipment (low-conf.)*/
    C5_DET_FAM_ALPR      = 15,  /* non-Flock ALPR (low-conf.)     */
    C5_DET_FAM_SMARTGLASS= 16,  /* recording smart glasses (BLE)  */
};

/* ---- existing request/response structs (unchanged shape) ---- */

struct __attribute__((packed)) c5_deauth_req_t {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint8_t  bcast_all;
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_pmkid_req_t {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_pmkid_t {
    uint8_t  bssid[6];
    uint8_t  sta[6];
    uint8_t  pmkid[16];
    uint8_t  ssid_len;
    char     ssid[33];
};

struct __attribute__((packed)) c5_hs_req_t {
    uint8_t  bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
};

/* v4: mirrors the slimmed posei_hs_t. MIC/SNonce/replay-counter live inside
 * eapol_m2; the ESSID is supplied by the S3 from its scan target. */
struct __attribute__((packed)) c5_hs_t {
    uint8_t  bssid[6];
    uint8_t  sta[6];
    uint8_t  anonce[32];
    uint16_t eapol_m2_len;
    uint8_t  eapol_m2[128];   /* full M2 802.1X frame (from the 802.1X header) */
};
static_assert(sizeof(c5_hs_t) == 174, "c5_hs_t must match posei_hs_t (174B)");

/* v5 (ESP-NOW v2 flag day): must stay byte-identical to posei_msg_t in
 * c5_node/main/proto.h. payload_len moved ahead of payload and widened to
 * u16; payload raised 230 → 1024. The frame is VARIABLE length on the wire —
 * only C5_HEADER_SZ + payload_len bytes are sent (see c5_wire_len), so the
 * bigger ceiling costs nothing on small control frames. Requires ESP-NOW v2
 * (auto-negotiated on ESP-IDF >= 5.4, which both chips run). */
#define C5_PAYLOAD_MAX 1024
struct __attribute__((packed)) c5_msg_t {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;
    uint16_t seq;
    uint16_t payload_len;
    uint8_t  payload[C5_PAYLOAD_MAX];
};

/* Header (everything before payload) = 10 bytes; mirrors POSEI_HEADER_SZ. */
#define C5_HEADER_SZ (offsetof(c5_msg_t, payload))

/* Bytes actually put on air / expected off air: header + used payload only. */
static inline size_t c5_wire_len(const c5_msg_t *m) {
    uint16_t pl = m->payload_len;
    if (pl > C5_PAYLOAD_MAX) pl = C5_PAYLOAD_MAX;
    return C5_HEADER_SZ + pl;
}

static_assert(C5_HEADER_SZ == 10, "v5 header must be 10 bytes (matches POSEI_HEADER_SZ)");
static_assert(sizeof(c5_msg_t) <= 1490, "c5_msg_t must fit one ESP-NOW v2 frame");

struct __attribute__((packed)) c5_ap_t {
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  auth;
    uint8_t  is_5g;
    char     ssid[33];
};

struct __attribute__((packed)) c5_zb_t {
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  frame_type;
    uint16_t pan_id;
    uint16_t src_short;
    uint16_t dst_short;
    uint8_t  seq;
};

struct __attribute__((packed)) c5_hello_t {
    char     name[12];
    uint32_t heap_kb;
    uint8_t  role;
    uint8_t  has_5g;
    uint8_t  has_ieee802154;
    uint8_t  caps;    /* POSEI_CAP_* bitfield (0 = legacy node) */
};

/* Phase A diagnostics snapshot (S3-local; not a wire struct). Filled by
 * c5_diag() from data already received — HELLO (name/heap/caps), the ESP-NOW
 * recv RSSI, ping/pong RTT, and the latest global RESP_STATUS. */
struct c5_diag_t {
    char     name[12];
    uint8_t  online;           /* seen within the 30 s eviction window */
    uint32_t last_seen_age_ms;
    int8_t   rssi;             /* 0 = unknown */
    uint16_t heap_kb;
    uint8_t  has_5g;
    uint8_t  has_ieee802154;
    uint32_t rtt_ms;           /* 0 = no ping answered yet */
    uint8_t  channel;          /* latest RESP_STATUS (attack/scan only) */
    uint32_t frames;
};

/* ---- NEW v3 request/response structs ---- */

struct __attribute__((packed)) c5_clients_req_t {
    uint8_t  target_bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  hop_all;
};

struct __attribute__((packed)) c5_sta_t {
    uint8_t  sta[6];
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
};

struct __attribute__((packed)) c5_probe_t {
    uint8_t  src[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  ssid_len;
    char     ssid[33];
};

struct __attribute__((packed)) c5_deauth_hit_t {
    uint8_t  src[6];
    uint8_t  dst[6];
    uint8_t  channel;
    uint8_t  reason;
};

struct __attribute__((packed)) c5_beacon_spam_req_t {
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  mode;        /* 0=meme, 1=rickroll, 2=ssid_list */
    uint8_t  ssid_n;
    char     ssids[5][33];
};

struct __attribute__((packed)) c5_probe_sniff_req_t {
    uint8_t  channel;     /* 0 = hop */
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_deauth_detect_req_t {
    uint8_t  channel;
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_karma_req_t {
    uint8_t  channel;
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_apclone_req_t {
    uint8_t  channel;
    char     ssid[33];
    uint8_t  open;
    char     pass[33];
};

struct __attribute__((packed)) c5_spectrum_t {
    uint8_t  ch_count;
    struct __attribute__((packed)) {
        uint8_t ch;
        int8_t  peak_rssi;
    } ch[24];
};

struct __attribute__((packed)) c5_spectrum_req_t {
    uint16_t duration_ms;
};

struct __attribute__((packed)) c5_ciw_req_t {
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  category;
    uint16_t interval_ms;
};

/* Time-set (v5, additive): S3 → node wall-clock, byte-identical to
 * posei_timeset_t (c5_node/main/proto.h). */
struct __attribute__((packed)) c5_timeset_t {
    uint32_t epoch;          /* UTC seconds                              */
    int16_t  tz_offset_min;  /* local offset from UTC, display only      */
};

/* ---- wire-size guards: every payload struct must fit one frame, mirroring
 * the _Static_asserts in c5_node/main/proto.h so drift breaks the build. ---- */
static_assert(sizeof(c5_ap_t)              <= C5_PAYLOAD_MAX, "c5_ap_t");
static_assert(sizeof(c5_zb_t)              <= C5_PAYLOAD_MAX, "c5_zb_t");
static_assert(sizeof(c5_hello_t)           <= C5_PAYLOAD_MAX, "c5_hello_t");
static_assert(sizeof(c5_pmkid_t)           <= C5_PAYLOAD_MAX, "c5_pmkid_t");
static_assert(sizeof(c5_clients_req_t)     <= C5_PAYLOAD_MAX, "c5_clients_req_t");
static_assert(sizeof(c5_sta_t)             <= C5_PAYLOAD_MAX, "c5_sta_t");
static_assert(sizeof(c5_probe_t)           <= C5_PAYLOAD_MAX, "c5_probe_t");
static_assert(sizeof(c5_deauth_hit_t)      <= C5_PAYLOAD_MAX, "c5_deauth_hit_t");
static_assert(sizeof(c5_beacon_spam_req_t) <= C5_PAYLOAD_MAX, "c5_beacon_spam_req_t");
static_assert(sizeof(c5_apclone_req_t)     <= C5_PAYLOAD_MAX, "c5_apclone_req_t");
static_assert(sizeof(c5_spectrum_t)        <= C5_PAYLOAD_MAX, "c5_spectrum_t");
static_assert(sizeof(c5_ciw_req_t)         <= C5_PAYLOAD_MAX, "c5_ciw_req_t");
static_assert(sizeof(c5_timeset_t)         == 6, "c5_timeset_t == posei_timeset_t (6B)");
static_assert(sizeof(c5_timeset_t)         <= C5_PAYLOAD_MAX, "c5_timeset_t");

/* ---- GODZILLA detection-publish payloads — byte-identical to
 * posei_wifi_det_t / posei_ble_det_t in c5_node/main/proto.h. The literal
 * size asserts below pin both headers to the same numbers, so drift on either
 * side breaks a build. ---- */
#define C5_DET_MAX 64

struct __attribute__((packed)) c5_wifi_det_entry_t {
    uint8_t  bssid[6];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  family;     /* C5_DET_FAM_* */
    uint8_t  flags;      /* reserved, 0 in P0 */
};

struct __attribute__((packed)) c5_ble_det_entry_t {
    uint8_t  addr[6];
    uint8_t  addr_type;  /* 0 = public, 1 = random */
    int8_t   rssi;
    uint8_t  family;
    uint8_t  flags;
};

struct __attribute__((packed)) c5_wifi_det_t {
    uint8_t  node_id;
    uint8_t  count;
    c5_wifi_det_entry_t det[C5_DET_MAX];
};

struct __attribute__((packed)) c5_ble_det_t {
    uint8_t  node_id;
    uint8_t  count;
    c5_ble_det_entry_t det[C5_DET_MAX];
};

static inline size_t c5_wifi_det_bytes(uint8_t count) {
    if (count > C5_DET_MAX) count = C5_DET_MAX;
    return offsetof(c5_wifi_det_t, det) + (size_t)count * sizeof(c5_wifi_det_entry_t);
}
static inline size_t c5_ble_det_bytes(uint8_t count) {
    if (count > C5_DET_MAX) count = C5_DET_MAX;
    return offsetof(c5_ble_det_t, det) + (size_t)count * sizeof(c5_ble_det_entry_t);
}

static_assert(sizeof(c5_wifi_det_entry_t) == 10, "c5_wifi_det_entry_t == posei_wifi_det_entry_t (10B)");
static_assert(sizeof(c5_ble_det_entry_t)  == 10, "c5_ble_det_entry_t == posei_ble_det_entry_t (10B)");
static_assert(sizeof(c5_wifi_det_t) == 2 + C5_DET_MAX * 10, "c5_wifi_det_t == posei_wifi_det_t");
static_assert(sizeof(c5_ble_det_t)  == 2 + C5_DET_MAX * 10, "c5_ble_det_t == posei_ble_det_t");
static_assert(sizeof(c5_wifi_det_t) <= C5_PAYLOAD_MAX, "c5_wifi_det_t fits frame");
static_assert(sizeof(c5_ble_det_t)  <= C5_PAYLOAD_MAX, "c5_ble_det_t fits frame");

/* S3-side merge of the batches above lives in a pure header (native-tested).
 * Pin its mirrored constants to this file's so drift breaks the build. */
#include "godzilla_det.h"
static_assert(GZ_TYPE_WIFI_DET == C5_TYPE_RESP_WIFI_DET, "gz/c5 wifi det type");
static_assert(GZ_TYPE_BLE_DET  == C5_TYPE_RESP_BLE_DET,  "gz/c5 ble det type");
static_assert(GZ_DET_BATCH_MAX == C5_DET_MAX,            "gz/c5 det batch max");
static_assert(GZ_DET_ENTRY_SZ  == sizeof(c5_wifi_det_entry_t), "gz/c5 wifi entry size");
static_assert(GZ_DET_ENTRY_SZ  == sizeof(c5_ble_det_entry_t),  "gz/c5 ble entry size");
static_assert(GZ_DET_HDR_SZ    == offsetof(c5_wifi_det_t, det), "gz/c5 wifi batch header");
static_assert(GZ_DET_HDR_SZ    == offsetof(c5_ble_det_t, det),  "gz/c5 ble batch header");

/* ---- API ---- */

bool c5_begin(void);
void c5_stop(void);

bool     c5_any_online(void);
int      c5_peer_count(void);
uint32_t c5_last_seen_ms(void);
const char *c5_peer_name(int idx);
void c5_peer_name_copy(int idx, char *out, int max);
int  c5_diag(c5_diag_t *out, int max);   /* Phase A per-peer diagnostics */

/* Existing command senders. */
uint16_t c5_cmd_scan_5g(uint16_t duration_ms);
uint16_t c5_cmd_scan_zb(uint8_t channel);
uint16_t c5_cmd_stop(void);
uint16_t c5_cmd_ping(void);
uint16_t c5_cmd_deauth(const uint8_t bssid[6], uint8_t channel,
                       uint8_t bcast_all, uint16_t duration_ms);
uint16_t c5_cmd_pmkid(const uint8_t bssid[6], uint8_t channel, uint16_t duration_ms);
uint16_t c5_cmd_hs(const uint8_t bssid[6], uint8_t channel, uint16_t duration_ms);

/* New v3 command senders. */
uint16_t c5_cmd_clients_hunt(uint16_t duration_ms);
uint16_t c5_cmd_clients_ap(const uint8_t bssid[6], uint8_t channel, uint16_t duration_ms);
uint16_t c5_cmd_beacon_spam(uint8_t channel, uint16_t duration_ms,
                            uint8_t mode, uint8_t ssid_n, const char ssids[][33]);
uint16_t c5_cmd_probe_sniff(uint8_t channel, uint16_t duration_ms);
uint16_t c5_cmd_deauth_detect(uint8_t channel, uint16_t duration_ms);
uint16_t c5_cmd_karma(uint8_t channel, uint16_t duration_ms);
uint16_t c5_cmd_apclone(uint8_t channel, const char *ssid, bool open, const char *pass);
uint16_t c5_cmd_spectrum(uint16_t duration_ms);
uint16_t c5_cmd_ciw(uint8_t channel, uint16_t duration_ms,
                    uint8_t category, uint16_t interval_ms);

/* Launch the live C5 deauth dashboard — same UX as feat_wifi_deauth
 * but routes TX to the C5 via ESP-NOW. Persists until ESC; SPACE
 * pauses. broadcast=true → FF:FF:FF:FF:FF:FF deauth on the AP's
 * channel. */
struct ap_t;
void c5_deauth_dashboard(const ap_t &a, bool broadcast);

/* Status (RESP_STATUS values). */
uint32_t c5_status_frames(void);
uint8_t  c5_status_channel(void);

/* Debug. */
uint32_t c5_dbg_resp_ap_frames(void);
uint32_t c5_dbg_raw_ap_records(void);

/* Pull collected results. */
int c5_aps(c5_ap_t *out, int max);
int c5_zbs(c5_zb_t *out, int max);
int c5_pmkids(c5_pmkid_t *out, int max);
int c5_hss(c5_hs_t *out, int max);
int c5_stas(c5_sta_t *out, int max);
int c5_probes(c5_probe_t *out, int max);
int c5_deauth_hits(c5_deauth_hit_t *out, int max);
bool c5_spectrum_get(c5_spectrum_t *out);   /* latest single batch */
/* GODZILLA detections merged from every publishing node (types 60/61).
 * Copies up to `max` rows; with out == NULL returns the row count. */
int c5_dets(gz_det_t *out, int max);
uint32_t c5_dbg_det_frames(void);    /* 60/61 frames accepted */
uint32_t c5_dbg_det_rejects(void);   /* 60/61 frames rejected as malformed */
void c5_clear_results(void);
