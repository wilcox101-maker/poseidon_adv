/* Native Unity tests for the GODZILLA node batching core,
 * godzilla_node/main/gz_batch.h. The core is pure (no radio, no heap), so it
 * runs on the host. These pin the producer half of the detection-publish
 * contract (docs/godzilla-detection-publish-contract.md):
 *   - payload_len is EXACTLY 2 + 10*count, count <= POSEI_DET_MAX,
 *   - a device seen twice in one batch occupies one entry (dedup by address),
 *   - more than POSEI_DET_MAX devices spill into further batches that carry
 *     the same node_id,
 *   - an empty batch is never put on the wire. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_batch.h"

void setUp(void) {}
void tearDown(void) {}

static void mk_addr(uint8_t a[6], unsigned n)
{
    a[0] = 0xAA; a[1] = 0xBB; a[2] = 0xCC; a[3] = 0x00;
    a[4] = (uint8_t)(n >> 8); a[5] = (uint8_t)(n & 0xFF);
}

/* ---- BLE ---- */

static void test_ble_init_and_add(void)
{
    posei_ble_det_t b;
    uint8_t a[6]; mk_addr(a, 1);
    gz_ble_batch_init(&b, 0x2A);
    TEST_ASSERT_EQUAL_UINT(0x2A, b.node_id);
    TEST_ASSERT_EQUAL_UINT(0, b.count);

    TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_ble_batch_add(&b, a, 1, -60, POSEI_DET_FAM_AIRTAG));
    TEST_ASSERT_EQUAL_UINT(1, b.count);
    TEST_ASSERT_EQUAL_MEMORY(a, b.det[0].addr, 6);
    TEST_ASSERT_EQUAL_UINT(1, b.det[0].addr_type);
    TEST_ASSERT_EQUAL_INT(-60, b.det[0].rssi);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_AIRTAG, b.det[0].family);
    TEST_ASSERT_EQUAL_UINT(0, b.det[0].flags);
}

/* Same address twice → one entry; strongest RSSI wins; a specific family
 * replaces a generic one but is never downgraded back. */
static void test_ble_dedup(void)
{
    posei_ble_det_t b;
    uint8_t a[6]; mk_addr(a, 7);
    gz_ble_batch_init(&b, 1);

    gz_ble_batch_add(&b, a, 0, -80, POSEI_DET_FAM_BLE_GENERIC);
    TEST_ASSERT_EQUAL_INT(GZ_ADD_UPDATED, gz_ble_batch_add(&b, a, 0, -50, POSEI_DET_FAM_TILE));
    TEST_ASSERT_EQUAL_UINT(1, b.count);
    TEST_ASSERT_EQUAL_INT(-50, b.det[0].rssi);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_TILE, b.det[0].family);

    /* weaker + generic sighting changes nothing */
    TEST_ASSERT_EQUAL_INT(GZ_ADD_UPDATED, gz_ble_batch_add(&b, a, 0, -90, POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_UINT(1, b.count);
    TEST_ASSERT_EQUAL_INT(-50, b.det[0].rssi);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_TILE, b.det[0].family);
}

