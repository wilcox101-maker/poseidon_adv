/* Native Unity tests for src/odid_decode.h — ASTM F3411 / Open Drone ID
 * decoding for BLE (GODZILLA reuse step 2).
 *
 * Vectors are built byte-by-byte from the reference layouts in
 * opendroneid-core-c (libopendroneid/opendroneid.h, a92bd5f) and the BLE
 * framing used by opendroneid/receiver-android (service data 0xFFFA, then
 * app code 0x0D, a message counter, then the 25-byte message or a pack). */
#include <unity.h>
#include <string.h>
#include <math.h>
#include "../../src/odid_decode.h"

void setUp(void) {}
void tearDown(void) {}

#define ASSERT_DBL(eps, exp, act) TEST_ASSERT_TRUE_MESSAGE(fabs((double)(act) - (double)(exp)) <= (eps), #act)

static void put_le32(uint8_t *p, int32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

static void basic_id(uint8_t m[25], const char *id, uint8_t id_type, uint8_t ua_type)
{
    memset(m, 0, 25);
    m[0] = (ODID_MSG_BASIC_ID << 4) | 0x2;
    m[1] = (uint8_t)((id_type << 4) | ua_type);
    memcpy(m + 2, id, strlen(id));
}

/* lat/lon in 1e-7 deg; alt_geo encoded (m+1000)*2; ew/mult flags in byte 1. */
static void location(uint8_t m[25], int32_t lat, int32_t lon, uint8_t dir, bool ew,
                     uint8_t spd, bool mult, int8_t vspd, uint16_t alt_geo_enc)
{
    memset(m, 0, 25);
    m[0] = (ODID_MSG_LOCATION << 4) | 0x2;
    m[1] = (uint8_t)((2 << 4) | (ew ? 0x02 : 0) | (mult ? 0x01 : 0));   /* status 2 = airborne */
    m[2] = dir;
    m[3] = spd;
    m[4] = (uint8_t)vspd;
    put_le32(m + 5, lat);
    put_le32(m + 9, lon);
    put_le16(m + 13, 0);
    put_le16(m + 15, alt_geo_enc);
}

static void system_msg(uint8_t m[25], int32_t op_lat, int32_t op_lon)
{
    memset(m, 0, 25);
    m[0] = (ODID_MSG_SYSTEM << 4) | 0x2;
    put_le32(m + 2, op_lat);
    put_le32(m + 6, op_lon);
}

static void test_basic_id(void)
{
    uint8_t m[25]; odid_info_t d = {};
    basic_id(m, "1581F5FKD229C00PZ1A8", 1, 2);
    TEST_ASSERT_EQUAL_INT(ODID_MSG_BASIC_ID, odid_decode_message(m, 25, &d));
    TEST_ASSERT_TRUE(d.have_id);
    TEST_ASSERT_EQUAL_STRING("1581F5FKD229C00PZ1A8", d.uas_id);
    TEST_ASSERT_EQUAL_UINT8(1, d.id_type);
    TEST_ASSERT_EQUAL_UINT8(2, d.ua_type);
}

static void test_basic_id_trims_padding(void)
{
    uint8_t m[25]; odid_info_t d = {};
    basic_id(m, "ABC  ", 1, 2);
    odid_decode_message(m, 25, &d);
    TEST_ASSERT_EQUAL_STRING("ABC", d.uas_id);
}

static void test_location(void)
{
    uint8_t m[25]; odid_info_t d = {};
    /* 42.3314, -83.0458; dir 90 (west flag off); 40*0.25 = 10 m/s; vspd 4*0.5 = 2;
     * alt 250 m -> (250+1000)*2 = 2500. */
    location(m, 423314000, -830458000, 90, false, 40, false, 4, 2500);
    TEST_ASSERT_EQUAL_INT(ODID_MSG_LOCATION, odid_decode_message(m, 25, &d));
    TEST_ASSERT_TRUE(d.have_loc);
    TEST_ASSERT_EQUAL_UINT8(2, d.status);
    ASSERT_DBL(1e-7, 42.3314, d.lat_deg);
    ASSERT_DBL(1e-7, -83.0458, d.lon_deg);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, d.track_deg);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, d.speed_ms);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f, d.vspeed_ms);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 250.0f, d.alt_geo_m);
}

/* EW flag adds 180 deg; speed multiplier: v*0.75 + 255*0.25. */
static void test_location_flags(void)
{
    uint8_t m[25]; odid_info_t d = {};
    location(m, 1, 1, 45, true, 10, true, -6, 2000);
    odid_decode_message(m, 25, &d);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 225.0f, d.track_deg);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10 * 0.75f + 255 * 0.25f, d.speed_ms);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -3.0f, d.vspeed_ms);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, d.alt_geo_m);
}

