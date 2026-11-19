/* Native Unity test for godzilla_node/main/gz_recon.h — the pure Recon session
 * log model (P3): the "row due" policy, the publish->SD ring queue, 8.3-safe
 * session paths and the CSV text. The SD card and writer task are on-target. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_recon.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t A1[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };

static gz_datetime_t when(void)
{
    gz_datetime_t t;
    t.year = 2026; t.month = 11; t.day = 19; t.hour = 5; t.minute = 10; t.second = 3;
    t.weekday = 4; t.osc_stopped = false;
    return t;
}

static gz_radar_table_t T;
static gz_recon_q_t Q;

static void test_due_policy(void)
{
    gz_radar_clear(&T);
    int i = gz_radar_note(&T, GZ_RADAR_BLE, A1, 1, -70, POSEI_DET_FAM_AIRTAG, 1000);
    gz_radar_row_t *r = &T.e[i];
    TEST_ASSERT_FALSE(r->logged);                     /* new rows start unlogged */
    TEST_ASSERT_TRUE(gz_recon_due(r, 1000));          /* first sighting */

    r->logged = true; r->log_ms = 1000;
    TEST_ASSERT_FALSE(gz_recon_due(r, 1000 + GZ_RECON_REPEAT_MS - 1));
    TEST_ASSERT_TRUE(gz_recon_due(r, 1000 + GZ_RECON_REPEAT_MS));

    /* Across the millis() wrap. */
    r->log_ms = 0xFFFFFF00u;
    TEST_ASSERT_FALSE(gz_recon_due(r, 0x00000100u));
    TEST_ASSERT_TRUE(gz_recon_due(r, 0xFFFFFF00u + GZ_RECON_REPEAT_MS));

    /* A re-sighting keeps the logged state... */
    gz_radar_note(&T, GZ_RADAR_BLE, A1, 1, -60, POSEI_DET_FAM_AIRTAG, 2000);
    TEST_ASSERT_TRUE(T.e[i].logged);
}

static void test_eviction_resets_logged(void)
{
    gz_radar_clear(&T);
    uint8_t a[6] = { 1, 0, 0, 0, 0, 0 };
    for (int k = 0; k < GZ_RADAR_MAX; ++k) {
        a[5] = (uint8_t)k;
        int i = gz_radar_note(&T, GZ_RADAR_WIFI, a, 6, -70, POSEI_DET_FAM_WIFI_AP, 100u + k);
        T.e[i].logged = true;
        T.e[i].log_ms = 100u + k;
    }
    uint8_t fresh[6] = { 9, 9, 9, 9, 9, 9 };
    int i = gz_radar_note(&T, GZ_RADAR_WIFI, fresh, 1, -50, POSEI_DET_FAM_WIFI_AP, 5000);
    TEST_ASSERT_EQUAL_INT(0, i);                      /* oldest row replaced */
    TEST_ASSERT_FALSE(T.e[i].logged);                 /* ...and is a new device */
    TEST_ASSERT_TRUE(gz_recon_due(&T.e[i], 5000));
}

static void test_queue_fifo_wrap_and_drop(void)
{
    gz_recon_q_clear(&Q);
    gz_radar_row_t r;
    memset(&r, 0, sizeof r);
    gz_radar_row_t out[GZ_RECON_Q_LEN];

    TEST_ASSERT_EQUAL_INT(0, gz_recon_q_pop(&Q, out, 8));

    /* Fill, then one more is refused and counted. */
    for (int k = 0; k < GZ_RECON_Q_LEN; ++k) {
        r.hits = (uint16_t)k;
        TEST_ASSERT_TRUE(gz_recon_q_push(&Q, &r));
    }
    r.hits = 999;
    TEST_ASSERT_FALSE(gz_recon_q_push(&Q, &r));
    TEST_ASSERT_EQUAL_UINT32(1, Q.dropped);

    /* Partial pop is oldest-first. */
    TEST_ASSERT_EQUAL_INT(10, gz_recon_q_pop(&Q, out, 10));
    TEST_ASSERT_EQUAL_UINT16(0, out[0].hits);
    TEST_ASSERT_EQUAL_UINT16(9, out[9].hits);

    /* Pushes now wrap around the ring; order is kept. */
    for (int k = 0; k < 10; ++k) {
        r.hits = (uint16_t)(100 + k);
        TEST_ASSERT_TRUE(gz_recon_q_push(&Q, &r));
    }
    int n = gz_recon_q_pop(&Q, out, GZ_RECON_Q_LEN);
    TEST_ASSERT_EQUAL_INT(GZ_RECON_Q_LEN, n);
    TEST_ASSERT_EQUAL_UINT16(10, out[0].hits);
    TEST_ASSERT_EQUAL_UINT16(GZ_RECON_Q_LEN - 1, out[GZ_RECON_Q_LEN - 11].hits);
    TEST_ASSERT_EQUAL_UINT16(100, out[GZ_RECON_Q_LEN - 10].hits);
    TEST_ASSERT_EQUAL_UINT16(109, out[GZ_RECON_Q_LEN - 1].hits);
    TEST_ASSERT_EQUAL_UINT16(0, Q.count);
}

