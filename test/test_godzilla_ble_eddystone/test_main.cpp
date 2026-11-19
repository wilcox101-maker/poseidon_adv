/* Native Unity test for the BLE Lab's Eddystone decoder (detection-breadth
 * §1.1): gz_ble_eddystone.h — passive UID/URL/TLM/EID parse from a raw
 * advertising payload, URL scheme/suffix expansion, summary lines, and
 * malformed-input rejection (truncated / oversized / wrong UUID / reserved
 * scheme / garbage → clean reject, no over-read). Adverts are hand-built in
 * the shapes real Eddystone beacons send (Google Eddystone spec). */
#include <unity.h>
#include <string.h>
#include <math.h>
#include "../../godzilla_node/main/gz_ble_eddystone.h"

void setUp(void) {}
void tearDown(void) {}

/* Flags, 0xFEAA service-UUID list, then UID service data: tx -20 (0xEC),
 * namespace 00..09, instance AA..AF. */
static const uint8_t UID[] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0xAA, 0xFE,
    0x17, 0x16, 0xAA, 0xFE, 0x00, 0xEC,
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,   /* namespace */
    0xAA,0xBB,0xCC,0xDD,0xEE,0xAF,                        /* instance  */
    0x00, 0x00                                            /* rfu       */
};

static void test_uid(void)
{
    gz_eddystone_t b;
    TEST_ASSERT_TRUE(gz_eddystone_parse(UID, sizeof UID, &b));
    TEST_ASSERT_TRUE(b.valid);
    TEST_ASSERT_EQUAL_HEX8(GZ_EDDY_UID, b.frame);
    TEST_ASSERT_TRUE(b.has_tx);
    TEST_ASSERT_EQUAL_INT8(-20, b.tx_power);
    const uint8_t ns[10] = {0,1,2,3,4,5,6,7,8,9};
    const uint8_t in[6]  = {0xAA,0xBB,0xCC,0xDD,0xEE,0xAF};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ns, b.namespace_id, 10);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(in, b.instance_id, 6);
    char s[96]; gz_eddystone_summary(&b, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(
        "Eddystone UID 00010203040506070809/aabbccddeeaf", s);
}

/* URL: https:// (scheme 0x03) + "example" + ".com/" (suffix 0x00). */
static void test_url(void)
{
    const uint8_t ad[] = {
        0x03, 0x03, 0xAA, 0xFE,
        0x0E, 0x16, 0xAA, 0xFE, 0x10, 0x12, 0x03,
        'e','x','a','m','p','l','e', 0x00
    };
    gz_eddystone_t b;
    TEST_ASSERT_TRUE(gz_eddystone_parse(ad, sizeof ad, &b));
    TEST_ASSERT_EQUAL_HEX8(GZ_EDDY_URL, b.frame);
    TEST_ASSERT_EQUAL_INT8(0x12, b.tx_power);
    TEST_ASSERT_EQUAL_STRING("https://example.com/", b.url);
    char s[96]; gz_eddystone_summary(&b, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Eddystone URL https://example.com/", s);
}

/* URL with a reserved scheme byte rejects cleanly. */
static void test_url_reserved_scheme(void)
{
    const uint8_t ad[] = {
        0x06, 0x16, 0xAA, 0xFE, 0x10, 0x00, 0x7F   /* scheme 0x7F reserved */
    };
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(ad, sizeof ad, &b));
}

/* URL carrying a non-printable byte in the host part rejects. */
static void test_url_nonprintable(void)
{
    const uint8_t ad[] = {
        0x09, 0x16, 0xAA, 0xFE, 0x10, 0x00, 0x03, 'a', 0x1F, 'b'
    };
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(ad, sizeof ad, &b));
}

/* TLM: version 0, 3300 mV, +25.5 C (0x1980 = 6528/256), adv 0x00000100,
 * sec 0x00001000. */
static void test_tlm(void)
{
    const uint8_t ad[] = {
        0x11, 0x16, 0xAA, 0xFE, 0x20, 0x00,
        0x0C, 0xE4,                     /* vbatt 3300 mV */
        0x19, 0x80,                     /* temp 25.5 C   */
        0x00, 0x00, 0x01, 0x00,         /* adv_cnt 256   */
        0x00, 0x00, 0x10, 0x00          /* sec_cnt 4096  */
    };
    gz_eddystone_t b;
    TEST_ASSERT_TRUE(gz_eddystone_parse(ad, sizeof ad, &b));
    TEST_ASSERT_EQUAL_HEX8(GZ_EDDY_TLM, b.frame);
    TEST_ASSERT_EQUAL_UINT16(3300, b.vbatt_mv);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.5f, b.temp_c);
    TEST_ASSERT_EQUAL_UINT32(256, b.adv_cnt);
    TEST_ASSERT_EQUAL_UINT32(4096, b.sec_cnt);
}

