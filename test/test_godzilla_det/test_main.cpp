/* Native Unity tests for src/godzilla_det.h — the S3-side merge of GODZILLA
 * detection-publish batches (RESP_WIFI_DET = 60, RESP_BLE_DET = 61, approach B).
 *
 * Batches are built with the real wire structs from c5_node/main/proto.h, so
 * these tests also prove godzilla_det.h parses the bytes the node will send.
 * Covered: well-formed ingest, the count-vs-length checks (count is
 * sender-controlled), upsert/dedup, Wi-Fi vs BLE key separation, attribution
 * (sender MAC + self-reported node_id), LRU eviction when full, and clear. */
#include <unity.h>
#include <string.h>
#include "../../c5_node/main/proto.h"
#include "../../src/godzilla_det.h"

static gz_det_table_t T;
static const uint8_t MAC_A[6] = { 0x24, 0x0A, 0xC4, 0x00, 0x00, 0x01 };
static const uint8_t MAC_B[6] = { 0x24, 0x0A, 0xC4, 0x00, 0x00, 0x02 };

void setUp(void) { gz_det_clear(&T); }
void tearDown(void) {}

static void make_addr(uint8_t out[6], uint8_t seed)
{
    for (int i = 0; i < 6; ++i) out[i] = (uint8_t)(0x10 + i + seed);
}

/* Build a Wi-Fi batch of `count` distinct entries; returns wire bytes used. */
static size_t wifi_batch(posei_wifi_det_t *b, uint8_t node_id, uint8_t count, uint8_t seed0)
{
    memset(b, 0, sizeof(*b));
    b->node_id = node_id;
    b->count = count;
    for (uint8_t i = 0; i < count && i < POSEI_DET_MAX; ++i) {
        make_addr(b->det[i].bssid, (uint8_t)(seed0 + i));
        b->det[i].channel = 6;
        b->det[i].rssi = (int8_t)(-40 - i);
        b->det[i].family = POSEI_DET_FAM_WIFI_AP;
    }
    return posei_wifi_det_bytes(count);
}

static size_t ble_batch(posei_ble_det_t *b, uint8_t node_id, uint8_t count, uint8_t seed0)
{
    memset(b, 0, sizeof(*b));
    b->node_id = node_id;
    b->count = count;
    for (uint8_t i = 0; i < count && i < POSEI_DET_MAX; ++i) {
        make_addr(b->det[i].addr, (uint8_t)(seed0 + i));
        b->det[i].addr_type = 1;
        b->det[i].rssi = (int8_t)(-60 - i);
        b->det[i].family = POSEI_DET_FAM_AIRTAG;
    }
    return posei_ble_det_bytes(count);
}

/* gz_det_t mirrors the wire constants it depends on. */
static void test_constants_match_wire(void)
{
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_MAX, GZ_DET_BATCH_MAX);
    TEST_ASSERT_EQUAL_UINT(sizeof(posei_wifi_det_entry_t), GZ_DET_ENTRY_SZ);
    TEST_ASSERT_EQUAL_UINT(offsetof(posei_wifi_det_t, det), GZ_DET_HDR_SZ);
    TEST_ASSERT_EQUAL_UINT(offsetof(posei_ble_det_t, det), GZ_DET_HDR_SZ);
    TEST_ASSERT_EQUAL_UINT(POSEI_TYPE_RESP_WIFI_DET, GZ_TYPE_WIFI_DET);
    TEST_ASSERT_EQUAL_UINT(POSEI_TYPE_RESP_BLE_DET, GZ_TYPE_BLE_DET);
}

static void test_wifi_ingest_fields(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 7, 3, 0);
    TEST_ASSERT_EQUAL_INT(3, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1000));
    TEST_ASSERT_EQUAL_INT(3, T.n);
    const gz_det_t *e = &T.e[1];
    TEST_ASSERT_EQUAL_MEMORY(b.det[1].bssid, e->addr, 6);
    TEST_ASSERT_EQUAL_UINT8(GZ_KIND_WIFI, e->kind);
    TEST_ASSERT_EQUAL_UINT8(6, e->aux);
    TEST_ASSERT_EQUAL_INT8(-41, e->rssi);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_WIFI_AP, e->family);
    TEST_ASSERT_EQUAL_UINT8(7, e->node_id);
    TEST_ASSERT_EQUAL_MEMORY(MAC_A, e->src_mac, 6);
    TEST_ASSERT_EQUAL_UINT32(1000, e->last_seen_ms);
    TEST_ASSERT_EQUAL_UINT16(1, e->hits);
}

