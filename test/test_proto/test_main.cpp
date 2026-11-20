/* Native Unity tests for the POSEIDON ESP-NOW wire protocol (v5) header,
 * c5_node/main/proto.h. proto.h is pure (stdint/stddef only — no esp_now.h),
 * so it compiles and runs on the native host. These pin the v5 flag-day
 * invariants so a future edit that silently breaks the wire format fails CI
 * instead of the hardware:
 *   - the 10-byte header and its field order (payload_len ahead of payload),
 *   - the variable-length wire-size math (posei_wire_len),
 *   - the version constant, and
 *   - that every payload struct still fits one ESP-NOW v2 frame.
 *
 * The compile-time _Static_asserts inside proto.h also fire here; this file
 * adds the run-time checks a static_assert can't express. proto_validate()
 * lives in proto.c (which pulls in esp_now.h) and so is covered on hardware,
 * not natively. */
#include <unity.h>
#include <string.h>
#include "../../c5_node/main/proto.h"

void setUp(void) {}
void tearDown(void) {}

/* Header is exactly 10 bytes and payload_len sits in it, ahead of payload. */
static void test_header_layout(void)
{
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)POSEI_HEADER_SZ);
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)offsetof(posei_msg_t, magic));
    TEST_ASSERT_EQUAL_UINT(4, (unsigned)offsetof(posei_msg_t, version));
    TEST_ASSERT_EQUAL_UINT(5, (unsigned)offsetof(posei_msg_t, type));
    TEST_ASSERT_EQUAL_UINT(6, (unsigned)offsetof(posei_msg_t, seq));
    TEST_ASSERT_EQUAL_UINT(8, (unsigned)offsetof(posei_msg_t, payload_len));
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)offsetof(posei_msg_t, payload));
}

static void test_version_is_5(void)
{
    TEST_ASSERT_EQUAL_UINT(5, POSEI_VERSION);
}

/* Variable-length framing: a message puts only header + used payload on air. */
static void test_wire_len(void)
{
    posei_msg_t m;
    memset(&m, 0, sizeof(m));
    m.payload_len = 0;
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)posei_wire_len(&m));   /* bare control frame */
    m.payload_len = 174;                                       /* a full handshake */
    TEST_ASSERT_EQUAL_UINT(184, (unsigned)posei_wire_len(&m));
    m.payload_len = POSEI_PAYLOAD_MAX;
    TEST_ASSERT_EQUAL_UINT(10 + POSEI_PAYLOAD_MAX, (unsigned)posei_wire_len(&m));
    /* clamps a bogus over-max length instead of overrunning */
    m.payload_len = POSEI_PAYLOAD_MAX + 50;
    TEST_ASSERT_EQUAL_UINT(10 + POSEI_PAYLOAD_MAX, (unsigned)posei_wire_len(&m));
}

/* Whole frame fits one ESP-NOW v2 frame, and the raised ceiling actually buys
 * the headroom the flag day was for (many AP records per frame, full EAPOL). */
static void test_fits_one_v2_frame(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT(1490, (unsigned)sizeof(posei_msg_t));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1024, (unsigned)POSEI_PAYLOAD_MAX);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(POSEI_PAYLOAD_MAX, (unsigned)sizeof(posei_hs_t));
    /* >5 AP records per RESP_AP batch (was 5 at the old 230-byte ceiling). */
    TEST_ASSERT_GREATER_THAN_UINT(5, (unsigned)(POSEI_PAYLOAD_MAX / sizeof(posei_ap_t)));
}

/* ---- GODZILLA node: multi-node detection-publish (approach B) ---- */

/* The new detection-publish types live in a fresh range (60+) and must not
 * collide with any existing C5 command/response type. The C5 never emits them,
 * so adding them leaves its on-air byte stream identical — the whole point of
 * approach B (header and version below are unchanged). */
static void test_godzilla_types_distinct(void)
{
    TEST_ASSERT_EQUAL_UINT(60, POSEI_TYPE_RESP_WIFI_DET);
    TEST_ASSERT_EQUAL_UINT(61, POSEI_TYPE_RESP_BLE_DET);
    /* above every existing type (HELLO=1, CMD 10-26, RESP 40-49) */
    TEST_ASSERT_GREATER_THAN_UINT(POSEI_TYPE_RESP_SPECTRUM, POSEI_TYPE_RESP_WIFI_DET);
    TEST_ASSERT_NOT_EQUAL(POSEI_TYPE_RESP_WIFI_DET, POSEI_TYPE_RESP_BLE_DET);
}

