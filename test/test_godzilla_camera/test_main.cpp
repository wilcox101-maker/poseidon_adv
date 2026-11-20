/* Native Unity test for godzilla_node/main/gz_camera.h — the passive IP-camera
 * OUI classifier (detection §4 unit 2):
 *   - OUIs of dedicated camera makers (Hikvision / Dahua / Amcrest / Ezviz) ->
 *     HIGH, from a globally-administered MAC;
 *   - a randomized (locally-administered) MAC is never attributed -> NONE;
 *   - an unrelated global OUI -> NONE;
 *   - the excluded Amcrest MA-M prefix 34:46:63 -> NONE (shared block);
 *   - the bare-OUI lookup agrees with the MAC classifier.
 * Pure host TU, same style as test_godzilla_wifi_mac. */
#include <unity.h>
#include "../../godzilla_node/main/gz_camera.h"

void setUp(void) {}
void tearDown(void) {}

/* Build a MAC from a 24-bit OUI + a fixed device tail. */
static void mac_from_oui(uint32_t oui, uint8_t mac[6])
{
    mac[0] = (uint8_t)(oui >> 16);
    mac[1] = (uint8_t)(oui >> 8);
    mac[2] = (uint8_t)oui;
    mac[3] = 0x11; mac[4] = 0x22; mac[5] = 0x33;
}

static void test_dedicated_camera_ouis_are_high(void)
{
    const uint32_t ouis[] = {
        0xACCB51,  /* Hikvision */
        0xE02EFE,  /* Dahua     */
        0x00651E,  /* Amcrest   */
        0x94EC13,  /* Ezviz     */
    };
    for (unsigned i = 0; i < sizeof ouis / sizeof ouis[0]; ++i) {
        TEST_ASSERT_EQUAL_INT(GZ_CAMERA_HIGH, gz_camera_oui_conf(ouis[i]));
        uint8_t mac[6];
        mac_from_oui(ouis[i], mac);
        TEST_ASSERT_EQUAL_INT(GZ_CAMERA_HIGH, gz_camera_classify_mac(mac));
    }
}

static void test_randomized_mac_is_never_attributed(void)
{
    /* Even if the OUI bytes coincide with a camera prefix, the locally-
     * administered bit means it is a privacy-randomized address, not that
     * vendor. Set bit 1 (0x02) of octet 0. */
    uint8_t mac[6];
    mac_from_oui(0xACCB51, mac);
    mac[0] |= GZ_MAC_LOCAL;
    TEST_ASSERT_EQUAL_INT(GZ_CAMERA_NONE, gz_camera_classify_mac(mac));
}

static void test_unrelated_oui_is_none(void)
{
    /* Espressif's OUI (not a camera maker) -> NONE. */
    TEST_ASSERT_EQUAL_INT(GZ_CAMERA_NONE, gz_camera_oui_conf(0x240AC4));
    uint8_t mac[6];
    mac_from_oui(0x240AC4, mac);
    TEST_ASSERT_EQUAL_INT(GZ_CAMERA_NONE, gz_camera_classify_mac(mac));
}

static void test_amcrest_ma_m_prefix_excluded(void)
{
    /* 34:46:63 is Amcrest's MA-M 24-bit prefix, shared with other MA-M
     * registrants, so it is NOT in the table — matching it would misattribute. */
    TEST_ASSERT_EQUAL_INT(GZ_CAMERA_NONE, gz_camera_oui_conf(0x344663));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dedicated_camera_ouis_are_high);
    RUN_TEST(test_randomized_mac_is_never_attributed);
    RUN_TEST(test_unrelated_oui_is_none);
    RUN_TEST(test_amcrest_ma_m_prefix_excluded);
    return UNITY_END();
}
