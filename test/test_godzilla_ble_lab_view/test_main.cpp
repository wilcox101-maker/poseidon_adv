/* Native Unity test for the BLE Lab screen's pure half (#98 §2.4b):
 * gz_ble_lab_view.h — rows that fit each panel, tap hit-testing, list-row
 * text, find-by-address and the decoded detail page. */
#include <unity.h>
#include <string.h>
#include <math.h>
#include "../../godzilla_node/main/gz_ble_lab_view.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t KBD[] = { 0x02, 0x01, 0x06,
                               0x03, 0x19, 0xC1, 0x03,
                               0x03, 0x03, 0x12, 0x18,
                               0x0A, 0x09, 'B','T',' ','K','e','y','b','d','!' };

static gz_lab_row_t row(const uint8_t *pl, int len, int8_t rssi, uint8_t last)
{
    gz_lab_row_t r; memset(&r, 0, sizeof r);
    for (int i = 0; i < 6; ++i) r.addr[i] = (uint8_t)(0xA0 + i + last);
    r.atype = 1; r.rssi = rssi; r.seen = 7; r.last_ms = 1000;
    r.len = (uint8_t)len; if (len) memcpy(r.payload, pl, (size_t)len);
    return r;
}

static void test_rows_fit_each_panel(void)
{
    TEST_ASSERT_EQUAL_INT(9,  gz_lab_rows_fit(360, true));    /* 1.85B round    */
    TEST_ASSERT_EQUAL_INT(10, gz_lab_rows_fit(320, false));   /* ES3C28P 240x320 */
    TEST_ASSERT_EQUAL_INT(12, gz_lab_rows_fit(502, false));   /* AMOLED, capped  */
    /* Rectangles: the last row ends above the hint line. */
    const int hs[] = { 320, 502 };
    for (int h : hs) {
        int bottom = gz_lab_list_top(h) + gz_lab_rows_fit(h, false) * GZ_LAB_ROW_H;
        TEST_ASSERT_TRUE(bottom <= gz_lab_hint_y(h, false));
    }
}

static void test_round_rows_stay_on_glass(void)
{
    /* Every row's corners for a 260 px column must sit inside the r=180 glass. */
    const int n = gz_lab_rows_fit(360, true), top = gz_lab_list_top(360);
    for (int i = 0; i <= n; ++i) {
        double dy = (top + i * GZ_LAB_ROW_H) - 180.0, dx = 130.0;
        TEST_ASSERT_TRUE(sqrt(dx * dx + dy * dy) <= 180.0);
    }
    TEST_ASSERT_TRUE(top + n * GZ_LAB_ROW_H <= gz_lab_hint_y(360, true));
}

static void test_row_hit(void)
{
    const int top = gz_lab_list_top(360);
    TEST_ASSERT_EQUAL_INT(-1, gz_lab_row_hit(top - 1, 360, 5));
    TEST_ASSERT_EQUAL_INT(0,  gz_lab_row_hit(top, 360, 5));
    TEST_ASSERT_EQUAL_INT(0,  gz_lab_row_hit(top + GZ_LAB_ROW_H - 1, 360, 5));
    TEST_ASSERT_EQUAL_INT(4,  gz_lab_row_hit(top + 4 * GZ_LAB_ROW_H, 360, 5));
    TEST_ASSERT_EQUAL_INT(-1, gz_lab_row_hit(top + 5 * GZ_LAB_ROW_H, 360, 5));
    TEST_ASSERT_EQUAL_INT(-1, gz_lab_row_hit(top, 360, 0));
    /* ES3C28P list starts 20 px higher (GZ_UI_DY = -20). */
    TEST_ASSERT_EQUAL_INT(48, gz_lab_list_top(320));
}

static void test_row_text(void)
{
    char b[112];
    gz_lab_row_t r = row(KBD, sizeof KBD, -55, 0);
    TEST_ASSERT_TRUE(gz_lab_row_text(&r, b, sizeof b));
    TEST_ASSERT_EQUAL_STRING(" -55  BT Keybd! | keyboard | HID", b);
    /* Nothing nameable: the address stands in. */
    const uint8_t flags[] = { 0x02, 0x01, 0x06 };
    r = row(flags, sizeof flags, -101, 0);
    TEST_ASSERT_FALSE(gz_lab_row_text(&r, b, sizeof b));
    TEST_ASSERT_EQUAL_STRING("-101  A0:A1:A2:A3:A4:A5", b);
}

static void test_find(void)
{
    gz_lab_row_t rows[3] = { row(KBD, 3, -50, 0), row(KBD, 3, -60, 1), row(KBD, 3, -70, 2) };
    TEST_ASSERT_EQUAL_INT(1, gz_lab_find(rows, 3, rows[1].addr));
    uint8_t other[6] = { 1, 1, 1, 1, 1, 1 };
    TEST_ASSERT_EQUAL_INT(-1, gz_lab_find(rows, 3, other));
}

static void test_detail_keyboard(void)
{
    char t[512];
    gz_lab_row_t r = row(KBD, sizeof KBD, -55, 0);
    gz_lab_detail_text(&r, 4000, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("A0:A1:A2:A3:A4:A5\nrandom\n-55 dBm | seen 7 | 3s ago"
                             "\nname: BT Keybd!\nkind: keyboard\nannounces HID"
                             "\nsvc: HID\nflags: 06\n22 B", t);
}

static void test_detail_other_fields(void)
{
    char t[512];
    /* Short name, unknown appearance, unknown + 128-bit services, service
     * data, unknown maker, TX power; truncated; seen 75 s ago. */
    uint8_t ad[] = { 0x03, 0x08, 'Z', 'q',
                     0x03, 0x19, 0x00, 0x00,
                     0x05, 0x02, 0x34, 0x12, 0x0F, 0x18,
                     0x04, 0x16, 0xAA, 0xFE, 0x10,
                     0x04, 0xFF, 0x34, 0x12, 0x01,
                     0x02, 0x0A, 0x04 };
    gz_lab_row_t r = row(ad, sizeof ad, -80, 0);
    r.truncated = true; r.atype = 0;
    gz_lab_detail_text(&r, 76000, t, sizeof t);
    TEST_ASSERT_EQUAL_STRING("A0:A1:A2:A3:A4:A5\npublic\n-80 dBm | seen 7 | 1m15s ago"
                             "\nname: Zq (short)\nkind: 0x0000\nsvc: 1234 Battery"
                             "\ndata: Eddystone, 1 B\nmaker: 1234, 1 B\ntx power: 4 dBm"
                             "\n27 B (cut)", t);
    /* Small buffer: cut cleanly, always terminated. */
    char small[20];
    gz_lab_detail_text(&r, 76000, small, sizeof small);
    TEST_ASSERT_EQUAL_size_t(19, strlen(small));
    /* Malformed advert is flagged. */
    const uint8_t bad[] = { 0x09, 0x09, 'x' };
    r = row(bad, sizeof bad, -90, 0);
    gz_lab_detail_text(&r, 1000, t, sizeof t);
    TEST_ASSERT_NOT_NULL(strstr(t, "3 B | malformed"));
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_rows_fit_each_panel);
    RUN_TEST(test_round_rows_stay_on_glass);
    RUN_TEST(test_row_hit);
    RUN_TEST(test_row_text);
    RUN_TEST(test_find);
    RUN_TEST(test_detail_keyboard);
    RUN_TEST(test_detail_other_fields);
    return UNITY_END();
}
