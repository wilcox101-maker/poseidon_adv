/* Native Unity test for godzilla_node/main/gz_radar.h — the node-local radar
 * table and geometry (P2b): upsert/eviction/sticky family, batch feed, TTL,
 * severity mapping vs the Cardputer view (#54), RSSI->radius, stable hashed
 * angle, screen placement and the finger hit test. */
#include <unity.h>
#include "../../godzilla_node/main/gz_radar.h"
#include "../../godzilla_node/main/gz_batch.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t A1[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
static const uint8_t A2[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01 };

static gz_radar_table_t T;

static void test_note_upsert_and_sticky_family(void)
{
    gz_radar_clear(&T);
    TEST_ASSERT_EQUAL_INT(0, gz_radar_note(&T, GZ_RADAR_BLE, A1, 1, -70,
                                           POSEI_DET_FAM_AIRTAG, 1000));
    /* Same device, vaguer family + new RSSI: one row, family stays AirTag. */
    TEST_ASSERT_EQUAL_INT(0, gz_radar_note(&T, GZ_RADAR_BLE, A1, 1, -60,
                                           POSEI_DET_FAM_BLE_GENERIC, 2000));
    TEST_ASSERT_EQUAL_INT(1, T.n);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_AIRTAG, T.e[0].family);
    TEST_ASSERT_EQUAL_INT8(-60, T.e[0].rssi);
    TEST_ASSERT_EQUAL_UINT16(2, T.e[0].hits);
    TEST_ASSERT_EQUAL_UINT32(2000, T.e[0].last_seen_ms);

    /* A generic row upgrades when a specific family arrives. */
    gz_radar_note(&T, GZ_RADAR_BLE, A2, 0, -80, POSEI_DET_FAM_BLE_GENERIC, 2000);
    gz_radar_note(&T, GZ_RADAR_BLE, A2, 0, -80, POSEI_DET_FAM_TILE, 2100);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_TILE, T.e[1].family);

    /* Same address, other kind: separate row. */
    gz_radar_note(&T, GZ_RADAR_WIFI, A1, 6, -50, POSEI_DET_FAM_WIFI_AP, 2200);
    TEST_ASSERT_EQUAL_INT(3, T.n);
}

static void test_zero_address_ignored(void)
{
    static const uint8_t Z[6] = { 0 };
    gz_radar_clear(&T);
    TEST_ASSERT_EQUAL_INT(-1, gz_radar_note(&T, GZ_RADAR_BLE, Z, 0, -50, 0, 1));
    TEST_ASSERT_EQUAL_INT(0, T.n);
}

static void test_evicts_least_recent_across_wrap(void)
{
    gz_radar_clear(&T);
    uint8_t a[6] = { 1, 0, 0, 0, 0, 0 };
    /* Fill with times just before the millis() wrap; row 5 is the oldest. */
    for (int i = 0; i < GZ_RADAR_MAX; ++i) {
        a[5] = (uint8_t)i;
        uint32_t t = 0xFFFFF000u + (uint32_t)i * 10u;
        if (i == 5) t = 0xFFFF0000u;
        gz_radar_note(&T, GZ_RADAR_BLE, a, 0, -70, 0, t);
    }
    TEST_ASSERT_EQUAL_INT(GZ_RADAR_MAX, T.n);
    uint8_t fresh[6] = { 9, 9, 9, 9, 9, 9 };
    int i = gz_radar_note(&T, GZ_RADAR_BLE, fresh, 0, -70, 0, 0x00000100u); /* wrapped */
    TEST_ASSERT_EQUAL_INT(5, i);
    TEST_ASSERT_EQUAL_INT(GZ_RADAR_MAX, T.n);
    TEST_ASSERT_EQUAL_UINT16(1, T.e[5].hits);
}

