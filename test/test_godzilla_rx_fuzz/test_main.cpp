/*
 * test_godzilla_rx_fuzz — receive-path robustness (detection-breadth §3 item 6).
 *
 * One consolidated fuzz/boundary sweep over every pure decoder the node owns,
 * the node-side detection ENCODE path (gz_batch.h), and the shared pure
 * S3-side merge helper (src/godzilla_det.h). We feed truncated, oversized,
 * zero-length and garbage buffers and assert: no crash, a clean reject (not a
 * half-decode), output bounds are respected, and counters increment on the
 * accepted path.
 *
 * NO src/* is edited — src/godzilla_det.h is included read-only (the #98 §2.4a
 * pattern). The POSEIDON-side c5_dets receive path stays Claudius's lane; this
 * covers the node tree and the shared pure header only.
 *
 * Over-read note: every fuzz input is placed in a heap block sized EXACTLY to
 * its logical length (fuzz_buf), so any read past the end is out-of-bounds and
 * is caught when this env is run under a sanitizer; the logical assertions hold
 * on every build.
 */
#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "../../godzilla_node/main/gz_ble_adv.h"
#include "../../godzilla_node/main/gz_ble_beacon.h"
#include "../../godzilla_node/main/gz_ble_eddystone.h"
#include "../../godzilla_node/main/gz_ble_altbeacon.h"
#include "../../godzilla_node/main/gz_ble_tracker.h"
#include "../../godzilla_node/main/gz_ble_company.h"
#include "../../godzilla_node/main/gz_ble_lab.h"
#include "../../godzilla_node/main/gz_wifi_mac.h"
#include "../../godzilla_node/main/gz_wifi_family.h"
#include "../../godzilla_node/main/gz_batch.h"
#include "../../src/godzilla_det.h"

void setUp(void) {}
void tearDown(void) {}

/* Deterministic xorshift32 PRNG — reproducible fuzzing. */
static uint32_t s_rng = 0x1234ABCDu;
static uint32_t rnd(void)
{
    uint32_t x = s_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return (s_rng = x);
}

/* A heap buffer sized exactly to `len`, filled with `len` random bytes. The
 * caller frees it. len==0 returns a 1-byte block (valid pointer, zero logical
 * length) so decoders get a non-NULL pointer with nothing to read. */
static uint8_t *fuzz_buf(int len)
{
    uint8_t *p = (uint8_t *)malloc(len > 0 ? (size_t)len : 1);
    for (int i = 0; i < len; ++i) p[i] = (uint8_t)rnd();
    return p;
}

/* Run every pure advert decoder over one exact-size buffer. None may crash or
 * over-read; a decode is either a clean accept or a clean reject. */
static void decode_all(const uint8_t *buf, int len)
{
    gz_adv_t a;      gz_adv_parse(buf, len, &a);
    /* A successful AD walk never reports more UUIDs than it can hold. */
    TEST_ASSERT_TRUE(a.n_uuid16 <= GZ_ADV_UUID16_MAX);

    gz_ibeacon_t ib; (void)gz_ibeacon_parse(buf, len, &ib);
    gz_eddystone_t ed; (void)gz_eddystone_parse(buf, len, &ed);
    gz_altbeacon_t ab; (void)gz_altbeacon_parse(buf, len, &ab);
    gz_tracker_t tk = gz_tracker_classify(buf, len);
    TEST_ASSERT_TRUE(tk >= GZ_TRACKER_NONE && tk <= GZ_TRACKER_SMARTTAG);
    gz_wifi_family_t fam = gz_wifi_family_classify(buf, len);
    TEST_ASSERT_TRUE(fam >= GZ_FAM_UNKNOWN && fam <= GZ_FAM_IOT);

    /* Summary formatters must stay inside their output buffers on any input. */
    char s[80];
    if (ib.valid) { int w = gz_ibeacon_summary(&ib, s, sizeof s);
                    TEST_ASSERT_TRUE(w > 0 && strlen(s) < sizeof s); }
    if (ed.valid) { gz_eddystone_summary(&ed, s, sizeof s);
                    TEST_ASSERT_TRUE(strlen(s) < sizeof s); }
    if (ab.valid) { gz_altbeacon_summary(&ab, s, sizeof s);
                    TEST_ASSERT_TRUE(strlen(s) < sizeof s); }
}

/* Sweep every length 0..64, many random payloads each — no crash, no
 * over-read, outputs bounded. */
static void test_decoder_fuzz_sweep(void)
{
    for (int len = 0; len <= 64; ++len) {
        for (int iter = 0; iter < 300; ++iter) {
            uint8_t *buf = fuzz_buf(len);
            decode_all(buf, len);
            free(buf);
        }
    }
    TEST_PASS();
}