static void test_ble_ingest_fields(void)
{
    posei_ble_det_t b;
    size_t n = ble_batch(&b, 2, 2, 0);
    TEST_ASSERT_EQUAL_INT(2, gz_det_ingest(&T, GZ_KIND_BLE, (const uint8_t *)&b, (uint16_t)n, MAC_B, 5));
    TEST_ASSERT_EQUAL_UINT8(GZ_KIND_BLE, T.e[0].kind);
    TEST_ASSERT_EQUAL_UINT8(1, T.e[0].aux);           /* addr_type */
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_AIRTAG, T.e[0].family);
    TEST_ASSERT_EQUAL_INT8(-60, T.e[0].rssi);
}

static void test_empty_batch_ok(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 0, 0);
    TEST_ASSERT_EQUAL_INT(0, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(0, T.n);
}

/* count is sender-controlled: never read past payload_len. */
static void test_rejects_short_and_lying_frames(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 4, 0);
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, 0, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, 1, MAC_A, 1));
    /* Truncated: claims 4 entries, carries 3. */
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)(n - 10), MAC_A, 1));
    /* One byte short of the last entry. */
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)(n - 1), MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(0, T.n);
}

static void test_rejects_count_over_max(void)
{
    static uint8_t raw[GZ_DET_HDR_SZ + 70 * GZ_DET_ENTRY_SZ];
    memset(raw, 0x5A, sizeof(raw));
    raw[0] = 1;
    raw[1] = 65;                                       /* > POSEI_DET_MAX */
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, raw, (uint16_t)sizeof(raw), MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(0, T.n);
}

static void test_rejects_bad_args(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 1, 0);
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(nullptr, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, GZ_KIND_WIFI, nullptr, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&T, 9, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
}

/* Trailing bytes past count*10 are tolerated (forward-compatible extension). */
static void test_trailing_bytes_tolerated(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 2, 0);
    TEST_ASSERT_EQUAL_INT(2, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)(n + 10), MAC_A, 1));
}

static void test_all_zero_addr_skipped(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 3, 0);
    memset(b.det[1].bssid, 0, 6);
    TEST_ASSERT_EQUAL_INT(2, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(2, T.n);
}

/* Same device seen again: one row, refreshed fields, hits counted. */
static void test_upsert_refreshes(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 1, 0);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 100);
    b.node_id = 3;
    b.det[0].rssi = -20;
    b.det[0].channel = 11;
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_B, 200);
    TEST_ASSERT_EQUAL_INT(1, T.n);
    TEST_ASSERT_EQUAL_INT8(-20, T.e[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(11, T.e[0].aux);
    TEST_ASSERT_EQUAL_UINT8(3, T.e[0].node_id);
    TEST_ASSERT_EQUAL_MEMORY(MAC_B, T.e[0].src_mac, 6);
    TEST_ASSERT_EQUAL_UINT32(200, T.e[0].last_seen_ms);
    TEST_ASSERT_EQUAL_UINT16(2, T.e[0].hits);
}

static void test_duplicate_inside_one_batch(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 2, 0);
    memcpy(b.det[1].bssid, b.det[0].bssid, 6);
    TEST_ASSERT_EQUAL_INT(2, gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(1, T.n);
    TEST_ASSERT_EQUAL_UINT16(2, T.e[0].hits);
}

/* A Wi-Fi BSSID and a BLE address with the same bytes are different devices. */
static void test_kind_separates_keys(void)
{
    posei_wifi_det_t w;
    posei_ble_det_t bl;
    size_t nw = wifi_batch(&w, 1, 1, 0);
    size_t nb = ble_batch(&bl, 1, 1, 0);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&w, (uint16_t)nw, MAC_A, 1);
    gz_det_ingest(&T, GZ_KIND_BLE, (const uint8_t *)&bl, (uint16_t)nb, MAC_A, 1);
    TEST_ASSERT_EQUAL_INT(2, T.n);
}

