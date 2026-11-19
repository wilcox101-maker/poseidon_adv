/* Native Unity test for the passive Wi-Fi device-family tag (detection-breadth
 * §2 item 5): gz_wifi_family.h — classify a management frame's 802.11 IE block
 * into AP / IoT / unknown on unambiguous signals only (no guessing), with a
 * safe IE-walk (truncated / oversized / garbage → unknown, no over-read).
 * IE blocks are hand-built in the shapes real frames carry. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_wifi_family.h"

void setUp(void) {}
void tearDown(void) {}

/* A beacon IE block: SSID, supported rates, DS param, then TIM (id 5) —
 * TIM is the unambiguous AP marker. */
static const uint8_t BEACON[] = {
    0x00, 0x04, 'H','o','m','e',          /* SSID "Home"           */
    0x01, 0x04, 0x82, 0x84, 0x8B, 0x96,   /* supported rates       */
    0x03, 0x01, 0x06,                      /* DS param, channel 6   */
    0x05, 0x04, 0x00, 0x01, 0x00, 0x00    /* TIM                   */
};

static void test_ap_from_tim(void)
{
    gz_wifi_family_t f = gz_wifi_family_classify(BEACON, sizeof BEACON);
    TEST_ASSERT_EQUAL_INT(GZ_FAM_AP, f);
    TEST_ASSERT_EQUAL_STRING("AP", gz_wifi_family_name(f));
}

/* A probe request from an Espressif station: SSID wildcard, rates, then a
 * vendor IE with the Espressif OUI 18:FE:34 and no TIM -> IoT. */
static void test_iot_from_espressif(void)
{
    const uint8_t pr[] = {
        0x00, 0x00,                                   /* wildcard SSID     */
        0x01, 0x04, 0x02, 0x04, 0x0B, 0x16,           /* supported rates   */
        0xDD, 0x05, 0x18, 0xFE, 0x34, 0x01, 0xAA      /* vendor Espressif  */
    };
    gz_wifi_family_t f = gz_wifi_family_classify(pr, sizeof pr);
    TEST_ASSERT_EQUAL_INT(GZ_FAM_IOT, f);
    TEST_ASSERT_EQUAL_STRING("IoT", gz_wifi_family_name(f));
}

/* An Espressif device acting as an AP (TIM present) is an AP, not IoT —
 * the unambiguous AP marker wins. */
static void test_tim_wins_over_vendor(void)
{
    const uint8_t ap[] = {
        0x05, 0x04, 0x00, 0x01, 0x00, 0x00,           /* TIM               */
        0xDD, 0x05, 0x18, 0xFE, 0x34, 0x01, 0xAA      /* vendor Espressif  */
    };
    TEST_ASSERT_EQUAL_INT(GZ_FAM_AP, gz_wifi_family_classify(ap, sizeof ap));
}

/* A generic probe request (SSID + rates + HT caps, no discriminating marker)
 * is left "unknown" — never guessed as phone or laptop. */
static void test_unknown_not_guessed(void)
{
    const uint8_t pr[] = {
        0x00, 0x00,
        0x01, 0x08, 0x02, 0x04, 0x0B, 0x16, 0x0C, 0x12, 0x18, 0x24,
        0x2D, 0x02, 0x21, 0x00                         /* HT caps (truncated ok for walk) */
    };
    gz_wifi_family_t f = gz_wifi_family_classify(pr, sizeof pr);
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, f);
    TEST_ASSERT_EQUAL_STRING("unknown", gz_wifi_family_name(f));
}

/* A non-Espressif vendor IE is not IoT (OUI must match exactly). */
static void test_other_vendor_not_iot(void)
{
    const uint8_t pr[] = {
        0x00, 0x00,
        0xDD, 0x05, 0x00, 0x50, 0xF2, 0x02, 0x00      /* Microsoft WMM OUI */
    };
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, gz_wifi_family_classify(pr, sizeof pr));
}

/* Every truncated prefix of the beacon never over-reads; it may drop the TIM
 * (then unknown) but must never crash or misclassify. */
static void test_truncated_overrun(void)
{
    for (int cut = 0; cut <= (int)sizeof BEACON; ++cut) {
        gz_wifi_family_t f = gz_wifi_family_classify(BEACON, cut);
        /* The TIM element occupies the last 6 bytes (indices 15..20); it is
         * only fully present in the complete block, so AP appears only then. */
        if (cut >= (int)sizeof BEACON) TEST_ASSERT_EQUAL_INT(GZ_FAM_AP, f);
        else                           TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, f);
    }
}

/* A vendor IE whose length overruns the buffer is skipped safely. */
static void test_vendor_len_overrun(void)
{
    const uint8_t pr[] = {
        0x00, 0x00,
        0xDD, 0x7F, 0x18, 0xFE, 0x34                   /* claims 0x7F, only 5 present */
    };
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, gz_wifi_family_classify(pr, sizeof pr));
}

/* Degenerate and garbage buffers never crash, over-read or misclassify. */
static void test_degenerate_and_fuzz(void)
{
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, gz_wifi_family_classify(NULL, 0));
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, gz_wifi_family_classify(BEACON, 0));
    const uint8_t zeros[8] = { 0 };   /* id 0 len 0 ... all empty SSIDs */
    TEST_ASSERT_EQUAL_INT(GZ_FAM_UNKNOWN, gz_wifi_family_classify(zeros, sizeof zeros));
    for (int len = 0; len <= 48; ++len) {
        uint8_t buf[48];
        for (int i = 0; i < len; ++i) buf[i] = (uint8_t)(0x91 ^ (i * 37 + len));
        (void)gz_wifi_family_classify(buf, len);
    }
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ap_from_tim);
    RUN_TEST(test_iot_from_espressif);
    RUN_TEST(test_tim_wins_over_vendor);
    RUN_TEST(test_unknown_not_guessed);
    RUN_TEST(test_other_vendor_not_iot);
    RUN_TEST(test_truncated_overrun);
    RUN_TEST(test_vendor_len_overrun);
    RUN_TEST(test_degenerate_and_fuzz);
    return UNITY_END();
}