/* The advert store takes garbage adverts of random length and address without
 * ever exceeding its capacity or returning an out-of-range index. */
static void test_lab_store_fuzz(void)
{
    gz_lab_table_t t; gz_lab_clear(&t);
    for (int iter = 0; iter < 5000; ++iter) {
        uint8_t addr[6];
        for (int i = 0; i < 6; ++i) addr[i] = (uint8_t)rnd();
        int len = (int)(rnd() % 40);            /* incl. > GZ_LAB_PAYLOAD */
        uint8_t *buf = fuzz_buf(len);
        int idx = gz_lab_note(&t, addr, (uint8_t)rnd(), (int8_t)rnd(),
                              buf, len, rnd());
        free(buf);
        TEST_ASSERT_TRUE(idx >= -1 && idx < GZ_LAB_MAX);
        TEST_ASSERT_TRUE(t.n >= 0 && t.n <= GZ_LAB_MAX);
        if (idx >= 0) TEST_ASSERT_TRUE(t.e[idx].len <= GZ_LAB_PAYLOAD);
    }
}

/* The MAC labeller never overflows its buffer on any first octet / OUI. */
static void test_mac_label_fuzz(void)
{
    for (int iter = 0; iter < 2000; ++iter) {
        uint8_t mac[6];
        for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)rnd();
        char s[24];
        int w = gz_mac_label(mac, s, sizeof s);
        TEST_ASSERT_TRUE(w > 0);
        TEST_ASSERT_TRUE(strlen(s) < sizeof s);
        /* A locally-administered address always reads "randomized". */
        if (mac[0] & GZ_MAC_LOCAL) TEST_ASSERT_EQUAL_STRING("randomized", s);
    }
}

/* ---- node-side detection ENCODE path (gz_batch.h) boundary/overflow ---- */

static void test_ble_batch_overflow(void)
{
    posei_ble_det_t b; gz_ble_batch_init(&b, 7);
    /* Add more distinct devices than the batch can hold. */
    int fulls = 0;
    for (int k = 0; k < POSEI_DET_MAX + 25; ++k) {
        uint8_t addr[6] = { (uint8_t)k, (uint8_t)(k >> 8), 0xAB, 0xCD, 0xEF, 0x01 };
        int r = gz_ble_batch_add(&b, addr, (uint8_t)(k & 1), (int8_t)-k,
                                 POSEI_DET_FAM_UNKNOWN);
        TEST_ASSERT_TRUE(r == GZ_ADD_NEW || r == GZ_ADD_UPDATED || r == GZ_ADD_FULL);
        if (r == GZ_ADD_FULL) fulls++;
        TEST_ASSERT_TRUE(b.count <= POSEI_DET_MAX);   /* never overruns */
    }
    TEST_ASSERT_TRUE(fulls > 0);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_MAX, b.count);

    /* Pack reports exactly the contract length; empty packs to 0. */
    posei_msg_t m;
    size_t n = gz_ble_batch_pack(&b, &m, 1);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)posei_ble_det_bytes(POSEI_DET_MAX), m.payload_len);
    TEST_ASSERT_TRUE(n > 0);
    gz_ble_batch_reset(&b);
    TEST_ASSERT_EQUAL_size_t(0, gz_ble_batch_pack(&b, &m, 2));
}

static void test_wifi_batch_dedup_fuzz(void)
{
    posei_wifi_det_t b; gz_wifi_batch_init(&b, 3);
    for (int iter = 0; iter < 4000; ++iter) {
        uint8_t mac[6];
        for (int i = 0; i < 6; ++i) mac[i] = (uint8_t)rnd();
        /* Narrow the address space so dedup fires often. */
        mac[0] = (uint8_t)(rnd() % 8); mac[1] = 0;
        gz_wifi_batch_add(&b, mac, (uint8_t)(rnd() % 14), (int8_t)rnd(),
                          (uint8_t)(rnd() % 6));
        TEST_ASSERT_TRUE(b.count <= POSEI_DET_MAX);
    }
    posei_msg_t m;
    size_t n = gz_wifi_batch_pack(&b, &m, 9);
    if (b.count) {
        TEST_ASSERT_EQUAL_UINT16((uint16_t)posei_wifi_det_bytes(b.count), m.payload_len);
        TEST_ASSERT_TRUE(n > 0);
    }
}

/* ---- shared pure S3-side merge helper (src/godzilla_det.h) ---- */