static void test_paths_are_8_3_safe(void)
{
    char dir[GZ_RECON_PATH_MAX], file[GZ_RECON_PATH_MAX];
    gz_datetime_t t = when();
    TEST_ASSERT_TRUE(gz_recon_paths(&t, dir, sizeof dir, file, sizeof file));
    TEST_ASSERT_EQUAL_STRING("/recon/2026", dir);
    TEST_ASSERT_EQUAL_STRING("/recon/2026/11190510.CSV", file);
    /* Base name is exactly 8 characters + ".CSV". */
    const char *base = strrchr(file, '/') + 1;
    TEST_ASSERT_EQUAL_INT(12, (int)strlen(base));

    /* No usable time: a fixed fallback, still 8.3. */
    TEST_ASSERT_TRUE(gz_recon_paths(NULL, dir, sizeof dir, file, sizeof file));
    TEST_ASSERT_EQUAL_STRING("/recon/NODATE", dir);
    TEST_ASSERT_EQUAL_STRING("/recon/NODATE/SESSION.CSV", file);
    t.month = 13;
    TEST_ASSERT_TRUE(gz_recon_paths(&t, dir, sizeof dir, file, sizeof file));
    TEST_ASSERT_EQUAL_STRING("/recon/NODATE/SESSION.CSV", file);

    /* Too-small buffers are reported, not overrun. */
    char tiny[8];
    t = when();
    TEST_ASSERT_FALSE(gz_recon_paths(&t, tiny, sizeof tiny, file, sizeof file));
}

static void test_csv_rows(void)
{
    char buf[GZ_RECON_ROW_MAX];
    gz_datetime_t t = when();
    gz_radar_row_t r;
    memset(&r, 0, sizeof r);
    memcpy(r.addr, A1, 6);

    r.kind = GZ_RADAR_WIFI; r.aux = 11; r.rssi = -67;
    r.family = POSEI_DET_FAM_WIFI_AP; r.hits = 3; r.last_seen_ms = 123456;
    int n = gz_recon_format_row(buf, sizeof buf, &t, &r);
    TEST_ASSERT_EQUAL_STRING(
        "2026-11-19T05:10:03,123456,wifi,11:22:33:44:55:66,11,,-67,Wi-Fi AP,3\n", buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);

    r.kind = GZ_RADAR_BLE; r.aux = 1; r.rssi = -81;
    r.family = POSEI_DET_FAM_AIRTAG; r.hits = 40;
    gz_recon_format_row(buf, sizeof buf, &t, &r);
    TEST_ASSERT_EQUAL_STRING(
        "2026-11-19T05:10:03,123456,ble,11:22:33:44:55:66,,random,-81,AirTag,40\n", buf);

    /* No clock: the time field is empty, the row is still well-formed. */
    r.aux = 0;
    gz_recon_format_row(buf, sizeof buf, NULL, &r);
    TEST_ASSERT_EQUAL_STRING(",123456,ble,11:22:33:44:55:66,,public,-81,AirTag,40\n", buf);

    /* Every row has the header's column count. */
    int commas_h = 0, commas_r = 0;
    for (const char *p = GZ_RECON_CSV_HEADER; *p; ++p) commas_h += (*p == ',');
    for (const char *p = buf; *p; ++p) commas_r += (*p == ',');
    TEST_ASSERT_EQUAL_INT(commas_h, commas_r);

    /* Worst case fits GZ_RECON_ROW_MAX; a short buffer returns 0. */
    r.kind = GZ_RADAR_WIFI; r.aux = 165; r.rssi = -128; r.hits = 65535;
    r.family = POSEI_DET_FAM_REMOTEID; r.last_seen_ms = 0xFFFFFFFFu;
    TEST_ASSERT_TRUE(gz_recon_format_row(buf, sizeof buf, &t, &r) > 0);
    char small[20];
    TEST_ASSERT_EQUAL_INT(0, gz_recon_format_row(small, sizeof small, &t, &r));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_due_policy);
    RUN_TEST(test_eviction_resets_logged);
    RUN_TEST(test_queue_fifo_wrap_and_drop);
    RUN_TEST(test_paths_are_8_3_safe);
    RUN_TEST(test_csv_rows);
    return UNITY_END();
}
