/* Native Unity test for godzilla_node/main/gz_le_equip.h — the passive LE-
 * equipment OUI classifier (detection §4 unit 3):
 *   - the Axon Enterprise OUI (00:25:DF) -> LOW from a globally-administered MAC;
 *   - a randomized (locally-administered) MAC is never attributed -> NONE;
 *   - an unrelated OUI -> NONE;
 *   - there is no HIGH tier (the family is LOW by definition).
 * Pure host TU, same style as test_godzilla_wifi_mac. */
#include <unity.h>
#include "../../godzilla_node/main/gz_le_equip.h"

void setUp(void) {}
void tearDown(void) {}

static void mac_from_oui(uint32_t oui, uint8_t mac[6])
{
    mac[0] = (uint8_t)(oui >> 16);
    mac[1] = (uint8_t)(oui >> 8);
    mac[2] = (uint8_t)oui;
    mac[3] = 0xAA; mac[4] = 0xBB; mac[5] = 0xCC;
}

static void test_axon_oui_is_low(void)
{
    TEST_ASSERT_EQUAL_INT(GZ_LE_EQUIP_LOW, gz_le_equip_oui_conf(0x0025DF));
    uint8_t mac[6];
    mac_from_oui(0x0025DF, mac);
    TEST_ASSERT_EQUAL_INT(GZ_LE_EQUIP_LOW, gz_le_equip_classify_mac(mac));
}

static void test_randomized_mac_is_none(void)
{
    uint8_t mac[6];
    mac_from_oui(0x0025DF, mac);
    mac[0] |= GZ_MAC_LOCAL;      /* privacy-randomized -> not attributed */
    TEST_ASSERT_EQUAL_INT(GZ_LE_EQUIP_NONE, gz_le_equip_classify_mac(mac));
}

static void test_unrelated_oui_is_none(void)
{
    /* A Hikvision camera OUI is not LE equipment here. */
    TEST_ASSERT_EQUAL_INT(GZ_LE_EQUIP_NONE, gz_le_equip_oui_conf(0xACCB51));
    uint8_t mac[6];
    mac_from_oui(0xACCB51, mac);
    TEST_ASSERT_EQUAL_INT(GZ_LE_EQUIP_NONE, gz_le_equip_classify_mac(mac));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_axon_oui_is_low);
    RUN_TEST(test_randomized_mac_is_none);
    RUN_TEST(test_unrelated_oui_is_none);
    return UNITY_END();
}