/* Approach B keeps the wire header and version frozen, so a C5 built before
 * this change still speaks the exact same protocol. */
static void test_c5_onair_unchanged(void)
{
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)POSEI_HEADER_SZ);
    TEST_ASSERT_EQUAL_UINT(5, POSEI_VERSION);
    TEST_ASSERT_EQUAL_UINT(14, POSEI_TYPE_CMD_DEAUTH);
    TEST_ASSERT_EQUAL_UINT(41, POSEI_TYPE_RESP_AP);
    TEST_ASSERT_EQUAL_UINT(49, POSEI_TYPE_RESP_SPECTRUM);
}

/* Classifier families are a shared vocabulary; their numeric values are wire
 * state, so pin them. */
static void test_det_families(void)
{
    TEST_ASSERT_EQUAL_UINT(0, POSEI_DET_FAM_UNKNOWN);
    TEST_ASSERT_EQUAL_UINT(1, POSEI_DET_FAM_WIFI_AP);
    TEST_ASSERT_EQUAL_UINT(2, POSEI_DET_FAM_WIFI_STA);
    TEST_ASSERT_EQUAL_UINT(3, POSEI_DET_FAM_BLE_GENERIC);
    TEST_ASSERT_EQUAL_UINT(4, POSEI_DET_FAM_FINDMY);
    TEST_ASSERT_EQUAL_UINT(5, POSEI_DET_FAM_AIRTAG);
    TEST_ASSERT_EQUAL_UINT(6, POSEI_DET_FAM_IBEACON);
    TEST_ASSERT_EQUAL_UINT(7, POSEI_DET_FAM_REMOTEID);
    TEST_ASSERT_EQUAL_UINT(8, POSEI_DET_FAM_FLOCK);
    TEST_ASSERT_EQUAL_UINT(9, POSEI_DET_FAM_RAVEN);
    TEST_ASSERT_EQUAL_UINT(10, POSEI_DET_FAM_TILE);
    TEST_ASSERT_EQUAL_UINT(11, POSEI_DET_FAM_SMARTTAG);
    TEST_ASSERT_EQUAL_UINT(12, POSEI_DET_FAM_SKIMMER);
    TEST_ASSERT_EQUAL_UINT(13, POSEI_DET_FAM_CAMERA);
    TEST_ASSERT_EQUAL_UINT(14, POSEI_DET_FAM_LE_EQUIP);
    TEST_ASSERT_EQUAL_UINT(15, POSEI_DET_FAM_ALPR);
    TEST_ASSERT_EQUAL_UINT(16, POSEI_DET_FAM_SMARTGLASS);
}

/* Each detection entry is a fixed 10 bytes, field order pinned. */
static void test_det_entry_layout(void)
{
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)sizeof(posei_wifi_det_entry_t));
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)offsetof(posei_wifi_det_entry_t, bssid));
    TEST_ASSERT_EQUAL_UINT(6, (unsigned)offsetof(posei_wifi_det_entry_t, channel));
    TEST_ASSERT_EQUAL_UINT(7, (unsigned)offsetof(posei_wifi_det_entry_t, rssi));
    TEST_ASSERT_EQUAL_UINT(8, (unsigned)offsetof(posei_wifi_det_entry_t, family));
    TEST_ASSERT_EQUAL_UINT(9, (unsigned)offsetof(posei_wifi_det_entry_t, flags));

    TEST_ASSERT_EQUAL_UINT(10, (unsigned)sizeof(posei_ble_det_entry_t));
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)offsetof(posei_ble_det_entry_t, addr));
    TEST_ASSERT_EQUAL_UINT(6, (unsigned)offsetof(posei_ble_det_entry_t, addr_type));
    TEST_ASSERT_EQUAL_UINT(7, (unsigned)offsetof(posei_ble_det_entry_t, rssi));
    TEST_ASSERT_EQUAL_UINT(8, (unsigned)offsetof(posei_ble_det_entry_t, family));
    TEST_ASSERT_EQUAL_UINT(9, (unsigned)offsetof(posei_ble_det_entry_t, flags));
}