static void test_hits_saturate(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 1, 0);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1);
    T.e[0].hits = 0xFFFF;
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 2);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, T.e[0].hits);
}

/* Full table: the least recently seen row is replaced, fresh rows survive. */
static void test_lru_eviction_when_full(void)
{
    posei_wifi_det_t b;
    for (int k = 0; k < GZ_DET_TABLE_MAX; ++k) {
        size_t n = wifi_batch(&b, 1, 1, (uint8_t)k);
        gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, (uint32_t)(1000 + k));
    }
    TEST_ASSERT_EQUAL_INT(GZ_DET_TABLE_MAX, T.n);
    /* Refresh the oldest (seed 0) so seed 1 becomes the LRU. */
    size_t n = wifi_batch(&b, 1, 1, 0);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 5000);
    /* New device evicts seed 1. */
    n = wifi_batch(&b, 1, 1, 200);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 6000);
    TEST_ASSERT_EQUAL_INT(GZ_DET_TABLE_MAX, T.n);
    uint8_t a0[6], a1[6], a200[6];
    make_addr(a0, 0); make_addr(a1, 1); make_addr(a200, 200);
    TEST_ASSERT_TRUE(gz_det_find(&T, GZ_KIND_WIFI, a0) >= 0);
    TEST_ASSERT_TRUE(gz_det_find(&T, GZ_KIND_WIFI, a1) < 0);
    TEST_ASSERT_TRUE(gz_det_find(&T, GZ_KIND_WIFI, a200) >= 0);
}

/* millis() wraps after ~49.7 days; age must use unsigned subtraction. */
static void test_lru_survives_millis_wrap(void)
{
    posei_wifi_det_t b;
    for (int k = 0; k < GZ_DET_TABLE_MAX; ++k) {
        size_t n = wifi_batch(&b, 1, 1, (uint8_t)k);
        /* seed 0 is oldest (pre-wrap), the rest just after wrap. */
        uint32_t t = (k == 0) ? 0xFFFFFF00u : (uint32_t)(10 + k);
        gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, t);
    }
    size_t n = wifi_batch(&b, 1, 1, 200);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 500);
    uint8_t a0[6], a1[6];
    make_addr(a0, 0); make_addr(a1, 1);
    TEST_ASSERT_TRUE(gz_det_find(&T, GZ_KIND_WIFI, a0) < 0);   /* pre-wrap row evicted */
    TEST_ASSERT_TRUE(gz_det_find(&T, GZ_KIND_WIFI, a1) >= 0);
}

static void test_full_max_batch(void)
{
    posei_ble_det_t b;
    size_t n = ble_batch(&b, 9, POSEI_DET_MAX, 0);
    TEST_ASSERT_EQUAL_INT(POSEI_DET_MAX, gz_det_ingest(&T, GZ_KIND_BLE, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1));
    TEST_ASSERT_EQUAL_INT(POSEI_DET_MAX, T.n);
}

static void test_clear(void)
{
    posei_wifi_det_t b;
    size_t n = wifi_batch(&b, 1, 3, 0);
    gz_det_ingest(&T, GZ_KIND_WIFI, (const uint8_t *)&b, (uint16_t)n, MAC_A, 1);
    gz_det_clear(&T);
    TEST_ASSERT_EQUAL_INT(0, T.n);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_constants_match_wire);
    RUN_TEST(test_wifi_ingest_fields);
    RUN_TEST(test_ble_ingest_fields);
    RUN_TEST(test_empty_batch_ok);
    RUN_TEST(test_rejects_short_and_lying_frames);
    RUN_TEST(test_rejects_count_over_max);
    RUN_TEST(test_rejects_bad_args);
    RUN_TEST(test_trailing_bytes_tolerated);
    RUN_TEST(test_all_zero_addr_skipped);
    RUN_TEST(test_upsert_refreshes);
    RUN_TEST(test_duplicate_inside_one_batch);
    RUN_TEST(test_kind_separates_keys);
    RUN_TEST(test_hits_saturate);
    RUN_TEST(test_lru_eviction_when_full);
    RUN_TEST(test_lru_survives_millis_wrap);
    RUN_TEST(test_full_max_batch);
    RUN_TEST(test_clear);
    return UNITY_END();
}
