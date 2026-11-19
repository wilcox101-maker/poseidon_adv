/* Native Unity test for the BLE Lab's AltBeacon decoder (detection-breadth
 * §1.1): gz_ble_altbeacon.h — passive AltBeacon parse from a raw advertising
 * payload, beacon-ID hex string, summary line, and malformed-input rejection
 * (truncated / oversized / missing beacon code / garbage → clean reject, no
 * over-read). Adverts are hand-built per the AltBeacon spec. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_ble_altbeacon.h"

void setUp(void) {}
void tearDown(void) {}

/* Flags, then the 0x1B-long 0xFF manufacturer AD: MFG ID 0x0118 (little-
 * endian 18 01), beacon code BE AC, 20-byte ID, ref RSSI -59 (0xC5),
 * reserved 0x00. */
static const uint8_t ALT[] = {
    0x02, 0x01, 0x06,
    0x1B, 0xFF, 0x18, 0x01, 0xBE, 0xAC,
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,   /* id[0..9]  */
    0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,   /* id[10..19]*/
    0xC5, 0x00                                           /* rssi, rsvd */
};

static void test_altbeacon_decode(void)
{
    gz_altbeacon_t b;
    TEST_ASSERT_TRUE(gz_altbeacon_parse(ALT, sizeof ALT, &b));
    TEST_ASSERT_TRUE(b.valid);
    TEST_ASSERT_EQUAL_HEX16(0x0118, b.company);
    TEST_ASSERT_EQUAL_INT8(-59, b.ref_rssi);
    TEST_ASSERT_EQUAL_HEX8(0x00, b.mfg_reserved);
    const uint8_t want[20] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,
        0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b.beacon_id, 20);

    char id[41];
    gz_altbeacon_id_str(&b, id, sizeof id);
    TEST_ASSERT_EQUAL_STRING(
        "0102030405060708090a0b0c0d0e0f1011121314", id);

    char s[96];
    gz_altbeacon_summary(&b, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING(
        "AltBeacon mfr0118 0102030405060708090a0b0c0d0e0f1011121314 @-59dBm", s);
}

/* AltBeacon as the only AD structure (no leading flags) still decodes. */
static void test_altbeacon_no_flags(void)
{
    gz_altbeacon_t b;
    TEST_ASSERT_TRUE(gz_altbeacon_parse(ALT + 3, (int)sizeof ALT - 3, &b));
    TEST_ASSERT_EQUAL_HEX16(0x0118, b.company);
}

/* Right length but the beacon code is not 0xBEAC — reject (an iBeacon-style
 * Apple 0x02/0x15 payload must not be mistaken for AltBeacon). */
static void test_missing_beacon_code(void)
{
    uint8_t ad[sizeof ALT];
    memcpy(ad, ALT, sizeof ALT);
    ad[7] = 0x02; ad[8] = 0x15;                 /* code BEAC -> 0215 */
    gz_altbeacon_t b;
    TEST_ASSERT_FALSE(gz_altbeacon_parse(ad, sizeof ad, &b));
    TEST_ASSERT_FALSE(b.valid);
}

/* Only the high byte of the code matches — still reject. */
static void test_half_code(void)
{
    uint8_t ad[sizeof ALT];
    memcpy(ad, ALT, sizeof ALT);
    ad[8] = 0xAD;                               /* BE AD, not BE AC */
    gz_altbeacon_t b;
    TEST_ASSERT_FALSE(gz_altbeacon_parse(ad, sizeof ad, &b));
}

/* Every truncated prefix of a valid advert rejects without over-read. */
static void test_truncated_overrun(void)
{
    gz_altbeacon_t b;
    for (int cut = 1; cut < (int)sizeof ALT; ++cut) {
        bool ok = gz_altbeacon_parse(ALT, cut, &b);
        TEST_ASSERT_FALSE(ok);
        TEST_ASSERT_FALSE(b.valid);
    }
}

/* Manufacturer AD one data byte short of a full AltBeacon (n == 25): reject
 * rather than over-read the reserved byte. */
static void test_one_byte_short(void)
{
    uint8_t ad[] = {
        0x1A, 0xFF, 0x18, 0x01, 0xBE, 0xAC,
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,
        0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,
        0xC5                                     /* rssi present, reserved absent */
    };
    gz_altbeacon_t b;
    TEST_ASSERT_FALSE(gz_altbeacon_parse(ad, sizeof ad, &b));
}

/* Degenerate and garbage buffers never crash, over-read or false-positive. */
static void test_degenerate_and_fuzz(void)
{
    gz_altbeacon_t b;
    TEST_ASSERT_FALSE(gz_altbeacon_parse(NULL, 0, &b));
    TEST_ASSERT_FALSE(gz_altbeacon_parse(ALT, 0, &b));
    const uint8_t zeros[10] = { 0 };
    TEST_ASSERT_FALSE(gz_altbeacon_parse(zeros, sizeof zeros, &b));
    for (int len = 0; len <= 48; ++len) {
        uint8_t buf[48];
        for (int i = 0; i < len; ++i) buf[i] = (uint8_t)(0x3C ^ (i * 29 + len));
        (void)gz_altbeacon_parse(buf, len, &b);
    }
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_altbeacon_decode);
    RUN_TEST(test_altbeacon_no_flags);
    RUN_TEST(test_missing_beacon_code);
    RUN_TEST(test_half_code);
    RUN_TEST(test_truncated_overrun);
    RUN_TEST(test_one_byte_short);
    RUN_TEST(test_degenerate_and_fuzz);
    return UNITY_END();
}
