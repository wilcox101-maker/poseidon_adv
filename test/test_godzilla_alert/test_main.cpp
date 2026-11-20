/* Native Unity test for godzilla_node/main/gz_alert.h — the pure priority
 * ALERT overlay gating (detection §4, unit 6). Four jobs:
 *   1. a LOW-confidence family NEVER raises on its own — LE_EQUIP and ALPR are
 *      LOW by definition, so they are never eligible whatever the hit count or
 *      mask (the rule the orders single out for a native test);
 *   2. HIGH surveillance / recording families raise on the first sighting, and
 *      a cleared enable bit suppresses exactly that family;
 *   3. trackers raise only once seen repeatedly (hits >= GZ_ALERT_TRACKER_HITS);
 *   4. the selector picks the highest-priority LIVE eligible row (surveillance
 *      over tracker, then stronger RSSI), skips dead rows, and honours the mask;
 *      plus the per-device rate-limit and the 60 s auto-clear helpers.
 * Pure, board-agnostic — one TU, hand-built rows, like test_godzilla_radar. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_alert.h"

void setUp(void) {}
void tearDown(void) {}

static gz_radar_row_t row(uint8_t family, int8_t rssi, uint16_t hits, uint32_t last_seen)
{
    gz_radar_row_t r;
    memset(&r, 0, sizeof r);
    r.addr[0] = 0x02; r.addr[5] = (uint8_t)family;   /* a distinct non-zero MAC */
    r.kind = GZ_RADAR_BLE;
    r.family = family;
    r.rssi = rssi;
    r.hits = hits;
    r.last_seen_ms = last_seen;
    return r;
}

/* ---- 1. LOW families never raise ----------------------------------------- */
static void test_low_families_never_eligible(void)
{
    /* LE_EQUIP and ALPR are LOW-only: no hit count, no mask, ever raises them. */
    for (uint16_t h = 0; h <= 1000; h += 100) {
        TEST_ASSERT_FALSE(gz_alert_row_eligible(POSEI_DET_FAM_LE_EQUIP, h, GZ_ALERT_MASK_ALL));
        TEST_ASSERT_FALSE(gz_alert_row_eligible(POSEI_DET_FAM_ALPR,     h, GZ_ALERT_MASK_ALL));
    }
    /* They carry no enable bit at all. */
    TEST_ASSERT_EQUAL_UINT(0u, gz_alert_family_bit(POSEI_DET_FAM_LE_EQUIP));
    TEST_ASSERT_EQUAL_UINT(0u, gz_alert_family_bit(POSEI_DET_FAM_ALPR));
    /* Neutral families likewise. */
    TEST_ASSERT_FALSE(gz_alert_row_eligible(POSEI_DET_FAM_WIFI_AP,     50, GZ_ALERT_MASK_ALL));
    TEST_ASSERT_FALSE(gz_alert_row_eligible(POSEI_DET_FAM_BLE_GENERIC, 50, GZ_ALERT_MASK_ALL));
    TEST_ASSERT_FALSE(gz_alert_row_eligible(POSEI_DET_FAM_IBEACON,     50, GZ_ALERT_MASK_ALL));
}

/* ---- 2. HIGH surveillance / recording families --------------------------- */
static void test_high_families_raise_and_respect_mask(void)
{
    const uint8_t hi[] = {
        POSEI_DET_FAM_FLOCK, POSEI_DET_FAM_RAVEN, POSEI_DET_FAM_REMOTEID,
        POSEI_DET_FAM_SKIMMER, POSEI_DET_FAM_CAMERA, POSEI_DET_FAM_SMARTGLASS,
    };
    for (unsigned i = 0; i < sizeof hi / sizeof hi[0]; ++i) {
        uint16_t bit = gz_alert_family_bit(hi[i]);
        TEST_ASSERT_TRUE(bit != 0u);
        TEST_ASSERT_FALSE(gz_alert_is_tracker(hi[i]));
        /* raises on the very first sighting when enabled */
        TEST_ASSERT_TRUE(gz_alert_row_eligible(hi[i], 1, GZ_ALERT_MASK_ALL));
        /* clearing only this family's bit suppresses only it */
        uint16_t without = (uint16_t)(GZ_ALERT_MASK_ALL & ~bit);
        TEST_ASSERT_FALSE(gz_alert_row_eligible(hi[i], 1, without));
        /* a different family's bit alone still raises this one iff its bit set */
        TEST_ASSERT_TRUE(gz_alert_row_eligible(hi[i], 1, bit));
    }
}

