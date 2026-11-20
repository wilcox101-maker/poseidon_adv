/* Native Unity test for godzilla_node/main/gz_accent.h — the shared accent
 * palette and polish primitives (watch-faces orders, unit 6):
 *   1. the accent palette: nine distinct colours, index wraps (never out of
 *      range), matches GZ_ACCENT_COUNT so the customiser and faces agree;
 *   2. always-on dim: 100% is unchanged, 0% is black, 50% halves each channel,
 *      and it clamps out-of-range percentages;
 *   3. the face-change fade ramp: 0 at the start, 255 at/after the duration,
 *      monotic in between, and a zero duration is instantly opaque.
 * Pure host TU. */
#include <unity.h>
#include "../../godzilla_node/main/gz_accent.h"
#include "../../godzilla_node/main/ui/faces/gz_faces.h"  /* GZ_ACCENT_COUNT */

void setUp(void) {}
void tearDown(void) {}

/* ---- 1. accent palette --------------------------------------------------- */
static void test_palette_count_matches_settings(void)
{
    TEST_ASSERT_EQUAL_INT(GZ_ACCENT_COUNT, GZ_ACCENT_N);
}

static void test_palette_distinct_and_wraps(void)
{
    for (int i = 0; i < GZ_ACCENT_N; ++i)
        for (int j = i + 1; j < GZ_ACCENT_N; ++j)
            TEST_ASSERT_TRUE(gz_accent_rgb(i) != gz_accent_rgb(j));   /* distinct */
    /* index wraps cleanly */
    TEST_ASSERT_EQUAL_HEX32(gz_accent_rgb(0), gz_accent_rgb(GZ_ACCENT_N));
    TEST_ASSERT_EQUAL_HEX32(gz_accent_rgb(1), gz_accent_rgb(GZ_ACCENT_N + 1));
    TEST_ASSERT_EQUAL_HEX32(gz_accent_rgb(GZ_ACCENT_N - 1), gz_accent_rgb(-1));
    /* in-range values are 24-bit colours */
    for (int i = 0; i < GZ_ACCENT_N; ++i)
        TEST_ASSERT_TRUE(gz_accent_rgb(i) <= 0xFFFFFFu);
}

/* ---- 2. always-on dim ---------------------------------------------------- */
static void test_dim_endpoints_and_half(void)
{
    const uint32_t c = 0x80C0FF;
    TEST_ASSERT_EQUAL_HEX32(c,        gz_dim_rgb(c, 100));   /* unchanged */
    TEST_ASSERT_EQUAL_HEX32(0x000000, gz_dim_rgb(c, 0));     /* black     */
    /* 50%: each channel halved (integer) -> 0x40 60 7F */
    TEST_ASSERT_EQUAL_HEX32(0x40607F, gz_dim_rgb(c, 50));
    /* clamps */
    TEST_ASSERT_EQUAL_HEX32(c,        gz_dim_rgb(c, 150));
    TEST_ASSERT_EQUAL_HEX32(0x000000, gz_dim_rgb(c, -20));
    /* white dimmed is grey of equal channels */
    TEST_ASSERT_EQUAL_HEX32(0x7F7F7F, gz_dim_rgb(0xFFFFFF, 50));
}

/* ---- 3. face-change fade ------------------------------------------------- */
static void test_fade_ramp(void)
{
    TEST_ASSERT_EQUAL_UINT8(0,   gz_fade_opa(0, 300));
    TEST_ASSERT_EQUAL_UINT8(255, gz_fade_opa(300, 300));
    TEST_ASSERT_EQUAL_UINT8(255, gz_fade_opa(900, 300));   /* past the end */
    TEST_ASSERT_EQUAL_UINT8(127, gz_fade_opa(150, 300));   /* midpoint ~127 */
    TEST_ASSERT_EQUAL_UINT8(255, gz_fade_opa(0, 0));       /* zero duration */
    /* monotonic non-decreasing */
    uint8_t prev = 0;
    for (uint32_t e = 0; e <= 300; e += 30) {
        uint8_t o = gz_fade_opa(e, 300);
        TEST_ASSERT_TRUE(o >= prev);
        prev = o;
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_palette_count_matches_settings);
    RUN_TEST(test_palette_distinct_and_wraps);
    RUN_TEST(test_dim_endpoints_and_half);
    RUN_TEST(test_fade_ramp);
    return UNITY_END();
}
