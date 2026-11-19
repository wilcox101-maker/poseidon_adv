/* Native Unity test for the BLE Lab's pure halves (#98 §2.4):
 *   gz_ble_adv.h — passive advertisement decoder (AD structures, names,
 *                  HID detection, summary line, malformed input);
 *   gz_ble_lab.h — the recent-advert table (update, evict, truncate, sort).
 * Adverts are hand-built in the shapes real devices send. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_ble_adv.h"
#include "../../godzilla_node/main/gz_ble_lab.h"

void setUp(void) {}
void tearDown(void) {}

/* A BLE keyboard: flags, appearance keyboard, HID service, complete name. */
static const uint8_t KBD[] = { 0x02, 0x01, 0x06,
                               0x03, 0x19, 0xC1, 0x03,
                               0x03, 0x03, 0x12, 0x18,
                               0x0A, 0x09, 'B','T',' ','K','e','y','b','d','!' };

static void test_keyboard_advert(void)
{
    gz_adv_t a; gz_adv_parse(KBD, sizeof KBD, &a);
    TEST_ASSERT_FALSE(a.malformed);
    TEST_ASSERT_TRUE(a.has_flags);  TEST_ASSERT_EQUAL_HEX8(0x06, a.flags);
    TEST_ASSERT_TRUE(a.has_appearance); TEST_ASSERT_EQUAL_HEX16(0x03C1, a.appearance);
    TEST_ASSERT_EQUAL_STRING("keyboard", gz_adv_appearance_name(a.appearance));
    TEST_ASSERT_EQUAL_UINT8(1, a.n_uuid16); TEST_ASSERT_EQUAL_HEX16(0x1812, a.uuid16[0]);
    TEST_ASSERT_TRUE(a.has_name); TEST_ASSERT_TRUE(a.name_complete);
    TEST_ASSERT_EQUAL_STRING("BT Keybd!", a.name);
    TEST_ASSERT_TRUE(gz_adv_is_hid(&a));
    char s[64]; gz_adv_summary(&a, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("BT Keybd! | keyboard | HID", s);
}

static void test_manufacturer_and_tx(void)
{
    /* Apple manufacturer data (company 0x004C), TX power -12 dBm. */
    const uint8_t ad[] = { 0x02, 0x0A, 0xF4,
                           0x07, 0xFF, 0x4C, 0x00, 0x02, 0x15, 0xAA, 0xBB };
    gz_adv_t a; gz_adv_parse(ad, sizeof ad, &a);
    TEST_ASSERT_TRUE(a.has_tx); TEST_ASSERT_EQUAL_INT8(-12, a.tx_dbm);
    TEST_ASSERT_TRUE(a.has_mfr); TEST_ASSERT_EQUAL_HEX16(0x004C, a.company);
    TEST_ASSERT_EQUAL_UINT8(4, a.mfr_len);
    TEST_ASSERT_EQUAL_STRING("Apple", gz_adv_company_name(a.company));
    TEST_ASSERT_FALSE(gz_adv_is_hid(&a));
    char s[64]; gz_adv_summary(&a, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("Apple", s);
    /* Unknown company shows as hex, never guessed. */
    const uint8_t ad2[] = { 0x03, 0xFF, 0x34, 0x12 };
    gz_adv_parse(ad2, sizeof ad2, &a);
    TEST_ASSERT_NULL(gz_adv_company_name(a.company));
    gz_adv_summary(&a, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("mfr 1234", s);
}

static void test_eddystone_service_data(void)
{
    const uint8_t ad[] = { 0x03, 0x03, 0xAA, 0xFE,
                           0x06, 0x16, 0xAA, 0xFE, 0x10, 0x00, 0x01 };
    gz_adv_t a; gz_adv_parse(ad, sizeof ad, &a);
    TEST_ASSERT_EQUAL_STRING("Eddystone", gz_adv_uuid16_name(a.uuid16[0]));
    TEST_ASSERT_TRUE(a.has_svc_data);
    TEST_ASSERT_EQUAL_HEX16(0xFEAA, a.svc_data_uuid);
    TEST_ASSERT_EQUAL_UINT8(3, a.svc_data_len);
}

static void test_malformed_and_terminators(void)
{
    gz_adv_t a;
    /* Second structure claims 9 bytes but only 2 remain: stop, keep the first. */
    const uint8_t bad[] = { 0x02, 0x01, 0x06, 0x09, 0x09, 'x' };
    gz_adv_parse(bad, sizeof bad, &a);
    TEST_ASSERT_TRUE(a.malformed);
    TEST_ASSERT_TRUE(a.has_flags);
    TEST_ASSERT_FALSE(a.has_name);
    /* Length 0 ends the payload; trailing zero padding is fine. */
    const uint8_t pad[] = { 0x02, 0x01, 0x06, 0x00, 0x05, 0x09, 'n','o','p','e' };
    gz_adv_parse(pad, sizeof pad, &a);
    TEST_ASSERT_FALSE(a.malformed);
    TEST_ASSERT_FALSE(a.has_name);
    /* Empty and one-byte inputs. */
    gz_adv_parse(pad, 0, &a);  TEST_ASSERT_FALSE(a.has_flags);
    const uint8_t one[] = { 0x05 };
    gz_adv_parse(one, 1, &a);  TEST_ASSERT_TRUE(a.malformed);
    char s[16]; gz_adv_parse(pad, 0, &a); gz_adv_summary(&a, s, sizeof s);
    TEST_ASSERT_EQUAL_STRING("(no data)", s);
}

static void test_names_and_lists(void)
{
    gz_adv_t a;
    /* Non-printable bytes become '?'; complete name beats a later short name. */
    const uint8_t ad[] = { 0x05, 0x09, 'A', 0x01, 'B', 0xC3,
                           0x03, 0x08, 'Z', 'Z' };
    gz_adv_parse(ad, sizeof ad, &a);
    TEST_ASSERT_EQUAL_STRING("A?B?", a.name);
    /* Long names are cut at GZ_ADV_NAME_MAX. */
    uint8_t lng[2 + 40]; lng[0] = 41; lng[1] = 0x09; memset(lng + 2, 'q', 40);
    gz_adv_parse(lng, sizeof lng, &a);
    TEST_ASSERT_EQUAL_size_t(GZ_ADV_NAME_MAX, strlen(a.name));
    /* 16-bit list capped at 8; 128-bit UUIDs counted. */
    uint8_t lst[2 + 20 + 2 + 32]; size_t k = 0;
    lst[k++] = 21; lst[k++] = 0x03; for (int i = 0; i < 10; ++i) { lst[k++] = (uint8_t)i; lst[k++] = 0x18; }
    lst[k++] = 33; lst[k++] = 0x07; memset(lst + k, 0x11, 32); k += 32;
    gz_adv_parse(lst, (int)k, &a);
    TEST_ASSERT_EQUAL_UINT8(GZ_ADV_UUID16_MAX, a.n_uuid16);
    TEST_ASSERT_EQUAL_UINT8(2, a.n_uuid128);
    /* Appearance categories and HID by category. */
    TEST_ASSERT_EQUAL_STRING("phone", gz_adv_appearance_name(0x0040));
    TEST_ASSERT_EQUAL_STRING("watch", gz_adv_appearance_name(0x00C1));
    TEST_ASSERT_EQUAL_STRING("mouse", gz_adv_appearance_name(0x03C2));
    TEST_ASSERT_NULL(gz_adv_appearance_name(0x0000));
    const uint8_t hidapp[] = { 0x03, 0x19, 0xCA, 0x03 };   /* HID presentation remote */
    gz_adv_parse(hidapp, sizeof hidapp, &a);
    TEST_ASSERT_TRUE(gz_adv_is_hid(&a));
}

static void test_lab_table(void)
{
    static gz_lab_table_t t; gz_lab_clear(&t);
    uint8_t a1[6] = { 1,2,3,4,5,6 }, a2[6] = { 1,2,3,4,5,7 }, zero[6] = { 0 };
    TEST_ASSERT_EQUAL_INT(-1, gz_lab_note(&t, zero, 0, -50, KBD, sizeof KBD, 0));
    int i = gz_lab_note(&t, a1, 1, -70, KBD, sizeof KBD, 1000);
    TEST_ASSERT_EQUAL_INT(0, i);
    TEST_ASSERT_EQUAL_UINT8(sizeof KBD, t.e[0].len);
    TEST_ASSERT_EQUAL_MEMORY(KBD, t.e[0].payload, sizeof KBD);
    TEST_ASSERT_EQUAL_INT(0, gz_lab_note(&t, a1, 1, -60, KBD, 3, 2000));   /* update in place */
    TEST_ASSERT_EQUAL_UINT16(2, t.e[0].seen);
    TEST_ASSERT_EQUAL_UINT8(3, t.e[0].len);
    TEST_ASSERT_EQUAL_INT8(-60, t.e[0].rssi);
    /* Extended advert longer than 31 bytes: kept truncated and flagged. */
    uint8_t big[60]; memset(big, 0x02, sizeof big);
    gz_lab_note(&t, a2, 0, -80, big, sizeof big, 3000);
    TEST_ASSERT_TRUE(t.e[1].truncated);
    TEST_ASSERT_EQUAL_UINT8(GZ_LAB_PAYLOAD, t.e[1].len);
    /* Fill, then a new device evicts the least recently heard (a1 @2000). */
    for (int k = 2; k < GZ_LAB_MAX; ++k) { uint8_t ak[6] = { 9,9,9,9,(uint8_t)k,1 };
        gz_lab_note(&t, ak, 0, -90, KBD, 3, 5000u + (uint32_t)k); }
    TEST_ASSERT_EQUAL_INT(GZ_LAB_MAX, t.n);
    uint8_t anew[6] = { 7,7,7,7,7,7 };
    TEST_ASSERT_EQUAL_INT(0, gz_lab_note(&t, anew, 0, -40, KBD, 3, 9000));
    TEST_ASSERT_EQUAL_MEMORY(anew, t.e[0].addr, 6);
    TEST_ASSERT_EQUAL_UINT16(1, t.e[0].seen);
    /* Sort a copy strongest-first. */
    gz_lab_row_t rows[GZ_LAB_MAX]; memcpy(rows, t.e, sizeof rows);
    gz_lab_sort_by_rssi(rows, t.n);
    TEST_ASSERT_EQUAL_INT8(-40, rows[0].rssi);
    for (int k = 1; k < t.n; ++k) TEST_ASSERT_TRUE(rows[k - 1].rssi >= rows[k].rssi);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_keyboard_advert);
    RUN_TEST(test_manufacturer_and_tx);
    RUN_TEST(test_eddystone_service_data);
    RUN_TEST(test_malformed_and_terminators);
    RUN_TEST(test_names_and_lists);
    RUN_TEST(test_lab_table);
    return UNITY_END();
}
