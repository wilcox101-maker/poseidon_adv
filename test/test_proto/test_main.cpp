/* Native Unity tests for the POSEIDON ESP-NOW wire protocol (v5) header,
 * c5_node/main/proto.h. proto.h is pure (stdint/stddef only — no esp_now.h),
 * so it compiles and runs on the native host. These pin the v5 flag-day
 * invariants so a future edit that silently breaks the wire format fails CI
 * instead of the hardware:
 *   - the 10-byte header and its field order (payload_len ahead of payload),
 *   - the variable-length wire-size math (posei_wire_len),
 *   - the version constant, and
 *   - that every payload struct still fits one ESP-NOW v2 frame.
 *
 * The compile-time _Static_asserts inside proto.h also fire here; this file
 * adds the run-time checks a static_assert can't express. proto_validate()
 * lives in proto.c (which pulls in esp_now.h) and so is covered on hardware,
 * not natively. */
#include <unity.h>
#include <string.h>
#include "../../c5_node/main/proto.h"

void setUp(void) {}
void tearDown(void) {}

/* Header is exactly 10 bytes and payload_len sits in it, ahead of payload. */
static void test_header_layout(void)
{
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)POSEI_HEADER_SZ);
    TEST_ASSERT_EQUAL_UINT(0, (unsigned)offsetof(posei_msg_t, magic));
    TEST_ASSERT_EQUAL_UINT(4, (unsigned)offsetof(posei_msg_t, version));
    TEST_ASSERT_EQUAL_UINT(5, (unsigned)offsetof(posei_msg_t, type));
    TEST_ASSERT_EQUAL_UINT(6, (unsigned)offsetof(posei_msg_t, seq));
    TEST_ASSERT_EQUAL_UINT(8, (unsigned)offsetof(posei_msg_t, payload_len));
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)offsetof(posei_msg_t, payload));
}

static void test_version_is_5(void)
{
    TEST_ASSERT_EQUAL_UINT(5, POSEI_VERSION);
}

/* Variable-length framing: a message puts only header + used payload on air. */
static void test_wire_len(void)
{
    posei_msg_t m;
    memset(&m, 0, sizeof(m));
    m.payload_len = 0;
    TEST_ASSERT_EQUAL_UINT(10, (unsigned)posei_wire_len(&m));   /* bare control frame */
    m.payload_len = 174;                                       /* a full handshake */
    TEST_ASSERT_EQUAL_UINT(184, (unsigned)posei_wire_len(&m));
    m.payload_len = POSEI_PAYLOAD_MAX;
    TEST_ASSERT_EQUAL_UINT(10 + POSEI_PAYLOAD_MAX, (unsigned)posei_wire_len(&m));
    /* clamps a bogus over-max length instead of overrunning */
    m.payload_len = POSEI_PAYLOAD_MAX + 50;
    TEST_ASSERT_EQUAL_UINT(10 + POSEI_PAYLOAD_MAX, (unsigned)posei_wire_len(&m));
}

/* Whole frame fits one ESP-NOW v2 frame, and the raised ceiling actually buys
 * the headroom the flag day was for (many AP records per frame, full EAPOL). */
static void test_fits_one_v2_frame(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT(1490, (unsigned)sizeof(posei_msg_t));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1024, (unsigned)POSEI_PAYLOAD_MAX);
    TEST_ASSERT_LESS_OR_EQUAL_UINT(POSEI_PAYLOAD_MAX, (unsigned)sizeof(posei_hs_t));
    /* >5 AP records per RESP_AP batch (was 5 at the old 230-byte ceiling). */
    TEST_ASSERT_GREATER_THAN_UINT(5, (unsigned)(POSEI_PAYLOAD_MAX / sizeof(posei_ap_t)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_header_layout);
    RUN_TEST(test_version_is_5);
    RUN_TEST(test_wire_len);
    RUN_TEST(test_fits_one_v2_frame);
    return UNITY_END();
}
