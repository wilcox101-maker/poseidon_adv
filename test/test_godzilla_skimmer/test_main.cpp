/* Native Unity test for godzilla_node/main/gz_skimmer.h — the passive BT card-
 * skimmer classifier (detection §4 unit 1). Captured-style advert bytes drive
 * every path:
 *   - a stock HM-10 ("HMSoft") and HC-08 advert with the 0xFFE0 serial service
 *     -> HIGH (name + UUID agree), including the service-data form;
 *   - the 0xFFE0 service with an unknown name -> LOW;
 *   - a known default name WITHOUT the service -> NONE (no name-only matching);
 *   - an unrelated advert and a malformed one -> NONE.
 * Pure host TU, same style as test_godzilla_ble_adv. */
#include <unity.h>
#include "../../godzilla_node/main/gz_skimmer.h"

void setUp(void) {}
void tearDown(void) {}

/* AD structure bytes. 0xFFE0 is little-endian E0 FF on the wire. */
#define FLAGS           0x02, 0x01, 0x06
#define UUID16_FFE0     0x03, 0x03, 0xE0, 0xFF          /* Complete 16-bit UUIDs */
#define NAME_HMSOFT     0x07, 0x09, 'H','M','S','o','f','t'
#define NAME_HC08       0x06, 0x09, 'H','C','-','0','8'
#define NAME_UNKNOWN    0x07, 0x09, 'M','y','U','A','R','T'

static int classify(const uint8_t *p, int n) { return gz_skimmer_classify_payload(p, n); }

static void test_stock_module_name_and_uuid_is_high(void)
{
    const uint8_t hm10[] = { FLAGS, NAME_HMSOFT, UUID16_FFE0 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_HIGH, classify(hm10, sizeof hm10));

    const uint8_t hc08[] = { FLAGS, NAME_HC08, UUID16_FFE0 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_HIGH, classify(hc08, sizeof hc08));
}

static void test_service_data_form_is_high(void)
{
    /* Some modules carry 0xFFE0 as service data (type 0x16: uuid LE then data). */
    const uint8_t sd[] = { FLAGS, NAME_HC08, 0x04, 0x16, 0xE0, 0xFF, 0x01 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_HIGH, classify(sd, sizeof sd));
}

static void test_service_without_known_name_is_low(void)
{
    const uint8_t low[] = { FLAGS, NAME_UNKNOWN, UUID16_FFE0 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_LOW, classify(low, sizeof low));
    /* Service present, no name at all -> still LOW (generic BLE-UART). */
    const uint8_t noname[] = { FLAGS, UUID16_FFE0 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_LOW, classify(noname, sizeof noname));
}

static void test_name_only_is_not_flagged(void)
{
    /* A known default name but no 0xFFE0 service: NOT a match (no name-only). */
    const uint8_t name_only[] = { FLAGS, NAME_HMSOFT };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_NONE, classify(name_only, sizeof name_only));
}

static void test_unrelated_and_malformed_are_none(void)
{
    /* Heart-rate sensor "Polar": HR service 0x180D, no 0xFFE0. */
    const uint8_t hr[] = { FLAGS, 0x06, 0x09, 'P','o','l','a','r', 0x03, 0x03, 0x0D, 0x18 };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_NONE, classify(hr, sizeof hr));

    /* A length byte that overruns the buffer -> parser flags malformed -> NONE. */
    const uint8_t bad[] = { 0x02, 0x01, 0x06, 0x1F, 0x09, 'X' };
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_NONE, classify(bad, sizeof bad));

    /* Empty payload. */
    TEST_ASSERT_EQUAL_INT(GZ_SKIMMER_NONE, classify(hr, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stock_module_name_and_uuid_is_high);
    RUN_TEST(test_service_data_form_is_high);
    RUN_TEST(test_service_without_known_name_is_low);
    RUN_TEST(test_name_only_is_not_flagged);
    RUN_TEST(test_unrelated_and_malformed_are_none);
    return UNITY_END();
}