static void test_system_operator(void)
{
    uint8_t m[25]; odid_info_t d = {};
    system_msg(m, 423300000, -830400000);
    TEST_ASSERT_EQUAL_INT(ODID_MSG_SYSTEM, odid_decode_message(m, 25, &d));
    TEST_ASSERT_TRUE(d.have_op);
    ASSERT_DBL(1e-7, 42.33, d.op_lat_deg);
    ASSERT_DBL(1e-7, -83.04, d.op_lon_deg);
}

static void test_message_rejects(void)
{
    uint8_t m[25]; odid_info_t d = {};
    basic_id(m, "X", 1, 2);
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_message(m, 24, &d));        /* short */
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_message(nullptr, 25, &d));
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_message(m, 25, nullptr));
    TEST_ASSERT_FALSE(d.have_id);
    /* Self-ID (type 3) is recognised but carries nothing we store. */
    m[0] = (ODID_MSG_SELF_ID << 4) | 0x2;
    TEST_ASSERT_EQUAL_INT(ODID_MSG_SELF_ID, odid_decode_message(m, 25, &d));
}

/* BLE service data body (after UUID): 0x0D, counter, 25-byte message. */
static void test_ble_single(void)
{
    uint8_t sd[27]; odid_info_t d = {};
    sd[0] = ODID_BLE_APP_CODE; sd[1] = 7;
    basic_id(sd + 2, "SN123", 1, 2);
    TEST_ASSERT_EQUAL_INT(1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    TEST_ASSERT_EQUAL_STRING("SN123", d.uas_id);
}

/* Regression: the old decoder read straight after the UUID, so app code
 * 0x0D was taken as the message header. A body without 0x0D is rejected. */
static void test_ble_requires_app_code(void)
{
    uint8_t sd[27]; odid_info_t d = {};
    sd[0] = 0x0C; sd[1] = 0;
    basic_id(sd + 2, "SN123", 1, 2);
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    TEST_ASSERT_FALSE(d.have_id);
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, 26, &d));      /* too short */
}

/* Message pack: [0xF2][size=25][count][count x 25B]. */
static void test_ble_pack(void)
{
    uint8_t sd[2 + 3 + 3 * 25]; odid_info_t d = {};
    sd[0] = ODID_BLE_APP_CODE; sd[1] = 1;
    uint8_t *p = sd + 2;
    p[0] = (ODID_MSG_PACK << 4) | 0x2; p[1] = 25; p[2] = 3;
    basic_id(p + 3, "PACKID", 1, 2);
    location(p + 28, 100000000, 200000000, 10, false, 8, false, 0, 2200);
    system_msg(p + 53, 100000100, 200000100);
    TEST_ASSERT_EQUAL_INT(3, odid_decode_ble_svc(sd, sizeof(sd), &d));
    TEST_ASSERT_EQUAL_STRING("PACKID", d.uas_id);
    TEST_ASSERT_TRUE(d.have_loc);
    ASSERT_DBL(1e-7, 10.0, d.lat_deg);
    TEST_ASSERT_TRUE(d.have_op);
}

static void test_pack_rejects(void)
{
    uint8_t sd[2 + 3 + 2 * 25]; odid_info_t d = {};
    sd[0] = ODID_BLE_APP_CODE; sd[1] = 1;
    uint8_t *p = sd + 2;
    p[0] = (ODID_MSG_PACK << 4) | 0x2; p[1] = 25; p[2] = 3;        /* claims 3, holds 2 */
    basic_id(p + 3, "A", 1, 2); basic_id(p + 28, "B", 1, 2);
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    p[2] = 2; p[1] = 24;                                             /* wrong message size */
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    p[1] = 25; p[2] = 0;                                             /* empty pack */
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    p[2] = 10;                                                       /* > 9 */
    TEST_ASSERT_EQUAL_INT(-1, odid_decode_ble_svc(sd, sizeof(sd), &d));
    TEST_ASSERT_FALSE(d.have_id);
}

/* A pack nested inside a pack is not decoded (no recursion). */
static void test_pack_no_nesting(void)
{
    uint8_t sd[2 + 3 + 25]; odid_info_t d = {};
    sd[0] = ODID_BLE_APP_CODE; sd[1] = 1;
    uint8_t *p = sd + 2;
    p[0] = (ODID_MSG_PACK << 4) | 0x2; p[1] = 25; p[2] = 1;
    memset(p + 3, 0, 25); p[3] = (ODID_MSG_PACK << 4) | 0x2;
    TEST_ASSERT_EQUAL_INT(0, odid_decode_ble_svc(sd, sizeof(sd), &d));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_basic_id);
    RUN_TEST(test_basic_id_trims_padding);
    RUN_TEST(test_location);
    RUN_TEST(test_location_flags);
    RUN_TEST(test_system_operator);
    RUN_TEST(test_message_rejects);
    RUN_TEST(test_ble_single);
    RUN_TEST(test_ble_requires_app_code);
    RUN_TEST(test_ble_pack);
    RUN_TEST(test_pack_rejects);
    RUN_TEST(test_pack_no_nesting);
    return UNITY_END();
}
