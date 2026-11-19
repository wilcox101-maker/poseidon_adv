/* Native Unity test for the BLE Lab's protective tracker surfacing
 * (detection-breadth §1 item 3): gz_ble_tracker.h — classify a raw advert
 * as a known PUBLIC tracker signature (Find My / Tile / SmartTag) decode-only,
 * with family label and malformed-input safety (truncated / oversized /
 * garbage → NONE, no over-read). Adverts are hand-built in the public shapes;
 * nothing here resolves identity or ownership. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_ble_tracker.h"

void setUp(void) {}
void tearDown(void) {}

/* Apple Find My / AirTag (separated): flags, then manufacturer data with an
 * Apple TLV type 0x12 ("offline finding") length 0x19. */
static const uint8_t FINDMY[] = {
    0x02, 0x01, 0x06,
    0x1E, 0xFF, 0x4C, 0x00,          /* mfr, Apple */
    0x12, 0x19,                      /* TLV type 0x12 (Find My), len 0x19 = 25 */
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,
    0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18  /* 25-byte body */
};

static void test_findmy(void)
{
    gz_tracker_t t = gz_tracker_classify(FINDMY, sizeof FINDMY);
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_FINDMY, t);
    TEST_ASSERT_TRUE(gz_tracker_is_tracker(t));
    TEST_ASSERT_EQUAL_STRING("Find My", gz_tracker_family(t));
    TEST_ASSERT_EQUAL_STRING("possible tracker (Find My)", gz_tracker_label(t));
}

/* An Apple advert that is NOT Find My (nearby type 0x10 only) is not flagged. */
static void test_apple_non_findmy(void)
{
    const uint8_t ad[] = {
        0x06, 0xFF, 0x4C, 0x00, 0x10, 0x02, 0xAB, 0xCD   /* TLV type 0x10 */
    };
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, gz_tracker_classify(ad, sizeof ad));
}

/* Tile via the 0xFEED service UUID in the complete-UUID list. */
static void test_tile_uuid_list(void)
{
    const uint8_t ad[] = {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0xED, 0xFE          /* complete 16-bit UUID 0xFEED */
    };
    gz_tracker_t t = gz_tracker_classify(ad, sizeof ad);
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_TILE, t);
    TEST_ASSERT_EQUAL_STRING("possible tracker (Tile)", gz_tracker_label(t));
}

/* Tile via 0xFEED service data. */
static void test_tile_service_data(void)
{
    const uint8_t ad[] = {
        0x05, 0x16, 0xED, 0xFE, 0x01, 0x02
    };
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_TILE, gz_tracker_classify(ad, sizeof ad));
}

/* Samsung Galaxy SmartTag via the 0xFD5A service UUID. */
static void test_smarttag(void)
{
    const uint8_t ad[] = {
        0x03, 0x03, 0x5A, 0xFD
    };
    gz_tracker_t t = gz_tracker_classify(ad, sizeof ad);
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_SMARTTAG, t);
    TEST_ASSERT_EQUAL_STRING("SmartTag", gz_tracker_family(t));
    TEST_ASSERT_EQUAL_STRING("possible tracker (SmartTag)", gz_tracker_label(t));
}

/* An ordinary advert (a heart-rate sensor) is not a tracker. */
static void test_not_a_tracker(void)
{
    const uint8_t ad[] = {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x0D, 0x18,          /* Heart Rate 0x180D */
        0x05, 0x09, 'H','R','M','1'
    };
    gz_tracker_t t = gz_tracker_classify(ad, sizeof ad);
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, t);
    TEST_ASSERT_FALSE(gz_tracker_is_tracker(t));
    TEST_ASSERT_NULL(gz_tracker_family(t));
    TEST_ASSERT_EQUAL_STRING("", gz_tracker_label(t));
}

/* Every truncated prefix of the Find My advert yields NONE without over-read. */
static void test_truncated_overrun(void)
{
    for (int cut = 1; cut < (int)sizeof FINDMY; ++cut)
        TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE,
                              gz_tracker_classify(FINDMY, cut));
}

/* An Apple mfr AD whose TLV length overruns the structure must not be read
 * past the end, and must not false-positive. */
static void test_apple_tlv_overrun(void)
{
    const uint8_t ad[] = {
        0x06, 0xFF, 0x4C, 0x00, 0x12, 0x7F, 0xAA, 0xBB  /* TLV len 0x7F >> data */
    };
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, gz_tracker_classify(ad, sizeof ad));
}

/* Degenerate and garbage buffers never crash, over-read or false-positive. */
static void test_degenerate_and_fuzz(void)
{
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, gz_tracker_classify(NULL, 0));
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, gz_tracker_classify(FINDMY, 0));
    const uint8_t zeros[12] = { 0 };
    TEST_ASSERT_EQUAL_INT(GZ_TRACKER_NONE, gz_tracker_classify(zeros, sizeof zeros));
    for (int len = 0; len <= 48; ++len) {
        uint8_t buf[48];
        for (int i = 0; i < len; ++i) buf[i] = (uint8_t)(0x6D ^ (i * 23 + len));
        (void)gz_tracker_classify(buf, len);
    }
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_findmy);
    RUN_TEST(test_apple_non_findmy);
    RUN_TEST(test_tile_uuid_list);
    RUN_TEST(test_tile_service_data);
    RUN_TEST(test_smarttag);
    RUN_TEST(test_not_a_tracker);
    RUN_TEST(test_truncated_overrun);
    RUN_TEST(test_apple_tlv_overrun);
    RUN_TEST(test_degenerate_and_fuzz);
    return UNITY_END();
}
