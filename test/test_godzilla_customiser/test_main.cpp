/* Native Unity test for the on-device UI customiser's pure model
 * (watch-faces orders, unit 4): gz_safearea.h + gz_customiser.h.
 *   1. safe-area box per board shape: on every board the layout box lies wholly
 *      inside the safe area (the safe circle on the round 1.85B, the margin
 *      inset on the rectangular 2.06 / ES3C28P), is square on the round panel,
 *      and is centred;
 *   2. slot-availability filter: the per-slot picker offers only complications
 *      the board can source (plus the empty slot) — never GZ_COMP_CLOCK on a
 *      board without an RTC, never GZ_COMP_IMU without an IMU, Battery only
 *      where powered — and the pod cycler steps over exactly that set.
 * All three boards from hand-built masks, like test_godzilla_geom. */
#include <unity.h>
#include <stdio.h>
#include "../../godzilla_node/main/gz_safearea.h"
#include "../../godzilla_node/main/gz_customiser.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t CAPS_185B = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_SD;
static const uint8_t CAPS_206  = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_BATT | GZ_CAP_SD;
static const uint8_t CAPS_ES3  = GZ_CAP_BATT | GZ_CAP_SD;   /* no RTC, no IMU */

typedef struct { const char *name; int w, h; bool round; } board_t;
static const board_t B185 = { "1.85B",   360, 360, true  };
static const board_t B206 = { "2.06",    410, 502, false };
static const board_t BES3 = { "ES3C28P", 240, 320, false };
static const board_t ALL[] = { B185, B206, BES3 };

/* ---- 1. safe-area box ---------------------------------------------------- */
static void test_safe_box_inside_all_boards(void)
{
    const int margin = 8;
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        gz_rect_t box = gz_safe_box(bd.w, bd.h, bd.round, margin);
        char msg[48];
        snprintf(msg, sizeof msg, "%s box clips", bd.name);
        TEST_ASSERT_TRUE_MESSAGE(
            gz_safe_rect_inside(box, bd.w, bd.h, bd.round, margin), msg);
        /* centred */
        TEST_ASSERT_EQUAL_INT(box.x, (bd.w - box.w) / 2);
        TEST_ASSERT_EQUAL_INT(box.y, (bd.h - box.h) / 2);
        TEST_ASSERT_TRUE(box.w > 0 && box.h > 0);
    }
}

static void test_round_box_is_square_rect_is_inset(void)
{
    const int margin = 8;
    gz_rect_t r = gz_safe_box(B185.w, B185.h, true, margin);
    TEST_ASSERT_EQUAL_INT(r.w, r.h);                 /* inscribed square */
    TEST_ASSERT_TRUE(r.w < B185.w);                  /* smaller than the panel */

    gz_rect_t q = gz_safe_box(B206.w, B206.h, false, margin);
    TEST_ASSERT_EQUAL_INT(B206.w - 2 * margin, q.w); /* plain inset */
    TEST_ASSERT_EQUAL_INT(B206.h - 2 * margin, q.h);
}

/* ---- 2. slot-availability filter ----------------------------------------- */
static int has(const gz_comp_id_t *a, int n, gz_comp_id_t c)
{
    for (int i = 0; i < n; ++i) if (a[i] == c) return 1;
    return 0;
}