/* TLM negative temperature: 0xFF00 = -1.0 C. */
static void test_tlm_negative_temp(void)
{
    const uint8_t ad[] = {
        0x11, 0x16, 0xAA, 0xFE, 0x20, 0x00,
        0x0C, 0xE4, 0xFF, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02
    };
    gz_eddystone_t b;
    TEST_ASSERT_TRUE(gz_eddystone_parse(ad, sizeof ad, &b));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, b.temp_c);
}

/* EID: present only, 8-byte id, value not resolved. */
static void test_eid(void)
{
    const uint8_t ad[] = {
        0x0D, 0x16, 0xAA, 0xFE, 0x30, 0xEC,
        0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
    };
    gz_eddystone_t b;
    TEST_ASSERT_TRUE(gz_eddystone_parse(ad, sizeof ad, &b));
    TEST_ASSERT_EQUAL_HEX8(GZ_EDDY_EID, b.frame);
    const uint8_t want[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b.eid, 8);
    char s[64]; gz_eddystone_summary(&b, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Eddystone EID (present)", s);
}

/* Wrong service UUID (not 0xFEAA) is not Eddystone. */
static void test_wrong_uuid(void)
{
    const uint8_t ad[] = {
        0x17, 0x16, 0xAB, 0xFE, 0x00, 0xEC,
        0,1,2,3,4,5,6,7,8,9, 0xAA,0xBB,0xCC,0xDD,0xEE,0xAF, 0,0
    };
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(ad, sizeof ad, &b));
}

/* Unknown frame type rejects. */
static void test_unknown_frame(void)
{
    const uint8_t ad[] = { 0x04, 0x16, 0xAA, 0xFE, 0x40 };
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(ad, sizeof ad, &b));
}

/* Every truncated prefix of a valid UID advert rejects without over-read. */
static void test_truncated_overrun(void)
{
    gz_eddystone_t b;
    for (int cut = 1; cut < (int)sizeof UID; ++cut) {
        bool ok = gz_eddystone_parse(UID, cut, &b);
        if (ok) {
            /* The only self-contained decodable prefix would still have to
             * carry a whole frame; UID needs the full body, so none should
             * decode short. */
            TEST_FAIL_MESSAGE("short UID prefix decoded");
        }
    }
    TEST_PASS();
}

/* TLM one byte short rejects (no over-read of sec_cnt). */
static void test_tlm_short(void)
{
    const uint8_t ad[] = {
        0x10, 0x16, 0xAA, 0xFE, 0x20, 0x00,
        0x0C, 0xE4, 0x19, 0x80,
        0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x10            /* sec_cnt missing a byte */
    };
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(ad, sizeof ad, &b));
}

/* Degenerate and garbage buffers never crash or over-read. */
static void test_degenerate_and_fuzz(void)
{
    gz_eddystone_t b;
    TEST_ASSERT_FALSE(gz_eddystone_parse(NULL, 0, &b));
    TEST_ASSERT_FALSE(gz_eddystone_parse(UID, 0, &b));
    const uint8_t zeros[12] = {0};
    TEST_ASSERT_FALSE(gz_eddystone_parse(zeros, sizeof zeros, &b));
    for (int len = 0; len <= 48; ++len) {
        uint8_t buf[48];
        for (int i = 0; i < len; ++i) buf[i] = (uint8_t)(0x5A ^ (i * 17 + len));
        (void)gz_eddystone_parse(buf, len, &b);
    }
    /* A buffer whose first AD claims 0xFEAA service data but is garbage body. */
    const uint8_t weird[] = { 0x04, 0x16, 0xAA, 0xFE, 0x10 };  /* URL, no scheme */
    TEST_ASSERT_FALSE(gz_eddystone_parse(weird, sizeof weird, &b));
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uid);
    RUN_TEST(test_url);
    RUN_TEST(test_url_reserved_scheme);
    RUN_TEST(test_url_nonprintable);
    RUN_TEST(test_tlm);
    RUN_TEST(test_tlm_negative_temp);
    RUN_TEST(test_eid);
    RUN_TEST(test_wrong_uuid);
    RUN_TEST(test_unknown_frame);
    RUN_TEST(test_truncated_overrun);
    RUN_TEST(test_tlm_short);
    RUN_TEST(test_degenerate_and_fuzz);
    return UNITY_END();
}