static void test_note_batch_reads_wire_layout(void)
{
    gz_radar_clear(&T);
    const uint8_t entries[2 * 10] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66,  6, (uint8_t)-55, POSEI_DET_FAM_WIFI_AP, 0,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01, 11, (uint8_t)-82, POSEI_DET_FAM_WIFI_STA, 0,
    };
    gz_radar_note_batch(&T, GZ_RADAR_WIFI, entries, 2, 500);
    TEST_ASSERT_EQUAL_INT(2, T.n);
    TEST_ASSERT_EQUAL_UINT8(6, T.e[0].aux);
    TEST_ASSERT_EQUAL_INT8(-55, T.e[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_WIFI_STA, T.e[1].family);
    TEST_ASSERT_EQUAL_INT8(-82, T.e[1].rssi);
}

static void test_feed_from_real_ble_batch(void)
{
    /* The publish tick feeds (const uint8_t *)snap.det: pin that the packed
     * posei_ble_det_entry_t layout reads back correctly. */
    posei_ble_det_t b;
    gz_ble_batch_init(&b, 2);
    gz_ble_batch_add(&b, A1, 1, -61, POSEI_DET_FAM_AIRTAG);
    gz_ble_batch_add(&b, A2, 0, -77, POSEI_DET_FAM_BLE_GENERIC);
    gz_radar_clear(&T);
    gz_radar_note_batch(&T, GZ_RADAR_BLE, (const uint8_t *)b.det, b.count, 42);
    TEST_ASSERT_EQUAL_INT(2, T.n);
    TEST_ASSERT_EQUAL_MEMORY(A1, T.e[0].addr, 6);
    TEST_ASSERT_EQUAL_UINT8(1, T.e[0].aux);
    TEST_ASSERT_EQUAL_INT8(-61, T.e[0].rssi);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_AIRTAG, T.e[0].family);
    TEST_ASSERT_EQUAL_INT8(-77, T.e[1].rssi);
}

static void test_ttl(void)
{
    gz_radar_row_t r = {};
    r.last_seen_ms = 0xFFFFFF00u;
    TEST_ASSERT_TRUE(gz_radar_live(&r, 0xFFFFFF00u + GZ_RADAR_TTL_MS - 1u));
    TEST_ASSERT_FALSE(gz_radar_live(&r, 0xFFFFFF00u + GZ_RADAR_TTL_MS));
}

static void test_severity_matches_cardputer(void)
{
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_SURVEIL, gz_radar_severity(POSEI_DET_FAM_FLOCK));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_SURVEIL, gz_radar_severity(POSEI_DET_FAM_RAVEN));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_SURVEIL, gz_radar_severity(POSEI_DET_FAM_REMOTEID));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_TRACKER, gz_radar_severity(POSEI_DET_FAM_FINDMY));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_TRACKER, gz_radar_severity(POSEI_DET_FAM_AIRTAG));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_TRACKER, gz_radar_severity(POSEI_DET_FAM_TILE));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_TRACKER, gz_radar_severity(POSEI_DET_FAM_SMARTTAG));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_NEUTRAL, gz_radar_severity(POSEI_DET_FAM_IBEACON));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_NEUTRAL, gz_radar_severity(POSEI_DET_FAM_WIFI_AP));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_NEUTRAL, gz_radar_severity(POSEI_DET_FAM_BLE_GENERIC));
    TEST_ASSERT_EQUAL_UINT8(GZ_SEV_NEUTRAL, gz_radar_severity(200));
    TEST_ASSERT_EQUAL_STRING("Remote ID", gz_radar_family_name(POSEI_DET_FAM_REMOTEID));
    TEST_ASSERT_EQUAL_STRING("Unknown", gz_radar_family_name(200));
}

static void test_radius_monotonic_and_clamped(void)
{
    TEST_ASSERT_EQUAL_INT(20, gz_radar_radius(-20, 20, 160));
    TEST_ASSERT_EQUAL_INT(20, gz_radar_radius(GZ_RADAR_RSSI_STRONG, 20, 160));
    TEST_ASSERT_EQUAL_INT(160, gz_radar_radius(GZ_RADAR_RSSI_WEAK, 20, 160));
    TEST_ASSERT_EQUAL_INT(160, gz_radar_radius(-128, 20, 160));
    TEST_ASSERT_EQUAL_INT(90, gz_radar_radius(-65, 20, 160));          /* midpoint */
    int prev = -1;
    for (int r = -30; r >= -100; --r) {
        int v = gz_radar_radius((int8_t)r, 20, 160);
        TEST_ASSERT_TRUE(v >= prev);
        prev = v;
    }
}