/* Public and random addresses with the same bytes are different devices. */
static void test_ble_addr_type_is_part_of_key(void)
{
    posei_ble_det_t b;
    uint8_t a[6]; mk_addr(a, 3);
    gz_ble_batch_init(&b, 1);
    TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_ble_batch_add(&b, a, 0, -60, POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_ble_batch_add(&b, a, 1, -60, POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_UINT(2, b.count);
}

/* A full batch refuses a NEW device (caller flushes) but still dedups. */
static void test_ble_full(void)
{
    posei_ble_det_t b;
    uint8_t a[6];
    gz_ble_batch_init(&b, 1);
    for (unsigned i = 0; i < POSEI_DET_MAX; i++) {
        mk_addr(a, i);
        TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_ble_batch_add(&b, a, 0, -70, POSEI_DET_FAM_BLE_GENERIC));
    }
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_MAX, b.count);
    mk_addr(a, 9999);
    TEST_ASSERT_EQUAL_INT(GZ_ADD_FULL, gz_ble_batch_add(&b, a, 0, -70, POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_MAX, b.count);
    mk_addr(a, 5);   /* already present */
    TEST_ASSERT_EQUAL_INT(GZ_ADD_UPDATED, gz_ble_batch_add(&b, a, 0, -40, POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_MAX, b.count);
}

/* Packing honours the consumer contract exactly. */
static void test_ble_pack_contract(void)
{
    posei_ble_det_t b;
    posei_msg_t m;
    uint8_t a[6];
    gz_ble_batch_init(&b, 0x2A);
    for (unsigned i = 0; i < 3; i++) { mk_addr(a, i); gz_ble_batch_add(&b, a, 1, -55, POSEI_DET_FAM_FINDMY); }

    memset(&m, 0xEE, sizeof(m));
    size_t wire = gz_ble_batch_pack(&b, &m, 0x1234);

    TEST_ASSERT_EQUAL_UINT(POSEI_MAGIC, m.magic);
    TEST_ASSERT_EQUAL_UINT(POSEI_VERSION, m.version);
    TEST_ASSERT_EQUAL_UINT(POSEI_TYPE_RESP_BLE_DET, m.type);
    TEST_ASSERT_EQUAL_UINT(0x1234, m.seq);
    TEST_ASSERT_EQUAL_UINT(2 + 10 * 3, m.payload_len);            /* exact */
    TEST_ASSERT_EQUAL_UINT(POSEI_HEADER_SZ + 2 + 10 * 3, (unsigned)wire);
    TEST_ASSERT_EQUAL_UINT((unsigned)posei_wire_len(&m), (unsigned)wire);

    const posei_ble_det_t *rx = (const posei_ble_det_t *)m.payload;
    TEST_ASSERT_EQUAL_UINT(0x2A, rx->node_id);
    TEST_ASSERT_EQUAL_UINT(3, rx->count);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_FINDMY, rx->det[2].family);
}

/* Nothing to report → nothing on the wire. */
static void test_empty_batch_not_packed(void)
{
    posei_ble_det_t b; posei_wifi_det_t w; posei_msg_t m;
    gz_ble_batch_init(&b, 1);
    gz_wifi_batch_init(&w, 1);
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)gz_ble_batch_pack(&b, &m, 1));
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)gz_wifi_batch_pack(&w, &m, 1));
}

/* Reset empties the batch but keeps the node identity. */
static void test_reset_keeps_node_id(void)
{
    posei_ble_det_t b; uint8_t a[6]; mk_addr(a, 1);
    gz_ble_batch_init(&b, 0x07);
    gz_ble_batch_add(&b, a, 0, -60, POSEI_DET_FAM_BLE_GENERIC);
    gz_ble_batch_reset(&b);
    TEST_ASSERT_EQUAL_UINT(0, b.count);
    TEST_ASSERT_EQUAL_UINT(0x07, b.node_id);
}

/* 70 distinct devices → a full batch of 64 then one of 6, same node_id, each
 * packed frame on-contract. This is the flush loop the radio task will run. */