static void test_options_follow_board_caps(void)
{
    gz_comp_id_t opt[GZ_COMP_COUNT];

    int n185 = gz_cust_comp_options(CAPS_185B, opt);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, opt[0]);        /* empty slot first */
    TEST_ASSERT_TRUE(has(opt, n185, GZ_COMP_CLOCK));    /* RTC present */
    TEST_ASSERT_TRUE(has(opt, n185, GZ_COMP_IMU));      /* IMU present */
    TEST_ASSERT_FALSE(has(opt, n185, GZ_COMP_BATTERY)); /* gauge undriven */

    int n206 = gz_cust_comp_options(CAPS_206, opt);
    TEST_ASSERT_TRUE(has(opt, n206, GZ_COMP_BATTERY));  /* powered */

    int nes3 = gz_cust_comp_options(CAPS_ES3, opt);
    TEST_ASSERT_FALSE(has(opt, nes3, GZ_COMP_CLOCK));   /* no RTC */
    TEST_ASSERT_FALSE(has(opt, nes3, GZ_COMP_IMU));     /* no IMU */
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_BATTERY));  /* has battery */
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_RECON));    /* has SD */
    /* the four all-board complications are always on offer */
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_STATUS));
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_CONTACTS));
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_PUBLISH));
    TEST_ASSERT_TRUE(has(opt, nes3, GZ_COMP_CHANNEL));
    /* fewer options on the barer board */
    TEST_ASSERT_TRUE(nes3 < n185);
}

static void test_every_offered_option_is_sourceable(void)
{
    const uint8_t boards[] = { CAPS_185B, CAPS_206, CAPS_ES3 };
    gz_comp_id_t opt[GZ_COMP_COUNT];
    for (unsigned b = 0; b < 3; ++b) {
        int n = gz_cust_comp_options(boards[b], opt);
        for (int i = 0; i < n; ++i)
            TEST_ASSERT_TRUE(gz_comp_available(opt[i], boards[b]));
    }
}

static void test_pod_cycler_skips_unavailable(void)
{
    /* On ES3 (no RTC/IMU) cycling never lands on CLOCK or IMU, and returns to
     * the start after a full loop. */
    gz_comp_id_t opt[GZ_COMP_COUNT];
    int n = gz_cust_comp_options(CAPS_ES3, opt);
    gz_comp_id_t c = GZ_COMP_NONE;
    for (int i = 0; i < n * 2; ++i) {
        c = gz_cust_cycle_comp(c, +1, CAPS_ES3);
        TEST_ASSERT_TRUE(c != GZ_COMP_CLOCK && c != GZ_COMP_IMU);
        TEST_ASSERT_TRUE(gz_comp_available(c, CAPS_ES3));
    }
    /* n forward steps from NONE returns to NONE */
    c = GZ_COMP_NONE;
    for (int i = 0; i < n; ++i) c = gz_cust_cycle_comp(c, +1, CAPS_ES3);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, c);
    /* -1 from NONE wraps to the last option */
    TEST_ASSERT_EQUAL_INT(opt[n - 1], gz_cust_cycle_comp(GZ_COMP_NONE, -1, CAPS_ES3));
}

static void test_face_and_scalar_cyclers(void)
{
    /* Every face fits every board, so the face cycler walks all four and wraps. */
    gz_face_id_t opt[GZ_FACE_COUNT];
    int n = gz_cust_face_options(CAPS_ES3, opt);
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COUNT, n);
    TEST_ASSERT_EQUAL_INT(GZ_FACE_COMMAND, gz_cust_cycle_face(GZ_FACE_CLASSIC, +1, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_FACE_MINIMAL, gz_cust_cycle_face(GZ_FACE_CLASSIC, -1, CAPS_ES3));
    /* accent / background scalar cyclers wrap in range */
    TEST_ASSERT_EQUAL_INT(0, gz_cust_cycle(GZ_ACCENT_COUNT - 1, +1, GZ_ACCENT_COUNT));
    TEST_ASSERT_EQUAL_INT(GZ_ACCENT_COUNT - 1, gz_cust_cycle(0, -1, GZ_ACCENT_COUNT));
    TEST_ASSERT_EQUAL_INT(GZ_BG_BLACK, gz_cust_cycle(GZ_BG_TEXTURE, +1, GZ_BG_COUNT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_safe_box_inside_all_boards);
    RUN_TEST(test_round_box_is_square_rect_is_inset);
    RUN_TEST(test_options_follow_board_caps);
    RUN_TEST(test_every_offered_option_is_sourceable);
    RUN_TEST(test_pod_cycler_skips_unavailable);
    RUN_TEST(test_face_and_scalar_cyclers);
    return UNITY_END();
}