/* ---- 3. trackers need repetition ----------------------------------------- */
static void test_trackers_need_repeats(void)
{
    const uint8_t tr[] = { POSEI_DET_FAM_FINDMY, POSEI_DET_FAM_AIRTAG,
                           POSEI_DET_FAM_TILE,   POSEI_DET_FAM_SMARTTAG };
    for (unsigned i = 0; i < sizeof tr / sizeof tr[0]; ++i) {
        TEST_ASSERT_TRUE(gz_alert_is_tracker(tr[i]));
        TEST_ASSERT_EQUAL_UINT(GZ_ALERT_BIT_TRACKER, gz_alert_family_bit(tr[i]));
        /* below the repeat threshold: no alert */
        TEST_ASSERT_FALSE(gz_alert_row_eligible(tr[i], GZ_ALERT_TRACKER_HITS - 1, GZ_ALERT_MASK_ALL));
        /* at / above: alert, unless the tracker bit is cleared */
        TEST_ASSERT_TRUE (gz_alert_row_eligible(tr[i], GZ_ALERT_TRACKER_HITS,     GZ_ALERT_MASK_ALL));
        TEST_ASSERT_TRUE (gz_alert_row_eligible(tr[i], GZ_ALERT_TRACKER_HITS + 9, GZ_ALERT_MASK_ALL));
        uint16_t without = (uint16_t)(GZ_ALERT_MASK_ALL & ~GZ_ALERT_BIT_TRACKER);
        TEST_ASSERT_FALSE(gz_alert_row_eligible(tr[i], GZ_ALERT_TRACKER_HITS + 9, without));
    }
}

/* ---- 4. selector + helpers ----------------------------------------------- */
static void test_select_prefers_surveillance_then_rssi(void)
{
    const uint32_t now = 100000u;
    gz_radar_row_t rows[] = {
        row(POSEI_DET_FAM_AIRTAG, -40, GZ_ALERT_TRACKER_HITS, now - 1000),  /* strong tracker */
        row(POSEI_DET_FAM_CAMERA, -80, 1, now - 1000),                      /* weak surveil   */
        row(POSEI_DET_FAM_FLOCK,  -50, 1, now - 1000),                      /* stronger surveil */
        row(POSEI_DET_FAM_LE_EQUIP, -30, 99, now - 1000),                   /* LOW: ineligible */
    };
    int n = (int)(sizeof rows / sizeof rows[0]);
    int sel = gz_alert_select(rows, n, GZ_ALERT_MASK_ALL, now);
    /* surveillance beats the strong tracker; among surveillance the stronger
     * RSSI (Flock -50) beats the weaker (Camera -80); LE_EQUIP is never chosen. */
    TEST_ASSERT_EQUAL_INT(2, sel);
    TEST_ASSERT_EQUAL_INT(POSEI_DET_FAM_FLOCK, rows[sel].family);
}

static void test_select_skips_dead_and_masked(void)
{
    const uint32_t now = 100000u;
    gz_radar_row_t rows[] = {
        row(POSEI_DET_FAM_FLOCK,  -50, 1, now - (GZ_RADAR_TTL_MS + 1)),     /* dead: too old */
        row(POSEI_DET_FAM_AIRTAG, -45, GZ_ALERT_TRACKER_HITS, now - 1000),  /* live tracker  */
    };
    int n = (int)(sizeof rows / sizeof rows[0]);
    /* the only live eligible row is the tracker */
    TEST_ASSERT_EQUAL_INT(1, gz_alert_select(rows, n, GZ_ALERT_MASK_ALL, now));
    /* mask out trackers -> nothing eligible is live -> -1 */
    uint16_t no_tr = (uint16_t)(GZ_ALERT_MASK_ALL & ~GZ_ALERT_BIT_TRACKER);
    TEST_ASSERT_EQUAL_INT(-1, gz_alert_select(rows, n, no_tr, now));
    /* empty table -> -1 */
    TEST_ASSERT_EQUAL_INT(-1, gz_alert_select(rows, 0, GZ_ALERT_MASK_ALL, now));
}

static void test_rate_limit_and_ttl(void)
{
    const uint32_t t0 = 500000u;
    /* never alerted before -> allowed regardless of last */
    TEST_ASSERT_TRUE(gz_alert_rate_ok(0, t0, /*ever=*/false));
    /* just alerted -> blocked until the cool-down elapses */
    TEST_ASSERT_FALSE(gz_alert_rate_ok(t0, t0 + 1000, true));
    TEST_ASSERT_FALSE(gz_alert_rate_ok(t0, t0 + GZ_ALERT_RELIMIT_MS - 1, true));
    TEST_ASSERT_TRUE (gz_alert_rate_ok(t0, t0 + GZ_ALERT_RELIMIT_MS,     true));
    /* overlay auto-clear at 60 s */
    TEST_ASSERT_FALSE(gz_alert_expired(t0, t0 + GZ_ALERT_TTL_MS - 1));
    TEST_ASSERT_TRUE (gz_alert_expired(t0, t0 + GZ_ALERT_TTL_MS));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_low_families_never_eligible);
    RUN_TEST(test_high_families_raise_and_respect_mask);
    RUN_TEST(test_trackers_need_repeats);
    RUN_TEST(test_select_prefers_surveillance_then_rssi);
    RUN_TEST(test_select_skips_dead_and_masked);
    RUN_TEST(test_rate_limit_and_ttl);
    return UNITY_END();
}
