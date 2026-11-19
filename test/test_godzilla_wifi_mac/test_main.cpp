/* Native Unity test for the radar/Recon MAC privacy flag + OUI vendor naming
 * (detection-breadth §2 item 4): gz_wifi_mac.h — locally-administered
 * (randomized) and multicast bit tests, OUI vendor lookup (verified IEEE
 * assignments; unknown never guessed), and the composed station label. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_wifi_mac.h"

void setUp(void) {}
void tearDown(void) {}

/* A global Espressif address (OUI 24:0A:C4): U/L bit clear, vendor named. */
static void test_global_vendor(void)
{
    const uint8_t mac[6] = { 0x24, 0x0A, 0xC4, 0x11, 0x22, 0x33 };
    TEST_ASSERT_FALSE(gz_mac_is_local(mac));
    TEST_ASSERT_FALSE(gz_mac_is_multicast(mac));
    TEST_ASSERT_EQUAL_HEX32(0x240AC4, gz_mac_oui(mac));
    TEST_ASSERT_EQUAL_STRING("Espressif", gz_mac_oui_vendor(gz_mac_oui(mac)));
    char s[24]; gz_mac_label(mac, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Espressif", s);
}

static void test_raspberry_pi(void)
{
    const uint8_t mac[6] = { 0xB8, 0x27, 0xEB, 0xAA, 0xBB, 0xCC };
    TEST_ASSERT_FALSE(gz_mac_is_local(mac));
    TEST_ASSERT_EQUAL_STRING("Raspberry Pi", gz_mac_oui_vendor(gz_mac_oui(mac)));
    char s[24]; gz_mac_label(mac, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Raspberry Pi", s);
}

/* A randomized MAC: octet 0 = 0xDA (…1010, U/L bit 0x02 set, multicast 0
 * clear) — reads "randomized", and the OUI is NOT looked up (would be noise). */
static void test_randomized(void)
{
    const uint8_t mac[6] = { 0xDA, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5 };
    TEST_ASSERT_TRUE(gz_mac_is_local(mac));
    TEST_ASSERT_FALSE(gz_mac_is_multicast(mac));
    char s[24]; gz_mac_label(mac, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("randomized", s);
}

/* The four locally-administered bit patterns in a nibble all read local:
 * 0x2, 0x6, 0xA, 0xE in the low nibble of octet 0. */
static void test_local_bit_patterns(void)
{
    const uint8_t lows[4] = { 0x02, 0x06, 0x0A, 0x0E };
    for (int i = 0; i < 4; ++i) {
        uint8_t mac[6] = { lows[i], 0, 0, 0, 0, 0 };
        TEST_ASSERT_TRUE(gz_mac_is_local(mac));
    }
    const uint8_t globals[4] = { 0x00, 0x04, 0x08, 0x0C };
    for (int i = 0; i < 4; ++i) {
        uint8_t mac[6] = { globals[i], 0, 0, 0, 0, 0 };
        TEST_ASSERT_FALSE(gz_mac_is_local(mac));
    }
}

/* Multicast bit independent of the local bit. */
static void test_multicast(void)
{
    const uint8_t mc[6]  = { 0x01, 0x00, 0x5E, 0, 0, 0 };  /* IPv4 multicast */
    TEST_ASSERT_TRUE(gz_mac_is_multicast(mc));
    const uint8_t uni[6] = { 0x24, 0x0A, 0xC4, 0, 0, 0 };
    TEST_ASSERT_FALSE(gz_mac_is_multicast(uni));
}

/* Unknown global OUI is never guessed — shows the raw OUI hex. */
static void test_unknown_oui(void)
{
    const uint8_t mac[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
    TEST_ASSERT_FALSE(gz_mac_is_local(mac));
    TEST_ASSERT_NULL(gz_mac_oui_vendor(gz_mac_oui(mac)));
    char s[24]; gz_mac_label(mac, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("OUI 001122", s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_global_vendor);
    RUN_TEST(test_raspberry_pi);
    RUN_TEST(test_randomized);
    RUN_TEST(test_local_bit_patterns);
    RUN_TEST(test_multicast);
    RUN_TEST(test_unknown_oui);
    return UNITY_END();
}
