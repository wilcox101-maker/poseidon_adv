/* Native Unity test for godzilla_node/main/ui/faces/gz_faces.h — the pure
 * watch-faces model (watch-faces orders, unit 1). Three jobs:
 *   1. registry selection: the requested face wins when it fits the board,
 *      else the first registered face that fits (CLASSIC) is chosen, and an
 *      out-of-range request falls back cleanly;
 *   2. capability filter: a complication a board can't source is hidden on
 *      that board and shown where the hardware exists, for all three boards;
 *   3. the NVS settings codec round-trips (encode -> decode == identity), and
 *      an unknown / short / corrupt blob falls back to defaults.
 * All three boards are checked in one TU from hand-built capability masks, the
 * same way test_godzilla_geom checks geometry. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/ui/faces/gz_faces.h"

void setUp(void) {}
void tearDown(void) {}

/* The three shipping boards as capability masks (from their gz_board_*.h). */
static const uint8_t CAPS_185B = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_SD; /* gauge undriven: no BATT */
static const uint8_t CAPS_206  = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_BATT | GZ_CAP_SD;
static const uint8_t CAPS_ES3  = GZ_CAP_BATT | GZ_CAP_SD;   /* no RTC, no IMU */

/* ---- 1. registry selection ---------------------------------------------- */
static void test_select_requested_when_available(void)
{
    /* On an RTC board every face fits, so a valid request is honoured. */
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_COMMAND, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_CLASSIC, gz_face_select(GZ_FACE_CLASSIC, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_MINIMAL, gz_face_select(GZ_FACE_MINIMAL, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_MODULAR, gz_face_select(GZ_FACE_MODULAR, CAPS_206));
    /* On a no-RTC board only COMMAND fits, so only it is honoured as requested. */
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_COMMAND, CAPS_ES3));
}

static void test_select_out_of_range_falls_back_to_first_fit(void)
{
    /* Garbage / old face ids fall back to the first registered face that fits.
     * On an RTC board that is CLASSIC; on a no-RTC board CLASSIC is a clock and
     * no longer fits, so the walk lands on COMMAND (caps 0). */
    TEST_ASSERT_EQUAL_INT(GZ_FACE_CLASSIC, gz_face_select(-1,  CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_CLASSIC, gz_face_select(GZ_FACE_COUNT, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(-1,  CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(99,  CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_COUNT, CAPS_ES3));
}

static void test_caps_filter_primitive(void)
{
    /* The filter underneath selection: a face/complication is allowed iff the
     * board sources every cap it needs. Prove the primitive directly. */
    TEST_ASSERT_TRUE (gz_caps_ok(0, 0));
    TEST_ASSERT_TRUE (gz_caps_ok(GZ_CAP_RTC, GZ_CAP_RTC | GZ_CAP_IMU));
    TEST_ASSERT_FALSE(gz_caps_ok(GZ_CAP_RTC, GZ_CAP_IMU));
    TEST_ASSERT_FALSE(gz_caps_ok(GZ_CAP_RTC | GZ_CAP_IMU, GZ_CAP_RTC));
    /* On a capability-less board only COMMAND (caps 0) is available; the three
     * clock faces need GZ_CAP_RTC and are filtered out. */
    TEST_ASSERT_TRUE (gz_face_available(GZ_FACE_COMMAND, 0));
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_CLASSIC, 0));
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_MODULAR, 0));
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_MINIMAL, 0));
}