static void test_angle_stable_and_spread(void)
{
    TEST_ASSERT_EQUAL_UINT16(gz_radar_angle(GZ_RADAR_BLE, A1),
                             gz_radar_angle(GZ_RADAR_BLE, A1));
    TEST_ASSERT_TRUE(gz_radar_angle(GZ_RADAR_BLE, A1) < 360);
    /* Kind is part of the key. */
    TEST_ASSERT_NOT_EQUAL(gz_radar_angle(GZ_RADAR_BLE, A1),
                          gz_radar_angle(GZ_RADAR_WIFI, A1));
    /* Sequential MACs land in many different 30-degree sectors. */
    uint16_t seen = 0;
    uint8_t a[6] = { 0x24, 0x0A, 0xC4, 0, 0, 0 };
    for (int i = 0; i < 64; ++i) {
        a[5] = (uint8_t)i;
        seen |= (uint16_t)(1u << (gz_radar_angle(GZ_RADAR_WIFI, a) / 30));
    }
    int sectors = 0;
    for (int s = 0; s < 12; ++s) sectors += (seen >> s) & 1;
    TEST_ASSERT_TRUE(sectors >= 10);
}

static void test_xy_compass(void)
{
    int x, y;
    gz_radar_xy(0, 100, 180, 180, &x, &y);    /* up */
    TEST_ASSERT_EQUAL_INT(180, x); TEST_ASSERT_EQUAL_INT(80, y);
    gz_radar_xy(90, 100, 180, 180, &x, &y);   /* right */
    TEST_ASSERT_EQUAL_INT(280, x); TEST_ASSERT_EQUAL_INT(180, y);
    gz_radar_xy(180, 100, 180, 180, &x, &y);  /* down */
    TEST_ASSERT_EQUAL_INT(180, x); TEST_ASSERT_EQUAL_INT(280, y);
    gz_radar_xy(270, 100, 180, 180, &x, &y);  /* left */
    TEST_ASSERT_EQUAL_INT(80, x); TEST_ASSERT_EQUAL_INT(180, y);
}

static void test_hit_nearest_live_within_tolerance(void)
{
    gz_radar_clear(&T);
    gz_radar_note(&T, GZ_RADAR_BLE, A1, 0, -50, 0, 1000);
    gz_radar_note(&T, GZ_RADAR_BLE, A2, 0, -90, 0, 1000);
    int x, y;
    gz_radar_place(&T.e[1], 180, 180, 20, 160, &x, &y);
    TEST_ASSERT_EQUAL_INT(1, gz_radar_hit(T.e, T.n, 2000, 180, 180, 20, 160, x + 5, y - 5, 22));
    /* Far from both: miss. */
    TEST_ASSERT_EQUAL_INT(-1, gz_radar_hit(T.e, T.n, 2000, 180, 180, 20, 160, 0, 0, 22));
    /* Expired rows are not hittable. */
    TEST_ASSERT_EQUAL_INT(-1, gz_radar_hit(T.e, T.n, 1000 + GZ_RADAR_TTL_MS, 180, 180,
                                           20, 160, x, y, 22));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_note_upsert_and_sticky_family);
    RUN_TEST(test_zero_address_ignored);
    RUN_TEST(test_evicts_least_recent_across_wrap);
    RUN_TEST(test_note_batch_reads_wire_layout);
    RUN_TEST(test_feed_from_real_ble_batch);
    RUN_TEST(test_ttl);
    RUN_TEST(test_severity_matches_cardputer);
    RUN_TEST(test_radius_monotonic_and_clamped);
    RUN_TEST(test_angle_stable_and_spread);
    RUN_TEST(test_xy_compass);
    RUN_TEST(test_hit_nearest_live_within_tolerance);
    return UNITY_END();
}
