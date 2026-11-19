/* Native Unity test: C5 command-enum parity (audit F7).
 *
 * The ESP-NOW wire type numbers live in TWO hand-maintained headers that must
 * agree byte-for-byte or the S3 and the C5/TRIDENT node silently talk past each
 * other on air:
 *   - src/c5_cmd.h            POSEIDON (S3) side  — C5_TYPE_*
 *   - c5_node/main/proto.h    C5/TRIDENT side     — POSEI_TYPE_*
 *
 * c5_cmd.h already pins its payload struct *sizes* to literals that mirror
 * proto.h, and proto.h is covered by test_proto. What nothing pinned until now
 * is that the two type *enumerations* carry the same number for the same
 * message — e.g. that a future edit moving POSEI_TYPE_CMD_DEAUTH off 14 while
 * C5_TYPE_CMD_DEAUTH stays 14 breaks the build instead of the hardware.
 *
 * This suite includes BOTH real headers in one translation unit (c5_cmd.h via
 * a host stub for <Arduino.h> — see Arduino.h in this folder) and static_asserts
 * every pair. The asserts fire at compile time; the runtime cases below give
 * Unity something to run and double as human-readable documentation of the map.
 *
 * The single intentional rename is C5_TYPE_CMD_HS == POSEI_TYPE_CMD_HS_CAPTURE
 * (both 17, net-002); every other pair shares a name.
 */
#include <unity.h>
#include "../../c5_node/main/proto.h"   /* POSEI_TYPE_*  (pure: stdint/stddef) */
#include "../../src/c5_cmd.h"           /* C5_TYPE_*     (via host Arduino.h)   */

