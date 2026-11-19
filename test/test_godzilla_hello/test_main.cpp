/* Native Unity tests for the GODZILLA HELLO builder,
 * godzilla_node/main/gz_hello.h. The builder is pure (proto.h + string.h), so
 * it runs on the host. These pin the node's advertised identity so a refactor
 * can't silently make GODZILLA announce itself as a C5 or claim radios it
 * doesn't have:
 *   - name is exactly "GODZILLA" and NUL-terminated within posei_hello_t.name,
 *   - role is GZ_ROLE and never 1 (the C5's role),
 *   - has_5g and has_ieee802154 are 0 (S3, 2.4 GHz only, no 802.15.4),
 *   - the caller's heap reading is carried through verbatim. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_hello.h"

void setUp(void) {}
void tearDown(void) {}

static void test_hello_identity(void)
{
    posei_hello_t h;
    gz_build_hello(&h, 123);
    TEST_ASSERT_EQUAL_STRING("GODZILLA", h.name);
    TEST_ASSERT_EQUAL_UINT8(GZ_ROLE, h.role);
    TEST_ASSERT_NOT_EQUAL(1, h.role);              /* never the C5's role */
    TEST_ASSERT_EQUAL_UINT8(0, h.has_5g);
    TEST_ASSERT_EQUAL_UINT8(0, h.has_ieee802154);
    TEST_ASSERT_EQUAL_UINT32(123, h.heap_kb);
}

/* Even from dirty memory the name must come back a valid, bounded C string. */
static void test_hello_name_nul_terminated(void)
{
    posei_hello_t h;
    memset(&h, 0xAA, sizeof(h));
    gz_build_hello(&h, 0);
    TEST_ASSERT_TRUE(strlen(h.name) < sizeof(h.name));
    TEST_ASSERT_EQUAL_CHAR('\0', h.name[sizeof(h.name) - 1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hello_identity);
    RUN_TEST(test_hello_name_nul_terminated);
    return UNITY_END();
}
