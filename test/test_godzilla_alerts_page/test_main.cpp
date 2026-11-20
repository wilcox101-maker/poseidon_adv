/* Native Unity test for the Alerts settings subpage pure model
 * (watch-faces customiser, unit 4; detection §4 item 6): gz_alerts_page.h.
 *
 *   1. row <-> enable-bit map: the seven family rows cover EXACTLY the seven
 *      GZ_ALERT_BIT_* bits (gz_alert.h), each once; Back and out-of-range rows
 *      carry no bit, so they can never flip the mask;
 *   2. toggle flips exactly one bit, is its own inverse, and Back is a no-op;
 *   3. "n/7 on" count tracks the mask, defaults all-on, and ignores stray bits;
 *   4. row labels are ASCII only (the node's LVGL build has no non-ASCII
 *      glyphs — #147/#150) and distinct.
 *
 * Ties the subpage to the one source of the bit meanings: the union of the row
 * bits must equal GZ_ALERT_MASK_ALL, so a new alerting family added in
 * gz_alert.h without a row here (or vice versa) fails this test. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_alerts_page.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- 1. row <-> bit map -------------------------------------------------- */
static void test_rows_cover_exactly_the_alert_bits(void)
{
    TEST_ASSERT_EQUAL_INT(7, GZ_ALERTS_FAMILY_COUNT);
    TEST_ASSERT_EQUAL_INT(GZ_ALERTS_FAMILY_COUNT, GZ_ALERTS_TOTAL);
    TEST_ASSERT_EQUAL_INT(GZ_ALERTS_FAMILY_COUNT, GZ_ALERTS_ROW_BACK);
    TEST_ASSERT_EQUAL_INT(GZ_ALERTS_FAMILY_COUNT + 1, GZ_ALERTS_ROW_COUNT);

    uint8_t seen = 0;
    for (int r = 0; r < GZ_ALERTS_FAMILY_COUNT; ++r) {
        uint8_t bit = gz_alerts_row_bit(r);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0u, bit, "family row has no bit");
        TEST_ASSERT_EQUAL_MESSAGE(0u, seen & bit, "two rows share a bit");
        seen |= bit;
    }
    /* every alerting bit has exactly one row, and no row covers a non-bit */
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_MASK_ALL, seen);

    /* Back and out-of-range never carry a bit (can't flip the mask) */
    TEST_ASSERT_EQUAL_HEX8(0u, gz_alerts_row_bit(GZ_ALERTS_ROW_BACK));
    TEST_ASSERT_EQUAL_HEX8(0u, gz_alerts_row_bit(-1));
    TEST_ASSERT_EQUAL_HEX8(0u, gz_alerts_row_bit(GZ_ALERTS_ROW_COUNT));
    TEST_ASSERT_FALSE(gz_alerts_row_is_family(GZ_ALERTS_ROW_BACK));
    TEST_ASSERT_FALSE(gz_alerts_row_is_family(-1));
    TEST_ASSERT_TRUE(gz_alerts_row_is_family(0));
    TEST_ASSERT_TRUE(gz_alerts_row_is_family(GZ_ALERTS_FAMILY_COUNT - 1));
}

static void test_row_bits_match_family_bits(void)
{
    /* the subpage's row bits are literally the gz_alert.h family bits */
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_FLOCK,      gz_alerts_row_bit(GZ_ALERTS_ROW_FLOCK));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_RAVEN,      gz_alerts_row_bit(GZ_ALERTS_ROW_RAVEN));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_REMOTEID,   gz_alerts_row_bit(GZ_ALERTS_ROW_REMOTEID));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_SKIMMER,    gz_alerts_row_bit(GZ_ALERTS_ROW_SKIMMER));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_CAMERA,     gz_alerts_row_bit(GZ_ALERTS_ROW_CAMERA));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_SMARTGLASS, gz_alerts_row_bit(GZ_ALERTS_ROW_SMARTGLASS));
    TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_BIT_TRACKER,    gz_alerts_row_bit(GZ_ALERTS_ROW_TRACKER));
}

