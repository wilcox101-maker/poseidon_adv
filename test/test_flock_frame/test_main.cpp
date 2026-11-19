/* Native Unity tests for src/flock_frame.h — 802.11 management-frame Flock
 * check (GODZILLA reuse step 3), extracted from surveillance_hunter.cpp.
 * `len` is the captured length INCLUDING the 4-byte FCS, as rx_ctrl.sig_len
 * reports it. */
#include <unity.h>
#include <string.h>
#include "../../src/flock_frame.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t T1_MAC[6]  = { 0xB4, 0x1E, 0x52, 0x01, 0x02, 0x03 };  /* Flock anchor */
static const uint8_t T2_MAC[6]  = { 0xA4, 0xCF, 0x12, 0x01, 0x02, 0x03 };  /* tier 2 */
static const uint8_t ANY_MAC[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

/* Beacon (subtype 8) or probe response (5): 24B header + 12B fixed + tags + FCS. */
static int mk_beacon(uint8_t *f, uint8_t subtype, const uint8_t bssid[6], const char *ssid)
{
    memset(f, 0, 128);
    f[0] = (uint8_t)(subtype << 4);
    memset(f + 4, 0xFF, 6);
    memcpy(f + 10, bssid, 6);
    memcpy(f + 16, bssid, 6);
    int n = 36;
    size_t sl = strlen(ssid);
    f[n++] = 0; f[n++] = (uint8_t)sl; memcpy(f + n, ssid, sl); n += (int)sl;
    return n + 4;
}

/* Probe request (subtype 4): 24B header + tags + FCS; addr2 = source. */
static int mk_probe(uint8_t *f, const uint8_t src[6], const char *ssid)
{
    memset(f, 0, 128);
    f[0] = 0x40;
    memset(f + 4, 0xFF, 6);
    memcpy(f + 10, src, 6);
    memset(f + 16, 0xFF, 6);
    int n = 24;
    size_t sl = strlen(ssid);
    f[n++] = 0; f[n++] = (uint8_t)sl; memcpy(f + n, ssid, sl); n += (int)sl;
    return n + 4;
}

static void test_beacon_t1_oui(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, T1_MAC, "HomeNet");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_T1, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_MEMORY(T1_MAC, r.addr, 6);
    TEST_ASSERT_EQUAL_STRING("HomeNet", r.ssid);
    TEST_ASSERT_EQUAL_UINT8(8, r.subtype);
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_FLOCK, flock_frame_family(&r));
}

static void test_ssid_pattern(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 5, ANY_MAC, "flock-4F2A");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_SSID, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_FLOCK, flock_frame_family(&r));
}

/* Tier 2 + matching SSID upgrades to the SSID class (as today). */
static void test_t2_upgraded_by_ssid(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, T2_MAC, "FlockOS");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_SSID, flock_frame_classify(f, n, &r));
}

/* Tier 2 alone: still a hunter hit (behaviour preserved), but NOT a Flock
 * detection on the wire, which needs corroboration. */
static void test_t2_alone(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, T2_MAC, "ESP_1234");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_T2, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_WIFI_AP, flock_frame_family(&r));
}

/* DeFlockJoplin: wildcard (empty-SSID) probe from a Flock OUI. */
static void test_wildcard_probe(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_probe(f, T1_MAC, "");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_PROBE, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_MEMORY(T1_MAC, r.addr, 6);
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_FLOCK, flock_frame_family(&r));
    /* T2 wildcard probe is also the probe fingerprint today. */
    n = mk_probe(f, T2_MAC, "");
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_PROBE, flock_frame_classify(f, n, &r));
}

static void test_ordinary_frames(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, ANY_MAC, "CoffeeShop");
    TEST_ASSERT_EQUAL_INT(SURV_UNKNOWN, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_WIFI_AP, flock_frame_family(&r));
    n = mk_probe(f, ANY_MAC, "CoffeeShop");
    TEST_ASSERT_EQUAL_INT(SURV_UNKNOWN, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_WIFI_STA, flock_frame_family(&r));
}

/* Short probe requests must still parse (wildcard probes are tiny). */
static void test_minimal_probe(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_probe(f, T1_MAC, "");
    TEST_ASSERT_EQUAL_INT(24 + 2 + 4, n);
    TEST_ASSERT_EQUAL_INT(SURV_FLOCK_PROBE, flock_frame_classify(f, n, &r));
}

static void test_rejects(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, T1_MAC, "x");
    TEST_ASSERT_EQUAL_INT(-1, flock_frame_classify(f, 39, &r));     /* < 36 + 4 */
    TEST_ASSERT_EQUAL_INT(-1, flock_frame_classify(nullptr, n, &r));
    TEST_ASSERT_EQUAL_INT(-1, flock_frame_classify(f, n, nullptr));
    f[0] = 0xD0;                                                      /* action frame */
    TEST_ASSERT_EQUAL_INT(-1, flock_frame_classify(f, n, &r));
    f[0] = 0x88;                                                      /* data frame (type 2) */
    TEST_ASSERT_EQUAL_INT(-1, flock_frame_classify(f, n, &r));
}

/* SSID length past the captured tags is ignored, not read. */
static void test_ssid_overrun(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, ANY_MAC, "Flock-A");
    f[37] = 30;                                     /* claims 30, only 7 present */
    TEST_ASSERT_EQUAL_INT(SURV_UNKNOWN, flock_frame_classify(f, n, &r));
    TEST_ASSERT_EQUAL_STRING("", r.ssid);
    f[37] = 33;                                     /* > 32 is never valid */
    flock_frame_classify(f, 36 + 2 + 33 + 4, &r);
    TEST_ASSERT_EQUAL_STRING("", r.ssid);
}

/* A beacon carrying an ODID vendor IE (FA:0B:BC type 0x0D) is a drone:
 * family REMOTEID, ahead of AP. The hunter class is unaffected. */
static void test_odid_beacon_family(void)
{
    uint8_t f[128]; flock_frame_t r;
    int n = mk_beacon(f, 8, ANY_MAC, "DRONE");
    n -= 4;                                       /* drop FCS, append IE, re-add FCS */
    const uint8_t ie[] = { 221, 6, 0xFA, 0x0B, 0xBC, 0x0D, 0x01, 0xF2 };
    memcpy(f + n, ie, sizeof(ie)); n += (int)sizeof(ie); n += 4;
    TEST_ASSERT_EQUAL_INT(SURV_UNKNOWN, flock_frame_classify(f, n, &r));
    TEST_ASSERT_TRUE(r.odid);
    TEST_ASSERT_EQUAL_UINT8(DET_FAM_REMOTEID, flock_frame_family(&r));
    n = mk_beacon(f, 8, ANY_MAC, "plain");
    flock_frame_classify(f, n, &r);
    TEST_ASSERT_FALSE(r.odid);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_beacon_t1_oui);
    RUN_TEST(test_ssid_pattern);
    RUN_TEST(test_t2_upgraded_by_ssid);
    RUN_TEST(test_t2_alone);
    RUN_TEST(test_wildcard_probe);
    RUN_TEST(test_ordinary_frames);
    RUN_TEST(test_minimal_probe);
    RUN_TEST(test_rejects);
    RUN_TEST(test_ssid_overrun);
    RUN_TEST(test_odid_beacon_family);
    return UNITY_END();
}
