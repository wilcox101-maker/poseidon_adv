/* Native Unity test for godzilla_node/main/gz_alpr.h — the passive non-Flock
 * ALPR OUI classifier (detection §4 unit 4):
 *   - Genetec ALPR-maker OUIs -> LOW from a globally-administered MAC;
 *   - a randomized (locally-administered) MAC is never attributed -> NONE;
 *   - an unrelated OUI -> NONE;
 *   - the ELSAG IAB 24-bit prefix 70:B3:D5 is NOT matched (shared pool);
 *   - 00:0A:B1 (unrelated Tokyo "GENETEC Corporation") is NOT matched.
 * Pure host TU, same style as test_godzilla_wifi_mac. */
#include <unity.h>
#include "../../godzilla_node/main/gz_alpr.h"

void setUp(void) {}
void tearDown(void) {}

static void mac_from_oui(uint32_t oui, uint8_t mac[6])
{
    mac[0] = (uint8_t)(oui >> 16);
    mac[1] = (uint8_t)(oui >> 8);
    mac[2] = (uint8_t)oui;
    mac[3] = 0x10; mac[4] = 0x20; mac[5] = 0x30;
}

static void test_genetec_ouis_are_low(void)
{
    const uint32_t ouis[] = { 0x00BF15, 0x0CBF15 };
    for (unsigned i = 0; i < sizeof ouis / sizeof ouis[0]; ++i) {
        TEST_ASSERT_EQUAL_INT(GZ_ALPR_LOW, gz_alpr_oui_conf(ouis[i]));
        uint8_t mac[6];
        mac_from_oui(ouis[i], mac);
        TEST_ASSERT_EQUAL_INT(GZ_ALPR_LOW, gz_alpr_classify_mac(mac));
    }
}

static void test_randomized_mac_is_none(void)
{
    uint8_t mac[6];
    mac_from_oui(0x00BF15, mac);
    mac[0] |= GZ_MAC_LOCAL;
    TEST_ASSERT_EQUAL_INT(GZ_ALPR_NONE, gz_alpr_classify_mac(mac));
}

static void test_unrelated_and_shared_iab_are_none(void)
{
    /* Espressif OUI -> NONE. */
    TEST_ASSERT_EQUAL_INT(GZ_ALPR_NONE, gz_alpr_oui_conf(0x240AC4));
    /* ELSAG lives under the IEEE IAB pool 70:B3:D5 (shared); its 24-bit prefix
     * is deliberately NOT in the table, so it must not match. */
    TEST_ASSERT_EQUAL_INT(GZ_ALPR_NONE, gz_alpr_oui_conf(0x70B3D5));
    /* 00:0A:B1 "GENETEC Corporation" (Tokyo) is unrelated to Genetec Inc. */
    TEST_ASSERT_EQUAL_INT(GZ_ALPR_NONE, gz_alpr_oui_conf(0x000AB1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_genetec_ouis_are_low);
    RUN_TEST(test_randomized_mac_is_none);
    RUN_TEST(test_unrelated_and_shared_iab_are_none);
    return UNITY_END();
}