/* ---- 1b. no-RTC-no-clock-faces (Dale 2026-11-20) ------------------------- */
static void test_clock_faces_gated_out_without_rtc(void)
{
    /* On the RTC-less ES3C28P the three clock faces are unavailable whole and
     * COMMAND is the only available face. */
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_CLASSIC, CAPS_ES3));
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_MODULAR, CAPS_ES3));
    TEST_ASSERT_FALSE(gz_face_available(GZ_FACE_MINIMAL, CAPS_ES3));
    TEST_ASSERT_TRUE (gz_face_available(GZ_FACE_COMMAND, CAPS_ES3));

    /* Requesting any clock face on a no-RTC board resolves to COMMAND, never a
     * blank screen. */
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_CLASSIC, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_MODULAR, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_face_select(GZ_FACE_MINIMAL, CAPS_ES3));
    /* The factory default (CLASSIC) stored on a no-RTC board resolves the same. */
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);              /* face = CLASSIC */
    gz_ui_settings_sanitise(&s, CAPS_ES3);
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, s.face);
}

static void test_all_faces_available_on_rtc_boards(void)
{
    /* Both RTC boards keep all four faces, and each request is honoured. */
    const uint8_t rtc_boards[] = { CAPS_185B, CAPS_206 };
    for (unsigned b = 0; b < sizeof rtc_boards / sizeof rtc_boards[0]; ++b)
        for (int f = 0; f < GZ_FACE_COUNT; ++f) {
            TEST_ASSERT_TRUE(gz_face_available((gz_face_id_t)f, rtc_boards[b]));
            TEST_ASSERT_EQUAL_INT(f, gz_face_select(f, rtc_boards[b]));
        }
}

/* ---- 2. capability filter ------------------------------------------------ */
static void test_all_board_complications_show_everywhere(void)
{
    const gz_comp_id_t all[] = { GZ_COMP_STATUS, GZ_COMP_CONTACTS,
                                 GZ_COMP_PUBLISH, GZ_COMP_CHANNEL, GZ_COMP_DECODE };
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; ++i) {
        TEST_ASSERT_TRUE(gz_comp_available(all[i], CAPS_185B));
        TEST_ASSERT_TRUE(gz_comp_available(all[i], CAPS_206));
        TEST_ASSERT_TRUE(gz_comp_available(all[i], CAPS_ES3));
    }
}

static void test_gated_complications_follow_the_hardware(void)
{
    /* IMU + clock are present on 1.85B / AMOLED, absent on ES3C28P. */
    TEST_ASSERT_TRUE (gz_comp_available(GZ_COMP_IMU,   CAPS_185B));
    TEST_ASSERT_TRUE (gz_comp_available(GZ_COMP_IMU,   CAPS_206));
    TEST_ASSERT_FALSE(gz_comp_available(GZ_COMP_IMU,   CAPS_ES3));

    TEST_ASSERT_TRUE (gz_comp_available(GZ_COMP_CLOCK, CAPS_185B));
    TEST_ASSERT_FALSE(gz_comp_available(GZ_COMP_CLOCK, CAPS_ES3));

    /* Recon needs an SD slot; all three have one. Battery likewise. */
    TEST_ASSERT_TRUE(gz_comp_available(GZ_COMP_RECON,   CAPS_ES3));
    TEST_ASSERT_TRUE(gz_comp_available(GZ_COMP_BATTERY, CAPS_ES3));

    /* An empty slot is always allowed; an out-of-range id never is. */
    TEST_ASSERT_TRUE (gz_comp_available(GZ_COMP_NONE, CAPS_ES3));
    TEST_ASSERT_FALSE(gz_comp_available((gz_comp_id_t)GZ_COMP_COUNT, CAPS_185B));
}

static void test_sanitise_drops_unsourceable_slots(void)
{
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);
    s.slots[0] = GZ_COMP_IMU;      /* not on ES3C28P */
    s.slots[1] = GZ_COMP_CLOCK;    /* not on ES3C28P */
    s.slots[2] = GZ_COMP_RECON;    /* ok (SD) */
    s.slots[3] = GZ_COMP_CONTACTS; /* ok (all) */
    s.accent   = 200;              /* out of range */
    s.background = 7;              /* out of range */

    gz_ui_settings_sanitise(&s, CAPS_ES3);

    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE,     s.slots[0]);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE,     s.slots[1]);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_RECON,    s.slots[2]);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_CONTACTS, s.slots[3]);
    TEST_ASSERT_EQUAL_INT(0,                s.accent);
    TEST_ASSERT_EQUAL_INT(GZ_BG_BLACK,      s.background);
    /* On ES3C28P the same slots survive untouched. */
    gz_ui_settings_t t;
    gz_ui_settings_defaults(&t);
    t.slots[0] = GZ_COMP_IMU;
    gz_ui_settings_sanitise(&t, CAPS_185B);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_IMU, t.slots[0]);
}