/* Malformed batches are rejected whole and leave the table unchanged. */
static void test_det_ingest_malformed(void)
{
    gz_det_table_t t; gz_det_clear(&t);
    const uint8_t src[6] = { 1, 2, 3, 4, 5, 6 };

    /* Prime one good row so we can prove "unchanged" on reject. */
    uint8_t good[GZ_DET_HDR_SZ + GZ_DET_ENTRY_SZ] = { 9, 1,
        0xAA,0xBB,0xCC,0xDD,0xEE,0xFF, 6, (uint8_t)-50, 1, 0 };
    TEST_ASSERT_EQUAL_INT(1, gz_det_ingest(&t, GZ_KIND_BLE, good, sizeof good, src, 1000));
    int n_before = t.n;
    uint16_t hits_before = t.e[0].hits;

    /* len < header. */
    const uint8_t tiny[1] = { 0 };
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, GZ_KIND_BLE, tiny, 1, src, 1001));
    /* count > GZ_DET_BATCH_MAX. */
    uint8_t over[GZ_DET_HDR_SZ] = { 0, (uint8_t)(GZ_DET_BATCH_MAX + 1) };
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, GZ_KIND_BLE, over, sizeof over, src, 1002));
    /* payload_len != 2 + 10*count (count says 2, only one entry present). */
    uint8_t wrong[GZ_DET_HDR_SZ + GZ_DET_ENTRY_SZ] = { 0, 2 };
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, GZ_KIND_BLE, wrong, sizeof wrong, src, 1003));
    /* bad kind, NULL payload, NULL src. */
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, 7, good, sizeof good, src, 1004));
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, GZ_KIND_BLE, NULL, 12, src, 1005));
    TEST_ASSERT_EQUAL_INT(-1, gz_det_ingest(&t, GZ_KIND_BLE, good, sizeof good, NULL, 1006));

    /* Table untouched by every reject. */
    TEST_ASSERT_EQUAL_INT(n_before, t.n);
    TEST_ASSERT_EQUAL_UINT16(hits_before, t.e[0].hits);
}

/* Well-formed random batches: accepted count is in range, the table never
 * exceeds capacity, and a repeated address increments its hit counter. */
static void test_det_ingest_fuzz(void)
{
    gz_det_table_t t; gz_det_clear(&t);
    const uint8_t src[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0, 0 };

    for (int iter = 0; iter < 3000; ++iter) {
        uint8_t count = (uint8_t)(rnd() % (GZ_DET_BATCH_MAX + 1));
        uint16_t plen = (uint16_t)(GZ_DET_HDR_SZ + count * GZ_DET_ENTRY_SZ);
        uint8_t *pl = (uint8_t *)malloc(plen);     /* exact size */
        pl[0] = (uint8_t)rnd();                    /* node_id */
        pl[1] = count;
        for (int i = GZ_DET_HDR_SZ; i < plen; ++i) pl[i] = (uint8_t)rnd();
        int applied = gz_det_ingest(&t, (uint8_t)(rnd() & 1), pl, plen, src, rnd());
        free(pl);
        TEST_ASSERT_TRUE(applied >= 0 && applied <= (int)count);
        TEST_ASSERT_TRUE(t.n >= 0 && t.n <= GZ_DET_TABLE_MAX);
    }

    /* Re-ingesting the same address lifts its hit count (counters increment). */
    uint8_t one[GZ_DET_HDR_SZ + GZ_DET_ENTRY_SZ] = { 5, 1,
        0x11,0x22,0x33,0x44,0x55,0x66, 3, (uint8_t)-40, 2, 0 };
    gz_det_table_t t2; gz_det_clear(&t2);
    gz_det_ingest(&t2, GZ_KIND_WIFI, one, sizeof one, src, 1);
    int idx = gz_det_find(&t2, GZ_KIND_WIFI, one + GZ_DET_HDR_SZ);
    TEST_ASSERT_TRUE(idx >= 0);
    uint16_t h1 = t2.e[idx].hits;
    gz_det_ingest(&t2, GZ_KIND_WIFI, one, sizeof one, src, 2);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(h1 + 1), t2.e[idx].hits);
    TEST_ASSERT_EQUAL_INT(1, t2.n);               /* same row, not a new one */

    /* All-zero address entries are skipped, never stored. */
    uint8_t zero_entry[GZ_DET_HDR_SZ + GZ_DET_ENTRY_SZ] = { 5, 1,
        0,0,0,0,0,0, 1, 2, 3, 4 };
    gz_det_table_t t3; gz_det_clear(&t3);
    TEST_ASSERT_EQUAL_INT(0, gz_det_ingest(&t3, GZ_KIND_BLE, zero_entry, sizeof zero_entry, src, 1));
    TEST_ASSERT_EQUAL_INT(0, t3.n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decoder_fuzz_sweep);
    RUN_TEST(test_lab_store_fuzz);
    RUN_TEST(test_mac_label_fuzz);
    RUN_TEST(test_ble_batch_overflow);
    RUN_TEST(test_wifi_batch_dedup_fuzz);
    RUN_TEST(test_det_ingest_malformed);
    RUN_TEST(test_det_ingest_fuzz);
    return UNITY_END();
}
