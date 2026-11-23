/* Native Unity test for the live-apply decision core (watch-faces, items I+L):
 * gz_ui_apply.h.
 *
 *   1. an identical save touches no consumer (so no needless rebuild);
 *   2. a finish-only change touches finish and nothing else;
 *   3. an alert-mask change and an always-on change each touch alert only;
 *   4. accent, background and the selected face each touch the face consumer
 *      only (item L made them live);
 *   5. a pod-only change (no live consumer) still touches nothing;
 *   6. a save that changes several fields reports every consumer involved.
 *
 * gz_ui_settings_defaults gives a known starting point; each case mutates a
 * copy so the diff is exercised field by field. */
#include <unity.h>
#include "../../godzilla_node/main/gz_ui_apply.h"

void setUp(void) {}
void tearDown(void) {}

static void base(gz_ui_settings_t *s) { gz_ui_settings_defaults(s); }

static void test_identical_touches_nothing(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_FALSE(c.face);
    TEST_ASSERT_FALSE(gz_ui_consumers_any(c));
}

static void test_finish_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.finish = (uint8_t)(a.finish == GZ_FINISH_GOLD ? GZ_FINISH_SILVER : GZ_FINISH_GOLD);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_TRUE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_FALSE(c.face);
    TEST_ASSERT_TRUE(gz_ui_consumers_any(c));
}

static void test_alert_mask_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.alert_mask = (uint8_t)(a.alert_mask ^ 0x01u);   /* flip one family bit */
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_TRUE(c.alert);
    TEST_ASSERT_FALSE(c.face);
}

static void test_always_on_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.always_on = (uint8_t)(a.always_on ? 0 : 1);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_TRUE(c.alert);
    TEST_ASSERT_FALSE(c.face);
}

static void test_accent_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.accent = (uint8_t)(a.accent + 1);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_TRUE(c.face);
    TEST_ASSERT_TRUE(gz_ui_consumers_any(c));
}

static void test_background_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.background = (uint8_t)(a.background == GZ_BG_BLACK ? GZ_BG_TEXTURE : GZ_BG_BLACK);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_TRUE(c.face);
}

static void test_face_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.face = (uint8_t)(a.face == GZ_FACE_COMMAND ? GZ_FACE_MODULAR : GZ_FACE_COMMAND);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_TRUE(c.face);
}

static void test_pod_only_touches_nothing(void)
{
    /* A slot/complication swap has no live consumer: the pods are read only
     * when a face screen is (re)built, so a pod-only save must move nothing. */
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.slots[0] = (uint8_t)(a.slots[0] + 1);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_FALSE(c.face);
    TEST_ASSERT_FALSE(gz_ui_consumers_any(c));
}

static void test_combined_change_reports_all(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.finish     = (uint8_t)(a.finish == GZ_FINISH_GOLD ? GZ_FINISH_SILVER : GZ_FINISH_GOLD);
    b.alert_mask = (uint8_t)(a.alert_mask ^ 0x08u);
    b.accent     = (uint8_t)(a.accent + 1);           /* now a live (face) consumer */
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_TRUE(c.finish);
    TEST_ASSERT_TRUE(c.alert);
    TEST_ASSERT_TRUE(c.face);
    TEST_ASSERT_TRUE(gz_ui_consumers_any(c));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_identical_touches_nothing);
    RUN_TEST(test_finish_only);
    RUN_TEST(test_alert_mask_only);
    RUN_TEST(test_always_on_only);
    RUN_TEST(test_accent_only);
    RUN_TEST(test_background_only);
    RUN_TEST(test_face_only);
    RUN_TEST(test_pod_only_touches_nothing);
    RUN_TEST(test_combined_change_reports_all);
    return UNITY_END();
}
