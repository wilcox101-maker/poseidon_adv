/* Native Unity test for godzilla_node/main/gz_hunt.h — the pure Hunt view
 * model (P3): one-target RSSI ring, time ordering, min/max, trend, lost state
 * and trace geometry. The screen and the store's spinlock are on-target. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_hunt.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t A1[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
static const uint8_t A2[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x67 };

static gz_hunt_t H;

static gz_radar_row_t row(uint8_t kind, const uint8_t *a, int8_t rssi, uint32_t t)
{
    gz_radar_row_t r;
    memset(&r, 0, sizeof r);
    memcpy(r.addr, a, 6);
    r.kind = kind; r.aux = 6; r.rssi = rssi;
    r.family = POSEI_DET_FAM_AIRTAG; r.hits = 3; r.last_seen_ms = t;
    return r;
}

static void test_select_seeds_and_filters(void)
{
    gz_hunt_clear(&H);
    TEST_ASSERT_FALSE(H.active);
    TEST_ASSERT_FALSE(gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -60, 0, 10));  /* nothing selected */

    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, -70, 1000);
    gz_hunt_select(&H, &r);
    TEST_ASSERT_TRUE(H.active);
    TEST_ASSERT_EQUAL_UINT8(1, H.n);                       /* seeded */
    TEST_ASSERT_EQUAL_INT8(-70, gz_hunt_latest(&H));
    TEST_ASSERT_EQUAL_UINT32(1000, H.last_ms);

    /* Other address, other radio with the same address: ignored. */
    TEST_ASSERT_FALSE(gz_hunt_note(&H, GZ_RADAR_BLE, A2, 1, -40, 0, 1100));
    TEST_ASSERT_FALSE(gz_hunt_note(&H, GZ_RADAR_WIFI, A1, 1, -40, 0, 1100));
    TEST_ASSERT_EQUAL_UINT8(1, H.n);

    TEST_ASSERT_TRUE(gz_hunt_note(&H, GZ_RADAR_BLE, A1, 0, -65, POSEI_DET_FAM_FINDMY, 2000));
    TEST_ASSERT_EQUAL_UINT8(2, H.n);
    TEST_ASSERT_EQUAL_INT8(-65, gz_hunt_latest(&H));
    TEST_ASSERT_EQUAL_UINT8(0, H.aux);
    TEST_ASSERT_EQUAL_UINT8(POSEI_DET_FAM_FINDMY, H.family);

    /* Selecting again starts a fresh trace. */
    gz_radar_row_t r2 = row(GZ_RADAR_WIFI, A2, -50, 3000);
    gz_hunt_select(&H, &r2);
    TEST_ASSERT_EQUAL_UINT8(1, H.n);
    TEST_ASSERT_TRUE(gz_hunt_is(&H, GZ_RADAR_WIFI, A2));
    TEST_ASSERT_FALSE(gz_hunt_is(&H, GZ_RADAR_BLE, A1));
}

static void test_ring_order_and_wrap(void)
{
    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, -90, 0);
    gz_hunt_select(&H, &r);                     /* sample 0 = -90 */
    for (int k = 1; k < GZ_HUNT_SAMPLES + 10; ++k)
        gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, (int8_t)(-90 + (k % 50)), 0, (uint32_t)k);
    TEST_ASSERT_EQUAL_UINT8(GZ_HUNT_SAMPLES, H.n);
    /* 70 samples written (k = 0..69), 60 held: oldest is k = 10. */
    TEST_ASSERT_EQUAL_INT8(-90 + 10, gz_hunt_at(&H, 0));
    TEST_ASSERT_EQUAL_INT8(-90 + (69 % 50), gz_hunt_at(&H, GZ_HUNT_SAMPLES - 1));
    TEST_ASSERT_EQUAL_INT8(gz_hunt_at(&H, GZ_HUNT_SAMPLES - 1), gz_hunt_latest(&H));
    for (int i = 1; i < GZ_HUNT_SAMPLES; ++i) {
        int k = 10 + i;
        TEST_ASSERT_EQUAL_INT8(-90 + (k % 50), gz_hunt_at(&H, i));
    }
}

static void test_minmax(void)
{
    gz_hunt_clear(&H);
    int8_t lo = 1, hi = 1;
    TEST_ASSERT_FALSE(gz_hunt_minmax(&H, &lo, &hi));
    TEST_ASSERT_EQUAL_INT8(1, lo);

    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, -70, 0);
    gz_hunt_select(&H, &r);
    gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -48, 0, 1);
    gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -88, 0, 2);
    gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -60, 0, 3);
    TEST_ASSERT_TRUE(gz_hunt_minmax(&H, &lo, &hi));
    TEST_ASSERT_EQUAL_INT8(-88, lo);
    TEST_ASSERT_EQUAL_INT8(-48, hi);
}