/* ---- 2. toggle ----------------------------------------------------------- */
static void test_toggle_flips_one_bit_and_is_its_own_inverse(void)
{
    for (int r = 0; r < GZ_ALERTS_FAMILY_COUNT; ++r) {
        uint8_t bit = gz_alerts_row_bit(r);

        /* from all-on: one tap clears exactly that bit, leaves the rest */
        uint8_t m = GZ_ALERT_MASK_ALL;
        uint8_t m1 = gz_alerts_toggle(m, r);
        TEST_ASSERT_EQUAL_HEX8(0u, m1 & bit);                 /* that bit off */
        TEST_ASSERT_EQUAL_HEX8(GZ_ALERT_MASK_ALL & ~bit, m1); /* only that bit */
        TEST_ASSERT_TRUE(gz_alerts_row_on(m, r));
        TEST_ASSERT_FALSE(gz_alerts_row_on(m1, r));

        /* a second tap restores it (own inverse) */
        TEST_ASSERT_EQUAL_HEX8(m, gz_alerts_toggle(m1, r));
    }
}

static void test_back_toggle_is_noop(void)
{
    uint8_t m = 0x2A;
    TEST_ASSERT_EQUAL_HEX8(m, gz_alerts_toggle(m, GZ_ALERTS_ROW_BACK));
    TEST_ASSERT_EQUAL_HEX8(m, gz_alerts_toggle(m, -1));
    TEST_ASSERT_EQUAL_HEX8(m, gz_alerts_toggle(m, GZ_ALERTS_ROW_COUNT));
}

/* ---- 3. count ------------------------------------------------------------ */
static void test_count_tracks_mask(void)
{
    TEST_ASSERT_EQUAL_INT(7, gz_alerts_count_on(GZ_ALERT_MASK_ALL));   /* default all-on */
    TEST_ASSERT_EQUAL_INT(0, gz_alerts_count_on(0u));
    TEST_ASSERT_EQUAL_INT(1, gz_alerts_count_on(GZ_ALERT_BIT_FLOCK));
    TEST_ASSERT_EQUAL_INT(2, gz_alerts_count_on(GZ_ALERT_BIT_FLOCK | GZ_ALERT_BIT_TRACKER));

    /* clearing one family from all-on drops the count by one */
    uint8_t m = gz_alerts_toggle(GZ_ALERT_MASK_ALL, GZ_ALERTS_ROW_CAMERA);
    TEST_ASSERT_EQUAL_INT(6, gz_alerts_count_on(m));

    /* a stray bit above the seven defined families is not counted */
    TEST_ASSERT_EQUAL_INT(7, gz_alerts_count_on((uint8_t)(GZ_ALERT_MASK_ALL | 0x80u)));
}

/* ---- 4. labels: ASCII only, distinct ------------------------------------- */
static void test_labels_ascii_and_distinct(void)
{
    const char *names[GZ_ALERTS_ROW_COUNT];
    for (int r = 0; r < GZ_ALERTS_ROW_COUNT; ++r) {
        const char *s = gz_alerts_row_name(r);
        names[r] = s;
        TEST_ASSERT_TRUE_MESSAGE(s && s[0], "empty row label");
        for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
            TEST_ASSERT_TRUE_MESSAGE(*p >= 0x20 && *p < 0x7F, "non-ASCII row label");
    }
    for (int i = 0; i < GZ_ALERTS_ROW_COUNT; ++i)
        for (int j = i + 1; j < GZ_ALERTS_ROW_COUNT; ++j)
            TEST_ASSERT_TRUE_MESSAGE(strcmp(names[i], names[j]) != 0, "duplicate row label");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rows_cover_exactly_the_alert_bits);
    RUN_TEST(test_row_bits_match_family_bits);
    RUN_TEST(test_toggle_flips_one_bit_and_is_its_own_inverse);
    RUN_TEST(test_back_toggle_is_noop);
    RUN_TEST(test_count_tracks_mask);
    RUN_TEST(test_labels_ascii_and_distinct);
    return UNITY_END();
}
