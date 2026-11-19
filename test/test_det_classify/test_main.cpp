/* Native Unity tests for src/det_classify.h — the shared BLE advertisement →
 * detection-family classifier (GODZILLA reuse step 1).
 *
 * Inputs are raw advertising payloads (AD structures: len, type, data...),
 * the form NimBLE exposes via getPayload() on both the Cardputer and the
 * GODZILLA node. Expected families are pinned against c5_node/main/proto.h. */
#include <unity.h>
#include <string.h>
#include "../../c5_node/main/proto.h"
#include "../../src/det_classify.h"

void setUp(void) {}
void tearDown(void) {}

#define CLS(arr) det_classify_ble((arr), sizeof(arr))

static void test_families_match_wire(void)
{
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_BLE_GENERIC, DET_FAM_BLE_GENERIC);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_FINDMY,      DET_FAM_FINDMY);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_IBEACON,     DET_FAM_IBEACON);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_REMOTEID,    DET_FAM_REMOTEID);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_RAVEN,       DET_FAM_RAVEN);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_TILE,        DET_FAM_TILE);
    TEST_ASSERT_EQUAL_UINT(POSEI_DET_FAM_SMARTTAG,    DET_FAM_SMARTTAG);
}

static void test_empty_and_null(void)
{
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, det_classify_ble(nullptr, 0));
    const uint8_t flags_only[] = { 0x02, 0x01, 0x06 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(flags_only));
}

/* Apple Find My (offline finding, separated): 4C 00 12 19 + status + key... */
static void test_findmy(void)
{
    uint8_t adv[31] = { 0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, 0x10 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_FINDMY, CLS(adv));
    /* Short "nearby owner" form is still Find My. */
    const uint8_t near[] = { 0x07, 0xFF, 0x4C, 0x00, 0x12, 0x02, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_FINDMY, CLS(near));
}

/* iBeacon: 4C 00 02 15 + 16B UUID + major + minor + tx. */
static void test_ibeacon(void)
{
    uint8_t adv[30] = { 0x02, 0x01, 0x06, 0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_IBEACON, CLS(adv));
    /* Apple subtype 0x02 without the 0x15 length is not an iBeacon. */
    const uint8_t not_ib[] = { 0x05, 0xFF, 0x4C, 0x00, 0x02, 0x03 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(not_ib));
}

/* Other Apple Continuity (AirPods proximity pairing) is generic. */
static void test_apple_other_generic(void)
{
    const uint8_t adv[] = { 0x08, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x01, 0x0E, 0x20 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(adv));
}

/* ASTM F3411 Remote ID: service data, UUID 0xFFFA. */
static void test_remoteid(void)
{
    uint8_t adv[31] = { 0x02, 0x01, 0x06, 0x1B, 0x16, 0xFA, 0xFF, 0x0D, 0x00, 0x02 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_REMOTEID, CLS(adv));
}

/* Raven: XUNTONG company ID 0x09C8 in manufacturer data. */
static void test_raven_company(void)
{
    const uint8_t adv[] = { 0x05, 0xFF, 0xC8, 0x09, 0x01, 0x02 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_RAVEN, CLS(adv));
}

/* Raven: custom 16-bit service UUID in the 0x3100-0x3500 set. */
static void test_raven_custom_uuid(void)
{
    const uint8_t adv[] = { 0x05, 0x03, 0x0A, 0x18, 0x00, 0x32 };   /* 180A + 3200 */
    TEST_ASSERT_EQUAL_UINT(DET_FAM_RAVEN, CLS(adv));
}

/* Standard SIG services alone must NOT read as Raven (the ble_scan false
 * positive: Device Information 0x180A is near-universal). */
static void test_sig_services_not_raven(void)
{
    const uint8_t dis[]  = { 0x03, 0x03, 0x0A, 0x18 };
    const uint8_t hts[]  = { 0x03, 0x02, 0x09, 0x18 };
    const uint8_t lns[]  = { 0x03, 0x03, 0x19, 0x18 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(dis));
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(hts));
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(lns));
}