/* ---- 3. settings codec --------------------------------------------------- */
static void test_codec_round_trips(void)
{
    gz_ui_settings_t in;
    gz_ui_settings_defaults(&in);
    in.face       = GZ_FACE_COMMAND;
    in.accent     = 5;
    in.background = GZ_BG_TEXTURE;
    in.always_on  = 1;
    in.slots[0]   = GZ_COMP_IMU;
    in.slots[1]   = GZ_COMP_BATTERY;
    in.slots[2]   = GZ_COMP_RECON;
    in.slots[3]   = GZ_COMP_DECODE;
    in.finish     = GZ_FINISH_SILVER;   /* v2: the case-finish byte round-trips */

    uint8_t blob[GZ_SETTINGS_WIRE_LEN];
    int n = gz_ui_settings_encode(&in, blob);
    TEST_ASSERT_EQUAL_INT(GZ_SETTINGS_WIRE_LEN, n);

    gz_ui_settings_t out;
    TEST_ASSERT_TRUE(gz_ui_settings_decode(blob, n, &out));
    TEST_ASSERT_EQUAL_MEMORY(&in, &out, sizeof in);
}

static void test_codec_defaults_round_trip(void)
{
    gz_ui_settings_t d, out;
    gz_ui_settings_defaults(&d);
    uint8_t blob[GZ_SETTINGS_WIRE_LEN];
    int n = gz_ui_settings_encode(&d, blob);
    TEST_ASSERT_TRUE(gz_ui_settings_decode(blob, n, &out));
    TEST_ASSERT_EQUAL_MEMORY(&d, &out, sizeof d);
}

static void test_codec_rejects_bad_blobs_to_defaults(void)
{
    gz_ui_settings_t def, out;
    gz_ui_settings_defaults(&def);

    /* null / short / long / bad magic / bad version all -> defaults, false. */
    TEST_ASSERT_FALSE(gz_ui_settings_decode(NULL, GZ_SETTINGS_WIRE_LEN, &out));
    TEST_ASSERT_EQUAL_MEMORY(&def, &out, sizeof def);

    uint8_t good[GZ_SETTINGS_WIRE_LEN];
    gz_ui_settings_encode(&def, good);

    TEST_ASSERT_FALSE(gz_ui_settings_decode(good, GZ_SETTINGS_WIRE_LEN - 1, &out));
    TEST_ASSERT_EQUAL_MEMORY(&def, &out, sizeof def);
    TEST_ASSERT_FALSE(gz_ui_settings_decode(good, GZ_SETTINGS_WIRE_LEN + 1, &out));

    uint8_t bad_magic[GZ_SETTINGS_WIRE_LEN];
    memcpy(bad_magic, good, sizeof good);
    bad_magic[0] = 0x00;
    TEST_ASSERT_FALSE(gz_ui_settings_decode(bad_magic, GZ_SETTINGS_WIRE_LEN, &out));
    TEST_ASSERT_EQUAL_MEMORY(&def, &out, sizeof def);

    uint8_t bad_ver[GZ_SETTINGS_WIRE_LEN];
    memcpy(bad_ver, good, sizeof good);
    bad_ver[1] = 0xFE;   /* an older/newer schema */
    TEST_ASSERT_FALSE(gz_ui_settings_decode(bad_ver, GZ_SETTINGS_WIRE_LEN, &out));
    TEST_ASSERT_EQUAL_MEMORY(&def, &out, sizeof def);
}

