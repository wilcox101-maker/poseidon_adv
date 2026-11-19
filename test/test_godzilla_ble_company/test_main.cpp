/* Native Unity test for the BLE Lab's company-ID -> vendor naming
 * (detection-breadth §1 item 2): gz_ble_company.h — SIG company-ID lookup,
 * known/unknown classification, and the "vendor - <type>" detail label
 * (known/unknown vendor, with/without a type). */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_ble_company.h"

void setUp(void) {}
void tearDown(void) {}

static void test_known_ids(void)
{
    TEST_ASSERT_EQUAL_STRING("Apple",             gz_ble_company_name(0x004C));
    TEST_ASSERT_EQUAL_STRING("Microsoft",         gz_ble_company_name(0x0006));
    TEST_ASSERT_EQUAL_STRING("Google",            gz_ble_company_name(0x00E0));
    TEST_ASSERT_EQUAL_STRING("Samsung",           gz_ble_company_name(0x0075));
    TEST_ASSERT_EQUAL_STRING("Nordic",            gz_ble_company_name(0x0059));
    TEST_ASSERT_EQUAL_STRING("Texas Instruments", gz_ble_company_name(0x000D));
    TEST_ASSERT_EQUAL_STRING("Intel",             gz_ble_company_name(0x0002));
    TEST_ASSERT_EQUAL_STRING("Nokia",             gz_ble_company_name(0x0001));
    TEST_ASSERT_EQUAL_STRING("IBM",               gz_ble_company_name(0x0003));
    TEST_ASSERT_EQUAL_STRING("Broadcom",          gz_ble_company_name(0x000F));
    TEST_ASSERT_TRUE(gz_ble_company_known(0x004C));
}

/* Unknown IDs are never guessed. */
static void test_unknown_ids(void)
{
    TEST_ASSERT_NULL(gz_ble_company_name(0x1234));
    TEST_ASSERT_NULL(gz_ble_company_name(0x0000));
    TEST_ASSERT_NULL(gz_ble_company_name(0xFFFF));
    TEST_ASSERT_FALSE(gz_ble_company_known(0x1234));
}

/* Consistency with the older short table in gz_ble_adv.h: the four it named
 * still resolve to the same vendor strings. */
static void test_superset_of_adv_table(void)
{
    TEST_ASSERT_EQUAL_STRING("Microsoft", gz_ble_company_name(0x0006));
    TEST_ASSERT_EQUAL_STRING("Apple",     gz_ble_company_name(0x004C));
    TEST_ASSERT_EQUAL_STRING("Samsung",   gz_ble_company_name(0x0075));
    TEST_ASSERT_EQUAL_STRING("Google",    gz_ble_company_name(0x00E0));
}

static void test_vendor_label(void)
{
    char s[48];

    gz_ble_vendor_label(0x004C, "keyboard", s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Apple - keyboard", s);

    gz_ble_vendor_label(0x004C, NULL, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Apple", s);

    gz_ble_vendor_label(0x004C, "", s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Apple", s);

    gz_ble_vendor_label(0x1234, "mouse", s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("mfr 1234 - mouse", s);

    gz_ble_vendor_label(0x1234, NULL, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("mfr 1234", s);
}

/* The label never overflows a small buffer. */
static void test_label_bounded(void)
{
    char s[8];
    int w = gz_ble_vendor_label(0x1234, "somelongtype", s, sizeof s);
    TEST_ASSERT_TRUE(w > 0);                 /* snprintf returns intended len */
    TEST_ASSERT_TRUE(strlen(s) < sizeof s);  /* but wrote within the buffer   */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_ids);
    RUN_TEST(test_unknown_ids);
    RUN_TEST(test_superset_of_adv_table);
    RUN_TEST(test_vendor_label);
    RUN_TEST(test_label_bounded);
    return UNITY_END();
}