/* A vendor 128-bit UUID whose bytes 12-13 happen to be 0x3100 is not a
 * 16-bit Raven UUID: only SIG-base 128-bit UUIDs reduce to 16 bits. */
static void test_vendor_uuid128_not_raven(void)
{
    /* Little-endian 128-bit: bytes[12..13] = 00 31, base is NOT SIG. */
    const uint8_t adv[] = { 0x11, 0x07,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0x00, 0x31, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(adv));
}

static void test_sig_uuid128_reduces(void)
{
    /* SIG base 0000xxxx-0000-1000-8000-00805F9B34FB, little-endian, xxxx = 3300. */
    const uint8_t adv[] = { 0x11, 0x07,
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_RAVEN, CLS(adv));
}

/* Tile: service UUID 0xFEED or 0xFD84 (UUID list or service data). */
static void test_tile(void)
{
    const uint8_t feed[] = { 0x03, 0x03, 0xED, 0xFE };
    const uint8_t fd84[] = { 0x05, 0x16, 0x84, 0xFD, 0x01, 0x02 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_TILE, CLS(feed));
    TEST_ASSERT_EQUAL_UINT(DET_FAM_TILE, CLS(fd84));
}

/* SmartTag: service data 0xFD5A, or Samsung CID 0x0075 + 42 09 (ble_db rule).
 * A plain Samsung CID (phones, buds) is not a SmartTag. */
static void test_smarttag(void)
{
    const uint8_t fd5a[]   = { 0x05, 0x16, 0x5A, 0xFD, 0x10, 0x20 };
    const uint8_t cid_tag[] = { 0x06, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x01 };
    const uint8_t phone[]  = { 0x06, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_SMARTTAG, CLS(fd5a));
    TEST_ASSERT_EQUAL_UINT(DET_FAM_SMARTTAG, CLS(cid_tag));
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(phone));
}

/* Priority: Remote ID beats everything; Raven beats tracker families. */
static void test_priority(void)
{
    const uint8_t rid_and_apple[] = { 0x05, 0x16, 0xFA, 0xFF, 0x0D, 0x00,
                                      0x05, 0xFF, 0x4C, 0x00, 0x12, 0x02 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_REMOTEID, CLS(rid_and_apple));
    const uint8_t raven_and_tile[] = { 0x03, 0x03, 0xED, 0xFE,
                                       0x04, 0xFF, 0xC8, 0x09, 0x00 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_RAVEN, CLS(raven_and_tile));
}

/* Malformed AD: length running past the buffer stops parsing safely; data
 * before it still counts, data after it is ignored. */
static void test_malformed_ad(void)
{
    const uint8_t overrun[] = { 0x03, 0x03, 0xED, 0xFE, 0x1F, 0xFF, 0xC8, 0x09 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_TILE, CLS(overrun));
    const uint8_t zero_len[] = { 0x00, 0x03, 0xED, 0xFE };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(zero_len));
    const uint8_t trunc_mfg[] = { 0x02, 0xFF, 0x4C };      /* CID cut short */
    TEST_ASSERT_EQUAL_UINT(DET_FAM_BLE_GENERIC, CLS(trunc_mfg));
    const uint8_t odd_uuid_list[] = { 0x04, 0x03, 0xED, 0xFE, 0x00 };
    TEST_ASSERT_EQUAL_UINT(DET_FAM_TILE, CLS(odd_uuid_list));
}

/* UUID strings as NimBLE versions print them. Only the Bluetooth base
 * reduces to 16 bits; a vendor 128-bit UUID never does. */
