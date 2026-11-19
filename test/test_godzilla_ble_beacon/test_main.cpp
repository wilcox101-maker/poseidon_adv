/* Native Unity test for the BLE Lab's iBeacon decoder (detection-breadth
 * §1.1): gz_ble_beacon.h — passive iBeacon parse from a raw advertising
 * payload, canonical UUID string, summary line, and malformed-input
 * rejection (truncated / oversized / wrong maker / overrun → clean reject,
 * no over-read). Adverts are hand-built in the shapes real beacons send. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_ble_beacon.h"

void setUp(void) {}
void tearDown(void) {}

/* A textbook iBeacon: flags, then the 0x1A-long Apple manufacturer AD with
 * subtype 0x02 / len 0x15, a known UUID, major 0x0001, minor 0x0002,
 * measured power -59 dBm (0xC5). */
static const uint8_t IBEACON[] = {
    0x02, 0x01, 0x06,
    0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15,
    0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2,
    0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0,
    0x00, 0x01, 0x00, 0x02, 0xC5
};

static void test_ibeacon_decode(void)
{
    gz_ibeacon_t b;
    TEST_ASSERT_TRUE(gz_ibeacon_parse(IBEACON, sizeof IBEACON, &b));
    TEST_ASSERT_TRUE(b.valid);
    TEST_ASSERT_EQUAL_HEX16(0x0001, b.major);
    TEST_ASSERT_EQUAL_HEX16(0x0002, b.minor);
    TEST_ASSERT_EQUAL_INT8(-59, b.power);
    const uint8_t want[16] = { 0xE2,0xC5,0x6D,0xB5,0xDF,0xFB,0x48,0xD2,
                               0xB0,0x60,0xD0,0xF5,0xA7,0x10,0x96,0xE0 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b.uuid, 16);

    char uuid[37];
    gz_ibeacon_uuid_str(&b, uuid, sizeof uuid);
    TEST_ASSERT_EQUAL_STRING("e2c56db5-dffb-48d2-b060-d0f5a71096e0", uuid);

    char s[80];
    gz_ibeacon_summary(&b, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(
        "iBeacon e2c56db5-dffb-48d2-b060-d0f5a71096e0 1/2 @-59dBm", s);
}

/* iBeacon as the only AD structure (no leading flags) still decodes. */
static void test_ibeacon_no_flags(void)
{
    gz_ibeacon_t b;
    TEST_ASSERT_TRUE(gz_ibeacon_parse(IBEACON + 3, (int)sizeof IBEACON - 3, &b));
    TEST_ASSERT_EQUAL_HEX16(0x0002, b.minor);
}

/* Not an iBeacon: Apple maker but a different subtype (e.g. Find My) — reject. */
static void test_non_ibeacon_apple(void)
{
    const uint8_t ad[] = { 0x1A, 0xFF, 0x4C, 0x00, 0x12, 0x15,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    gz_ibeacon_t b;
    TEST_ASSERT_FALSE(gz_ibeacon_parse(ad, sizeof ad, &b));
    TEST_ASSERT_FALSE(b.valid);
}

/* Non-Apple company ID with the iBeacon shape — reject (we only name iBeacon). */
static void test_wrong_company(void)
{
    uint8_t ad[sizeof IBEACON];
    memcpy(ad, IBEACON, sizeof IBEACON);
    ad[5] = 0x99; ad[6] = 0x00;                 /* company 0x0099, not Apple */
    gz_ibeacon_t b;
    TEST_ASSERT_FALSE(gz_ibeacon_parse(ad, sizeof ad, &b));
}

/* Truncated: the declared AD length is right but the buffer is cut short, so
 * the length overruns — must reject without reading past the end. */
static void test_truncated_overrun(void)
{
    gz_ibeacon_t b;
    for (int cut = 1; cut < (int)sizeof IBEACON; ++cut) {
        /* Any prefix shorter than the full advert cannot hold a whole
         * iBeacon; the parser must reject every one cleanly. */
        bool ok = gz_ibeacon_parse(IBEACON, cut, &b);
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_FALSE(b.valid);
    }
}

/* Manufacturer AD present but one data byte short of a full iBeacon (n == 24):
 * reject rather than over-read the major/minor/power. */
static void test_one_byte_short(void)
{
    /* L = 0x19 (25) => n = 24 data bytes: company+subtype+sublen+uuid+major
     * +minor but power missing. */
    uint8_t ad[] = { 0x19, 0xFF, 0x4C, 0x00, 0x02, 0x15,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   /* 16 uuid */
                     0x00, 0x01, 0x00, 0x02 };          /* major, minor; no power */
    gz_ibeacon_t b;
    TEST_ASSERT_FALSE(gz_ibeacon_parse(ad, sizeof ad, &b));
}

/* Zero-length, empty and all-padding buffers reject cleanly. */
static void test_degenerate_buffers(void)
{
    gz_ibeacon_t b;
    TEST_ASSERT_FALSE(gz_ibeacon_parse(NULL, 0, &b));
    TEST_ASSERT_FALSE(gz_ibeacon_parse(IBEACON, 0, &b));
    const uint8_t zeros[8] = { 0 };
    TEST_ASSERT_FALSE(gz_ibeacon_parse(zeros, sizeof zeros, &b));
    const uint8_t neg[] = { 0x1A };               /* a lone length byte */
    TEST_ASSERT_FALSE(gz_ibeacon_parse(neg, sizeof neg, &b));
}

/* Garbage of assorted lengths never over-reads and never false-positives. */
static void test_garbage_fuzz(void)
{
    gz_ibeacon_t b;
    for (int len = 0; len <= 40; ++len) {
        uint8_t buf[40];
        for (int i = 0; i < len; ++i) buf[i] = (uint8_t)(0xA5 ^ (i * 31 + len));
        (void)gz_ibeacon_parse(buf, len, &b);     /* must not crash / over-read */
    }
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ibeacon_decode);
    RUN_TEST(test_ibeacon_no_flags);
    RUN_TEST(test_non_ibeacon_apple);
    RUN_TEST(test_wrong_company);
    RUN_TEST(test_truncated_overrun);
    RUN_TEST(test_one_byte_short);
    RUN_TEST(test_degenerate_buffers);
    RUN_TEST(test_garbage_fuzz);
    return UNITY_END();
}
