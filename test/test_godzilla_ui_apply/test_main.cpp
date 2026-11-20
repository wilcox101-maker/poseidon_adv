/* Native Unity test for the live-apply decision core (watch-faces, item I):
 * gz_ui_apply.h.
 *
 *   1. an identical save touches no consumer (so no needless watch rebuild);
 *   2. a finish-only change touches finish and not alert;
 *   3. an alert-mask change and an always-on change each touch alert only;
 *   4. a save that changes only non-live fields (accent, background, face,
 *      pods) touches nothing;
 *   5. a save that changes several fields reports every consumer involved.
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
}

static void test_always_on_only(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.always_on = (uint8_t)(a.always_on ? 0 : 1);
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_TRUE(c.alert);
}

static void test_non_live_fields_touch_nothing(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.accent     = (uint8_t)(a.accent + 1);
    b.background  = (uint8_t)(a.background == GZ_BG_BLACK ? GZ_BG_TEXTURE : GZ_BG_BLACK);
    b.face       = (uint8_t)GZ_FACE_COMMAND;
    b.slots[0]   = GZ_COMP_CLOCK;
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_FALSE(c.finish);
    TEST_ASSERT_FALSE(c.alert);
    TEST_ASSERT_FALSE(gz_ui_consumers_any(c));
}

static void test_combined_change_reports_all(void)
{
    gz_ui_settings_t a, b;
    base(&a); base(&b);
    b.finish     = (uint8_t)(a.finish == GZ_FINISH_GOLD ? GZ_FINISH_SILVER : GZ_FINISH_GOLD);
    b.alert_mask = (uint8_t)(a.alert_mask ^ 0x08u);
    b.accent     = (uint8_t)(a.accent + 1);           /* non-live, should not matter */
    gz_ui_consumers_t c = gz_ui_consumers_diff(&a, &b);
    TEST_ASSERT_TRUE(c.finish);
    TEST_ASSERT_TRUE(c.alert);
    TEST_ASSERT_TRUE(gz_ui_consumers_any(c));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_identical_touches_nothing);
    RUN_TEST(test_finish_only);
    RUN_TEST(test_alert_mask_only);
    RUN_TEST(test_always_on_only);
    RUN_TEST(test_non_live_fields_touch_nothing);
    RUN_TEST(test_combined_change_reports_all);
    return UNITY_END();
}