/* ---- 3b. v2 case-finish byte + v1 fall-back ------------------------------ */
static void test_codec_v2_finish_byte(void)
{
    /* v2 is an 11-byte blob at version 2, with the finish as the last byte. */
    TEST_ASSERT_EQUAL_INT(2u, GZ_SETTINGS_VERSION);
    TEST_ASSERT_EQUAL_INT(6 + GZ_FACE_SLOTS + 1, GZ_SETTINGS_WIRE_LEN);

    gz_ui_settings_t in, out;
    gz_ui_settings_defaults(&in);
    in.finish = GZ_FINISH_SILVER;
    uint8_t blob[GZ_SETTINGS_WIRE_LEN];
    int n = gz_ui_settings_encode(&in, blob);
    TEST_ASSERT_EQUAL_INT(2u, blob[1]);                       /* version byte  */
    TEST_ASSERT_EQUAL_INT(GZ_FINISH_SILVER, blob[n - 1]);     /* finish is last */
    TEST_ASSERT_TRUE(gz_ui_settings_decode(blob, n, &out));
    TEST_ASSERT_EQUAL_INT(GZ_FINISH_SILVER, out.finish);
}

static void test_codec_v1_blob_decodes_to_defaults(void)
{
    /* A real v1 blob: the old 10-byte layout (no finish byte), version 1. Both
     * its version byte and its length differ from v2, so decode must reject it
     * and fall back to defaults — the existing short/old-blob path, unchanged. */
    gz_ui_settings_t def, out;
    gz_ui_settings_defaults(&def);

    const int V1_LEN = 6 + GZ_FACE_SLOTS;                     /* 10 bytes */
    uint8_t v1[V1_LEN];
    v1[0] = GZ_SETTINGS_MAGIC;
    v1[1] = 1u;                                               /* v1 */
    v1[2] = GZ_FACE_COMMAND; v1[3] = 3; v1[4] = GZ_BG_TEXTURE; v1[5] = 1;
    for (int i = 0; i < GZ_FACE_SLOTS; ++i) v1[6 + i] = GZ_COMP_STATUS;

    TEST_ASSERT_FALSE(gz_ui_settings_decode(v1, V1_LEN, &out));
    TEST_ASSERT_EQUAL_MEMORY(&def, &out, sizeof def);         /* incl. finish = GOLD */
    TEST_ASSERT_EQUAL_INT(GZ_FINISH_GOLD, out.finish);
}

static void test_sanitise_clamps_finish(void)
{
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);
    s.finish = 200;                                           /* out of range */
    gz_ui_settings_sanitise(&s, CAPS_185B);
    TEST_ASSERT_EQUAL_INT(GZ_FINISH_GOLD, s.finish);
    /* a valid finish survives */
    s.finish = GZ_FINISH_SILVER;
    gz_ui_settings_sanitise(&s, CAPS_185B);
    TEST_ASSERT_EQUAL_INT(GZ_FINISH_SILVER, s.finish);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_select_requested_when_available);
    RUN_TEST(test_select_out_of_range_falls_back_to_first_fit);
    RUN_TEST(test_caps_filter_primitive);
    RUN_TEST(test_clock_faces_gated_out_without_rtc);
    RUN_TEST(test_all_faces_available_on_rtc_boards);
    RUN_TEST(test_all_board_complications_show_everywhere);
    RUN_TEST(test_gated_complications_follow_the_hardware);
    RUN_TEST(test_sanitise_drops_unsourceable_slots);
    RUN_TEST(test_codec_round_trips);
    RUN_TEST(test_codec_defaults_round_trip);
    RUN_TEST(test_codec_rejects_bad_blobs_to_defaults);
    RUN_TEST(test_codec_v2_finish_byte);
    RUN_TEST(test_codec_v1_blob_decodes_to_defaults);
    RUN_TEST(test_sanitise_clamps_finish);
    return UNITY_END();
}