/* ---- compile-time pinning: the real guard. Drift fails the build. ---- */
#define GZ_PARITY(c5, posei) \
    static_assert((int)(c5) == (int)(posei), #c5 " must equal " #posei)

GZ_PARITY(C5_TYPE_HELLO,             POSEI_TYPE_HELLO);
GZ_PARITY(C5_TYPE_CMD_PING,          POSEI_TYPE_CMD_PING);
GZ_PARITY(C5_TYPE_CMD_SCAN_5G,       POSEI_TYPE_CMD_SCAN_5G);
GZ_PARITY(C5_TYPE_CMD_SCAN_ZB,       POSEI_TYPE_CMD_SCAN_ZB);
GZ_PARITY(C5_TYPE_CMD_SCAN_2G,       POSEI_TYPE_CMD_SCAN_2G);
GZ_PARITY(C5_TYPE_CMD_DEAUTH,        POSEI_TYPE_CMD_DEAUTH);
GZ_PARITY(C5_TYPE_CMD_STOP,          POSEI_TYPE_CMD_STOP);
GZ_PARITY(C5_TYPE_CMD_PMKID,         POSEI_TYPE_CMD_PMKID);
GZ_PARITY(C5_TYPE_CMD_HS,            POSEI_TYPE_CMD_HS_CAPTURE);  /* net-002 rename */
GZ_PARITY(C5_TYPE_CMD_CLIENTS_HUNT,  POSEI_TYPE_CMD_CLIENTS_HUNT);
GZ_PARITY(C5_TYPE_CMD_CLIENTS_AP,    POSEI_TYPE_CMD_CLIENTS_AP);
GZ_PARITY(C5_TYPE_CMD_BEACON_SPAM,   POSEI_TYPE_CMD_BEACON_SPAM);
GZ_PARITY(C5_TYPE_CMD_PROBE_SNIFF,   POSEI_TYPE_CMD_PROBE_SNIFF);
GZ_PARITY(C5_TYPE_CMD_DEAUTH_DETECT, POSEI_TYPE_CMD_DEAUTH_DETECT);
GZ_PARITY(C5_TYPE_CMD_KARMA,         POSEI_TYPE_CMD_KARMA);
GZ_PARITY(C5_TYPE_CMD_APCLONE,       POSEI_TYPE_CMD_APCLONE);
GZ_PARITY(C5_TYPE_CMD_SPECTRUM,      POSEI_TYPE_CMD_SPECTRUM);
GZ_PARITY(C5_TYPE_CMD_CIW,           POSEI_TYPE_CMD_CIW);
GZ_PARITY(C5_TYPE_RESP_PONG,         POSEI_TYPE_RESP_PONG);
GZ_PARITY(C5_TYPE_RESP_AP,           POSEI_TYPE_RESP_AP);
GZ_PARITY(C5_TYPE_RESP_ZB,           POSEI_TYPE_RESP_ZB);
GZ_PARITY(C5_TYPE_RESP_STATUS,       POSEI_TYPE_RESP_STATUS);
GZ_PARITY(C5_TYPE_RESP_PMKID,        POSEI_TYPE_RESP_PMKID);
GZ_PARITY(C5_TYPE_RESP_HS,           POSEI_TYPE_RESP_HS);
GZ_PARITY(C5_TYPE_RESP_STA,          POSEI_TYPE_RESP_STA);
GZ_PARITY(C5_TYPE_RESP_PROBE,        POSEI_TYPE_RESP_PROBE);
GZ_PARITY(C5_TYPE_RESP_DEAUTH_HIT,   POSEI_TYPE_RESP_DEAUTH_HIT);
GZ_PARITY(C5_TYPE_RESP_SPECTRUM,     POSEI_TYPE_RESP_SPECTRUM);
GZ_PARITY(C5_TYPE_RESP_WIFI_DET,     POSEI_TYPE_RESP_WIFI_DET);
GZ_PARITY(C5_TYPE_RESP_BLE_DET,      POSEI_TYPE_RESP_BLE_DET);

/* The frame magic and flag-day version must also match. */
static_assert((unsigned)C5_MAGIC   == (unsigned)POSEI_MAGIC,   "wire magic");
static_assert((int)C5_VERSION      == (int)POSEI_VERSION,      "wire version (v5 flag day)");

void setUp(void) {}
void tearDown(void) {}

/* Runtime mirrors of the compile-time asserts: a spread across commands,
 * responses, the GODZILLA publish types (60/61) and the one rename. */
static void test_commands_match(void)
{
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_CMD_PING,   C5_TYPE_CMD_PING);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_CMD_DEAUTH, C5_TYPE_CMD_DEAUTH);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_CMD_CIW,    C5_TYPE_CMD_CIW);
}

static void test_hs_rename_is_pinned(void)
{
    /* The only name that differs across the two headers. */
    TEST_ASSERT_EQUAL_INT(17, C5_TYPE_CMD_HS);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_CMD_HS_CAPTURE, C5_TYPE_CMD_HS);
}

static void test_responses_match(void)
{
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_RESP_PONG,     C5_TYPE_RESP_PONG);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_RESP_SPECTRUM, C5_TYPE_RESP_SPECTRUM);
}

static void test_godzilla_detection_types_match(void)
{
    TEST_ASSERT_EQUAL_INT(60, C5_TYPE_RESP_WIFI_DET);
    TEST_ASSERT_EQUAL_INT(61, C5_TYPE_RESP_BLE_DET);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_RESP_WIFI_DET, C5_TYPE_RESP_WIFI_DET);
    TEST_ASSERT_EQUAL_INT(POSEI_TYPE_RESP_BLE_DET,  C5_TYPE_RESP_BLE_DET);
}

static void test_magic_and_version_match(void)
{
    TEST_ASSERT_EQUAL_UINT32((uint32_t)POSEI_MAGIC, (uint32_t)C5_MAGIC);
    TEST_ASSERT_EQUAL_INT(POSEI_VERSION, C5_VERSION);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_commands_match);
    RUN_TEST(test_hs_rename_is_pinned);
    RUN_TEST(test_responses_match);
    RUN_TEST(test_godzilla_detection_types_match);
    RUN_TEST(test_magic_and_version_match);
    return UNITY_END();
}
