/* Native Unity test for godzilla_node/main/gz_fmt.h — count pluralisation for
 * the UI labels ("1 hit" / "N hits", "1 sample", "saved 1 row"). */
#include <unity.h>
#include <stdio.h>
#include "../../godzilla_node/main/gz_fmt.h"

void setUp(void) {}
void tearDown(void) {}

static void test_plural(void)
{
    TEST_ASSERT_EQUAL_STRING("hits", gz_plural(0, "hit", "hits"));
    TEST_ASSERT_EQUAL_STRING("hit",  gz_plural(1, "hit", "hits"));
    TEST_ASSERT_EQUAL_STRING("hits", gz_plural(2, "hit", "hits"));
    TEST_ASSERT_EQUAL_STRING("hits", gz_plural(65535, "hit", "hits"));
    TEST_ASSERT_EQUAL_STRING("rows", gz_plural(4294967295ul, "row", "rows"));
}

/* The exact strings the three labels now produce. */
static void test_labels(void)
{
    char b[48];
    snprintf(b, sizeof b, "%u %s", 1u, gz_plural(1, "hit", "hits"));          TEST_ASSERT_EQUAL_STRING("1 hit", b);
    snprintf(b, sizeof b, "%u %s", 40u, gz_plural(40, "hit", "hits"));        TEST_ASSERT_EQUAL_STRING("40 hits", b);
    snprintf(b, sizeof b, "%u %s", 1u, gz_plural(1, "sample", "samples"));    TEST_ASSERT_EQUAL_STRING("1 sample", b);
    snprintf(b, sizeof b, "saved %lu %s", 1ul, gz_plural(1, "row", "rows"));  TEST_ASSERT_EQUAL_STRING("saved 1 row", b);
    snprintf(b, sizeof b, "saved %lu %s", 12ul, gz_plural(12, "row", "rows")); TEST_ASSERT_EQUAL_STRING("saved 12 rows", b);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_plural);
    RUN_TEST(test_labels);
    return UNITY_END();
}