/* A batch carries the publishing node_id up front, then count entries, and the
 * whole thing fits one ESP-NOW v2 frame at the max entry count. */
static void test_det_batch_fits(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(64, POSEI_DET_MAX);
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)offsetof(posei_wifi_det_t, node_id));
    TEST_ASSERT_EQUAL_UINT(1, (unsigned)offsetof(posei_wifi_det_t, count));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(POSEI_PAYLOAD_MAX, (unsigned)sizeof(posei_wifi_det_t));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(POSEI_PAYLOAD_MAX, (unsigned)sizeof(posei_ble_det_t));
}

/* Variable-length batch: only header(2) + count*entry bytes go on the wire,
 * and an over-max count is clamped, never overrun. */
static void test_det_bytes_math(void)
{
    const unsigned hdr = (unsigned)offsetof(posei_wifi_det_t, det);
    TEST_ASSERT_EQUAL_UINT(hdr, (unsigned)posei_wifi_det_bytes(0));
    TEST_ASSERT_EQUAL_UINT(hdr + 3u * 10u, (unsigned)posei_wifi_det_bytes(3));
    TEST_ASSERT_EQUAL_UINT(hdr + (unsigned)POSEI_DET_MAX * 10u,
                           (unsigned)posei_wifi_det_bytes(POSEI_DET_MAX));
    TEST_ASSERT_EQUAL_UINT(hdr + (unsigned)POSEI_DET_MAX * 10u,
                           (unsigned)posei_wifi_det_bytes(POSEI_DET_MAX + 7));
    TEST_ASSERT_EQUAL_UINT(hdr, (unsigned)posei_ble_det_bytes(0));
    TEST_ASSERT_EQUAL_UINT(hdr + 5u * 10u, (unsigned)posei_ble_det_bytes(5));
}

/* End-to-end: pack a GODZILLA Wi-Fi detection batch into a message, set the
 * wire length from the helper, and confirm posei_wire_len agrees and the data
 * round-trips through the payload bytes. */
static void test_det_envelope_roundtrip(void)
{
    posei_msg_t m;
    memset(&m, 0, sizeof(m));
    m.magic = POSEI_MAGIC;
    m.version = POSEI_VERSION;
    m.type = POSEI_TYPE_RESP_WIFI_DET;

    posei_wifi_det_t batch;
    memset(&batch, 0, sizeof(batch));
    batch.node_id = 0x2A;
    batch.count = 2;
    batch.det[0].channel = 6;  batch.det[0].rssi = -42;
    batch.det[0].family = POSEI_DET_FAM_WIFI_AP;
    batch.det[1].channel = 11; batch.det[1].rssi = -71;
    batch.det[1].family = POSEI_DET_FAM_WIFI_STA;

    size_t used = posei_wifi_det_bytes(batch.count);
    memcpy(m.payload, &batch, used);
    m.payload_len = (uint16_t)used;

    TEST_ASSERT_EQUAL_UINT((unsigned)(POSEI_HEADER_SZ + used),
                           (unsigned)posei_wire_len(&m));

    const posei_wifi_det_t *rx = (const posei_wifi_det_t *)m.payload;
    TEST_ASSERT_EQUAL_UINT(0x2A, rx->node_id);
    TEST_ASSERT_EQUAL_UINT(2, rx->count);
    TEST_ASSERT_EQUAL_INT(-42, rx->det[0].rssi);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_WIFI_STA, rx->det[1].family);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_header_layout);
    RUN_TEST(test_version_is_5);
    RUN_TEST(test_wire_len);
    RUN_TEST(test_fits_one_v2_frame);
    RUN_TEST(test_godzilla_types_distinct);
    RUN_TEST(test_c5_onair_unchanged);
    RUN_TEST(test_det_families);
    RUN_TEST(test_det_entry_layout);
    RUN_TEST(test_det_batch_fits);
    RUN_TEST(test_det_bytes_math);
    RUN_TEST(test_det_envelope_roundtrip);
    return UNITY_END();
}