static void test_ble_spill_into_several_batches(void)
{
    posei_ble_det_t b; posei_msg_t m; uint8_t a[6];
    unsigned frames = 0, total = 0, sizes[4] = {0, 0, 0, 0};
    gz_ble_batch_init(&b, 0x11);

    for (unsigned i = 0; i < 70; i++) {
        mk_addr(a, i);
        if (gz_ble_batch_add(&b, a, 0, -60, POSEI_DET_FAM_BLE_GENERIC) == GZ_ADD_FULL) {
            TEST_ASSERT_TRUE(gz_ble_batch_pack(&b, &m, (uint16_t)frames) > 0);
            const posei_ble_det_t *rx = (const posei_ble_det_t *)m.payload;
            TEST_ASSERT_EQUAL_UINT(0x11, rx->node_id);
            TEST_ASSERT_EQUAL_UINT(2 + 10 * rx->count, m.payload_len);
            sizes[frames++] = rx->count; total += rx->count;
            gz_ble_batch_reset(&b);
            TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_ble_batch_add(&b, a, 0, -60, POSEI_DET_FAM_BLE_GENERIC));
        }
    }
    TEST_ASSERT_TRUE(gz_ble_batch_pack(&b, &m, (uint16_t)frames) > 0);
    const posei_ble_det_t *rx = (const posei_ble_det_t *)m.payload;
    TEST_ASSERT_EQUAL_UINT(0x11, rx->node_id);
    sizes[frames++] = rx->count; total += rx->count;

    TEST_ASSERT_EQUAL_UINT(2, frames);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_MAX, sizes[0]);
    TEST_ASSERT_EQUAL_UINT(70 - POSEI_DET_MAX, sizes[1]);
    TEST_ASSERT_EQUAL_UINT(70, total);
}

/* ---- Wi-Fi ---- */

static void test_wifi_add_dedup_pack(void)
{
    posei_wifi_det_t w; posei_msg_t m; uint8_t a[6]; mk_addr(a, 42);
    gz_wifi_batch_init(&w, 0x2A);

    TEST_ASSERT_EQUAL_INT(GZ_ADD_NEW, gz_wifi_batch_add(&w, a, 1, -75, POSEI_DET_FAM_UNKNOWN));
    /* re-seen on another channel, stronger, now classified */
    TEST_ASSERT_EQUAL_INT(GZ_ADD_UPDATED, gz_wifi_batch_add(&w, a, 6, -48, POSEI_DET_FAM_WIFI_AP));
    TEST_ASSERT_EQUAL_UINT(1, w.count);
    TEST_ASSERT_EQUAL_UINT(6, w.det[0].channel);          /* channel of strongest sighting */
    TEST_ASSERT_EQUAL_INT(-48, w.det[0].rssi);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_WIFI_AP, w.det[0].family);
    /* weaker sighting elsewhere does not move it */
    gz_wifi_batch_add(&w, a, 11, -88, POSEI_DET_FAM_WIFI_AP);
    TEST_ASSERT_EQUAL_UINT(6, w.det[0].channel);

    size_t wire = gz_wifi_batch_pack(&w, &m, 9);
    TEST_ASSERT_EQUAL_UINT(POSEI_TYPE_RESP_WIFI_DET, m.type);
    TEST_ASSERT_EQUAL_UINT(2 + 10 * 1, m.payload_len);
    TEST_ASSERT_EQUAL_UINT(POSEI_HEADER_SZ + 12, (unsigned)wire);
}

static void test_wifi_full_and_reset(void)
{
    posei_wifi_det_t w; uint8_t a[6];
    gz_wifi_batch_init(&w, 3);
    for (unsigned i = 0; i < POSEI_DET_MAX; i++) { mk_addr(a, i); gz_wifi_batch_add(&w, a, 1, -70, POSEI_DET_FAM_WIFI_STA); }
    mk_addr(a, 777);
    TEST_ASSERT_EQUAL_INT(GZ_ADD_FULL, gz_wifi_batch_add(&w, a, 1, -70, POSEI_DET_FAM_WIFI_STA));
    gz_wifi_batch_reset(&w);
    TEST_ASSERT_EQUAL_UINT(0, w.count);
    TEST_ASSERT_EQUAL_UINT(3, w.node_id);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ble_init_and_add);
    RUN_TEST(test_ble_dedup);
    RUN_TEST(test_ble_addr_type_is_part_of_key);
    RUN_TEST(test_ble_full);
    RUN_TEST(test_ble_pack_contract);
    RUN_TEST(test_empty_batch_not_packed);
    RUN_TEST(test_reset_keeps_node_id);
    RUN_TEST(test_ble_spill_into_several_batches);
    RUN_TEST(test_wifi_add_dedup_pack);
    RUN_TEST(test_wifi_full_and_reset);
    return UNITY_END();
}
