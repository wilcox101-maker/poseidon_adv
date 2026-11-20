/* Native Unity test for godzilla_node/main/gz_smartglass.h — the passive
 * recording-smart-glasses classifier by Bluetooth SIG company identifier
 * (detection §4 unit 5). Captured-style advert bytes drive every path:
 *   - a manufacturer advert whose SIG company ID is a dedicated smart-glasses
 *     maker (Vuzix 0x060C) -> HIGH;
 *   - a company ID of a maker that also ships non-glasses products
 *     (Meta Platforms Technologies 0x00B6, Snapchat 0x03C2) -> LOW;
 *   - an unrelated company ID (Apple 0x004C), a name-only advert, no
 *     manufacturer data, and a malformed/empty advert -> NONE.
 * Pure host TU, same style as test_godzilla_skimmer. */
#include <unity.h>
#include "../../godzilla_node/main/gz_smartglass.h"

void setUp(void) {}
void tearDown(void) {}

/* AD structure bytes. A 16-bit company ID is little-endian on the wire, so the
 * manufacturer-specific AD (type 0xFF) carries it low byte first. */
#define FLAGS            0x02, 0x01, 0x06
/* Manufacturer-specific AD: len, 0xFF, company-LE[, extra payload]. */
#define MFR_VUZIX        0x03, 0xFF, 0x0C, 0x06                 /* 0x060C */
#define MFR_META_RL      0x03, 0xFF, 0xB6, 0x00                 /* 0x00B6 */
#define MFR_SNAP         0x03, 0xFF, 0xC2, 0x03                 /* 0x03C2 */
#define MFR_APPLE        0x03, 0xFF, 0x4C, 0x00                 /* 0x004C */
#define NAME_GLASS       0x06, 0x09, 'G','l','a','s','s'

static int classify(const uint8_t *p, int n) { return gz_smartglass_classify_payload(p, n); }

static void test_dedicated_maker_company_id_is_high(void)
{
    /* Vuzix Corporation (0x060C) — a dedicated AR/smart-glasses maker. */
    const uint8_t vuzix[] = { FLAGS, MFR_VUZIX };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_HIGH, classify(vuzix, sizeof vuzix));
    /* With extra manufacturer payload after the company ID — still HIGH. */
    const uint8_t vuzix_ext[] = { FLAGS, 0x06, 0xFF, 0x0C, 0x06, 0xDE, 0xAD, 0xBE, NAME_GLASS };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_HIGH, classify(vuzix_ext, sizeof vuzix_ext));
}

static void test_shared_parent_company_id_is_low(void)
{
    /* Meta Platforms Technologies, LLC (0x00B6) — Reality Labs / Ray-Ban Meta,
     * but the ID also ships Quest headsets etc., so LOW (needs corroboration). */
    const uint8_t meta[] = { FLAGS, MFR_META_RL };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_LOW, classify(meta, sizeof meta));
    /* Snapchat Inc (0x03C2) — Snap Spectacles, but the ID also ships app
     * beacons etc., so LOW. */
    const uint8_t snap[] = { FLAGS, MFR_SNAP };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_LOW, classify(snap, sizeof snap));
}

static void test_unrelated_company_is_none(void)
{
    /* Apple (0x004C) — an unlisted company for this classifier. */
    const uint8_t apple[] = { FLAGS, MFR_APPLE };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, classify(apple, sizeof apple));
}

static void test_no_manufacturer_data_is_none(void)
{
    /* A name-only advert (no manufacturer-specific AD) is never flagged — a
     * local name alone is not a signal. */
    const uint8_t name_only[] = { FLAGS, NAME_GLASS };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, classify(name_only, sizeof name_only));
    /* Just flags, nothing else. */
    const uint8_t flags_only[] = { FLAGS };
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, classify(flags_only, sizeof flags_only));
}

static void test_malformed_and_empty_are_none(void)
{
    /* A manufacturer AD whose declared length runs past the buffer: malformed. */
    const uint8_t bad[] = { FLAGS, 0x0A, 0xFF, 0x0C, 0x06 };   /* len says 10, only 2 follow */
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, classify(bad, sizeof bad));
    /* Zero-length payload. */
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, classify((const uint8_t *)"", 0));
}

static void test_classify_struct_direct(void)
{
    /* The struct-level entry point agrees with the payload one. */
    gz_adv_t a;
    const uint8_t vuzix[] = { FLAGS, MFR_VUZIX };
    gz_adv_parse(vuzix, sizeof vuzix, &a);
    TEST_ASSERT_TRUE(a.has_mfr);
    TEST_ASSERT_EQUAL_HEX16(0x060C, a.company);
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_HIGH, gz_smartglass_classify(&a));
    /* A bare company-id lookup. */
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_HIGH, gz_smartglass_cid_conf(0x060C));
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_LOW,  gz_smartglass_cid_conf(0x00B6));
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_LOW,  gz_smartglass_cid_conf(0x03C2));
    TEST_ASSERT_EQUAL_INT(GZ_SMARTGLASS_NONE, gz_smartglass_cid_conf(0x004C));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dedicated_maker_company_id_is_high);
    RUN_TEST(test_shared_parent_company_id_is_low);
    RUN_TEST(test_unrelated_company_is_none);
    RUN_TEST(test_no_manufacturer_data_is_none);
    RUN_TEST(test_malformed_and_empty_are_none);
    RUN_TEST(test_classify_struct_direct);
    return UNITY_END();
}