static void test_uuid_str_to16(void)
{
    uint16_t u = 0;
    TEST_ASSERT_TRUE(det_uuid_str_to16("180a", &u));   TEST_ASSERT_EQUAL_HEX16(0x180A, u);
    TEST_ASSERT_TRUE(det_uuid_str_to16("0x3200", &u)); TEST_ASSERT_EQUAL_HEX16(0x3200, u);
    TEST_ASSERT_TRUE(det_uuid_str_to16("0000FEED-0000-1000-8000-00805F9B34FB", &u));
    TEST_ASSERT_EQUAL_HEX16(0xFEED, u);
    TEST_ASSERT_TRUE(det_uuid_str_to16("00003100-0000-1000-8000-00805f9b34fb", &u));
    TEST_ASSERT_EQUAL_HEX16(0x3100, u);
    /* Vendor UUID whose chars 4..7 read 3100: must not reduce. */
    TEST_ASSERT_FALSE(det_uuid_str_to16("12343100-abcd-ef01-2345-6789abcdef01", &u));
    TEST_ASSERT_FALSE(det_uuid_str_to16("00003100-0000-1000-8000-00805f9b34fc", &u));
    TEST_ASSERT_FALSE(det_uuid_str_to16("", &u));
    TEST_ASSERT_FALSE(det_uuid_str_to16("18g0", &u));
    TEST_ASSERT_FALSE(det_uuid_str_to16(nullptr, &u));
}

/* The ble_scan Raven check: custom Raven UUIDs yes, SIG services no. */
static void test_raven_custom_uuid_helper(void)
{
    TEST_ASSERT_TRUE(det_raven_custom_uuid(0x3100));
    TEST_ASSERT_TRUE(det_raven_custom_uuid(0x3500));
    TEST_ASSERT_FALSE(det_raven_custom_uuid(0x180A));
    TEST_ASSERT_FALSE(det_raven_custom_uuid(0x1809));
    TEST_ASSERT_FALSE(det_raven_custom_uuid(0x1819));
    TEST_ASSERT_FALSE(det_raven_custom_uuid(0xFEED));
}

/* Tracker screens: label only for tracker families (<= 11 chars, fits the
 * 12-byte buffers in ble_extras/ble_finder). A Samsung phone is not one. */
static void test_tracker_label(void)
{
    TEST_ASSERT_EQUAL_STRING("Find My",  det_tracker_label(DET_FAM_FINDMY));
    TEST_ASSERT_EQUAL_STRING("AirTag",   det_tracker_label(DET_FAM_AIRTAG));
    TEST_ASSERT_EQUAL_STRING("SmartTag", det_tracker_label(DET_FAM_SMARTTAG));
    TEST_ASSERT_EQUAL_STRING("Tile",     det_tracker_label(DET_FAM_TILE));
    TEST_ASSERT_NULL(det_tracker_label(DET_FAM_BLE_GENERIC));
    TEST_ASSERT_NULL(det_tracker_label(DET_FAM_RAVEN));
    const uint8_t phone[] = { 0x06, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02 };
    TEST_ASSERT_NULL(det_tracker_label(CLS(phone)));
}

static void test_family_names(void)
{
    TEST_ASSERT_EQUAL_STRING("Find My",  det_family_name(DET_FAM_FINDMY));
    TEST_ASSERT_EQUAL_STRING("Raven",    det_family_name(DET_FAM_RAVEN));
    TEST_ASSERT_EQUAL_STRING("Unknown",  det_family_name(200));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_families_match_wire);
    RUN_TEST(test_empty_and_null);
    RUN_TEST(test_findmy);
    RUN_TEST(test_ibeacon);
    RUN_TEST(test_apple_other_generic);
    RUN_TEST(test_remoteid);
    RUN_TEST(test_raven_company);
    RUN_TEST(test_raven_custom_uuid);
    RUN_TEST(test_sig_services_not_raven);
    RUN_TEST(test_vendor_uuid128_not_raven);
    RUN_TEST(test_sig_uuid128_reduces);
    RUN_TEST(test_tile);
    RUN_TEST(test_smarttag);
    RUN_TEST(test_priority);
    RUN_TEST(test_malformed_ad);
    RUN_TEST(test_uuid_str_to16);
    RUN_TEST(test_raven_custom_uuid_helper);
    RUN_TEST(test_tracker_label);
    RUN_TEST(test_family_names);
    return UNITY_END();
}