static void fill(int8_t first5, int8_t last5)
{
    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, first5, 0);
    gz_hunt_select(&H, &r);
    for (int k = 1; k < 5; ++k)  gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, first5, 0, (uint32_t)k);
    for (int k = 5; k < 10; ++k) gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, last5,  0, (uint32_t)k);
}

static void test_trend(void)
{
    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, -70, 0);
    gz_hunt_select(&H, &r);
    for (int k = 1; k < 9; ++k) gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -40, 0, (uint32_t)k);
    TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_NONE, gz_hunt_trend(&H));   /* 9 < 10 */

    fill(-70, -70);           TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_STEADY,   gz_hunt_trend(&H));
    fill(-70, -67);           TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_STRONGER, gz_hunt_trend(&H));  /* +3 exactly */
    fill(-70, -68);           TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_STEADY,   gz_hunt_trend(&H));  /* +2 */
    fill(-60, -63);           TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_WEAKER,   gz_hunt_trend(&H));  /* -3 exactly */
    fill(-60, -62);           TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_STEADY,   gz_hunt_trend(&H));

    /* Only the newest 10 count: old strong readings don't mask a fade. */
    fill(-40, -40);
    for (int k = 10; k < 15; ++k) gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -40, 0, (uint32_t)k);
    for (int k = 15; k < 20; ++k) gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -80, 0, (uint32_t)k);
    TEST_ASSERT_EQUAL_INT(GZ_HUNT_TREND_WEAKER, gz_hunt_trend(&H));
}

static void test_lost(void)
{
    gz_hunt_clear(&H);
    TEST_ASSERT_TRUE(gz_hunt_lost(&H, 0));                       /* empty */
    gz_radar_row_t r = row(GZ_RADAR_BLE, A1, -70, 5000);
    gz_hunt_select(&H, &r);
    TEST_ASSERT_FALSE(gz_hunt_lost(&H, 5000 + GZ_RADAR_TTL_MS - 1));
    TEST_ASSERT_TRUE(gz_hunt_lost(&H, 5000 + GZ_RADAR_TTL_MS));
    /* Across the millis() wrap. */
    gz_hunt_note(&H, GZ_RADAR_BLE, A1, 1, -70, 0, 0xFFFFFF00u);
    TEST_ASSERT_FALSE(gz_hunt_lost(&H, 0x00000100u));
}

static void test_geometry(void)
{
    /* y: strong at top, weak at bottom, clamped like the radar. */
    TEST_ASSERT_EQUAL_INT(10,  gz_hunt_y(GZ_RADAR_RSSI_STRONG, 10, 70));
    TEST_ASSERT_EQUAL_INT(10,  gz_hunt_y(-10, 10, 70));
    TEST_ASSERT_EQUAL_INT(70,  gz_hunt_y(GZ_RADAR_RSSI_WEAK, 10, 70));
    TEST_ASSERT_EQUAL_INT(70,  gz_hunt_y(-120, 10, 70));
    TEST_ASSERT_EQUAL_INT(40,  gz_hunt_y(-65, 10, 70));          /* midpoint */

    /* x: newest at the right edge whatever n is; a full window spans it. */
    TEST_ASSERT_EQUAL_INT(200, gz_hunt_x(0, 1, 23, 200));
    TEST_ASSERT_EQUAL_INT(200, gz_hunt_x(9, 10, 23, 200));
    TEST_ASSERT_EQUAL_INT(23,  gz_hunt_x(0, GZ_HUNT_SAMPLES, 23, 200));
    TEST_ASSERT_EQUAL_INT(200, gz_hunt_x(GZ_HUNT_SAMPLES - 1, GZ_HUNT_SAMPLES, 23, 200));
    for (int i = 1; i < GZ_HUNT_SAMPLES; ++i)                     /* monotonic */
        TEST_ASSERT_TRUE(gz_hunt_x(i, GZ_HUNT_SAMPLES, 23, 200) >
                         gz_hunt_x(i - 1, GZ_HUNT_SAMPLES, 23, 200));
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_select_seeds_and_filters);
    RUN_TEST(test_ring_order_and_wrap);
    RUN_TEST(test_minmax);
    RUN_TEST(test_trend);
    RUN_TEST(test_lost);
    RUN_TEST(test_geometry);
    return UNITY_END();
}
