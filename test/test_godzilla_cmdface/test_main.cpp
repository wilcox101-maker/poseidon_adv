/* Native Unity test for godzilla_node/main/gz_cmdface.h — the pure Command-face
 * model (watch-faces orders, unit 3):
 *   1. pod -> field binding table: the four pods bind to the right
 *      complications and live subjects; the navigation pod is IMU tilt on an
 *      IMU board and Wi-Fi channel otherwise; every pod is board-sourceable
 *      (never faked), and GPS / bearing are nowhere in the table;
 *   2. round-safe pod placement: on all three boards the four corner pods lie
 *      wholly inside the safe area (safe circle on the round 1.85B, inset rect
 *      on the 2.06 / ES3C28P), and the layout is symmetric about the centre;
 *   3. quadrant feature map: a tap opens the feature for the quadrant it fell
 *      in, and a tap on a centre line resolves up/left.
 * All three boards from hand-built masks, the same way test_godzilla_geom /
 * test_godzilla_faces check theirs. */
#include <unity.h>
#include <stdio.h>
#include "../../godzilla_node/main/gz_cmdface.h"

void setUp(void) {}
void tearDown(void) {}

/* The three shipping boards as capability masks (from their gz_board_*.h). */
static const uint8_t CAPS_185B = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_SD;
static const uint8_t CAPS_206  = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_BATT | GZ_CAP_SD;
static const uint8_t CAPS_ES3  = GZ_CAP_BATT | GZ_CAP_SD;   /* no IMU */

typedef struct { const char *name; int w, h; bool round; } board_t;
static const board_t B185 = { "1.85B",   360, 360, true  };
static const board_t B206 = { "2.06",    410, 502, false };
static const board_t BES3 = { "ES3C28P", 240, 320, false };
static const board_t ALL[] = { B185, B206, BES3 };

/* ---- 1. pod -> field binding table --------------------------------------- */
static void test_fixed_pods_bind_all_board_complications(void)
{
    const uint8_t boards[] = { CAPS_185B, CAPS_206, CAPS_ES3 };
    for (unsigned b = 0; b < 3; ++b) {
        TEST_ASSERT_EQUAL_INT(GZ_COMP_STATUS,   gz_cmd_pod_comp(0, boards[b]));
        TEST_ASSERT_EQUAL_INT(GZ_COMP_CONTACTS, gz_cmd_pod_comp(1, boards[b]));
        TEST_ASSERT_EQUAL_INT(GZ_COMP_PUBLISH,  gz_cmd_pod_comp(2, boards[b]));
        /* and they read the right live subjects */
        TEST_ASSERT_EQUAL_INT(GZ_SUBJ_HEAP_KB,   gz_cmd_pod_field(0, boards[b]));
        TEST_ASSERT_EQUAL_INT(GZ_SUBJ_CONTACTS,  gz_cmd_pod_field(1, boards[b]));
        TEST_ASSERT_EQUAL_INT(GZ_SUBJ_PUB_AGE_S, gz_cmd_pod_field(2, boards[b]));
    }
}

static void test_nav_pod_is_imu_or_channel(void)
{
    /* IMU boards: tilt. No-IMU board: Wi-Fi channel. */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_IMU,     gz_cmd_pod_comp(3, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_IMU,     gz_cmd_pod_comp(3, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_CHANNEL, gz_cmd_pod_comp(3, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_IMU_TILT, gz_cmd_pod_field(3, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_CHANNEL,  gz_cmd_pod_field(3, CAPS_ES3));
}

static void test_every_pod_is_sourceable_and_in_range(void)
{
    const uint8_t boards[] = { CAPS_185B, CAPS_206, CAPS_ES3 };
    for (unsigned b = 0; b < 3; ++b)
        for (int slot = 0; slot < GZ_FACE_SLOTS; ++slot) {
            gz_comp_id_t c = gz_cmd_pod_comp(slot, boards[b]);
            TEST_ASSERT_TRUE(gz_comp_available(c, boards[b]));  /* never faked */
        }
    /* An out-of-range slot is an empty pod, not a crash. */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, gz_cmd_pod_comp(4, CAPS_185B));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, gz_cmd_pod_comp(-1, CAPS_185B));
}

static void test_no_gps_or_bearing_field(void)
{
    /* The binding table maps only honest node fields — there is no direction or
     * location subject to bind. Walk every complication: each maps to a real
     * subject id or to NONE, and none is a bearing/GPS value (there is none). */
    for (int c = 0; c < GZ_COMP_COUNT; ++c) {
        gz_subj_field_t f = gz_comp_subject((gz_comp_id_t)c);
        TEST_ASSERT_TRUE(f >= GZ_SUBJ_NONE && f <= GZ_SUBJ_RECON_ON);
    }
}

/* ---- 2. round-safe pod placement ----------------------------------------- */
static void test_pods_inside_safe_area_all_boards(void)
{
    const int pod_r = 34, margin = 6;
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        for (int i = 0; i < 4; ++i) {
            int x, y;
            gz_cmd_pod_pos(i, bd.w, bd.h, bd.round, pod_r, margin, &x, &y);
            char msg[48];
            snprintf(msg, sizeof msg, "%s pod %d clips", bd.name, i);
            TEST_ASSERT_TRUE_MESSAGE(
                gz_cmd_pod_inside(x, y, pod_r, bd.w, bd.h, bd.round, margin), msg);
        }
    }
}

static void test_pod_layout_is_symmetric(void)
{
    /* The four pods mirror about the dial centre on every board. */
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        int x0, y0, x1, y1, x2, y2, x3, y3;
        gz_cmd_pod_pos(0, bd.w, bd.h, bd.round, 34, 6, &x0, &y0);
        gz_cmd_pod_pos(1, bd.w, bd.h, bd.round, 34, 6, &x1, &y1);
        gz_cmd_pod_pos(2, bd.w, bd.h, bd.round, 34, 6, &x2, &y2);
        gz_cmd_pod_pos(3, bd.w, bd.h, bd.round, 34, 6, &x3, &y3);
        int cx = bd.w / 2, cy = bd.h / 2;
        TEST_ASSERT_EQUAL_INT(cx - x0, x1 - cx);   /* left/right mirror */
        TEST_ASSERT_EQUAL_INT(cx - x2, x3 - cx);
        TEST_ASSERT_EQUAL_INT(cy - y0, y2 - cy);   /* top/bottom mirror */
        TEST_ASSERT_EQUAL_INT(y0, y1);             /* top pods share a row */
        TEST_ASSERT_EQUAL_INT(y2, y3);             /* bottom pods share a row */
    }
}

static void test_tiny_pod_still_fits(void)
{
    /* A pod larger than the safe half collapses to the centre, never negative
     * offset (no clip, no wrap). */
    int x, y;
    gz_cmd_pod_pos(0, 240, 320, false, 500, 6, &x, &y);
    TEST_ASSERT_EQUAL_INT(120, x);
    TEST_ASSERT_EQUAL_INT(160, y);
}

/* ---- 3. quadrant feature map --------------------------------------------- */
static void test_quadrant_maps_each_corner(void)
{
    const int cx = 180, cy = 180;
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_RADAR,  gz_cmd_quadrant(100, 100, cx, cy)); /* TL */
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_HUNT,   gz_cmd_quadrant(260, 100, cx, cy)); /* TR */
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_LAB,    gz_cmd_quadrant(100, 260, cx, cy)); /* BL */
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_STATUS, gz_cmd_quadrant(260, 260, cx, cy)); /* BR */
}

static void test_quadrant_centre_line_resolves_up_left(void)
{
    const int cx = 180, cy = 180;
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_RADAR, gz_cmd_quadrant(cx, cy, cx, cy));     /* dead centre */
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_RADAR, gz_cmd_quadrant(cx, 100, cx, cy));    /* on x axis, up */
    TEST_ASSERT_EQUAL_INT(GZ_CMD_FEAT_RADAR, gz_cmd_quadrant(100, cy, cx, cy));    /* on y axis, left */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixed_pods_bind_all_board_complications);
    RUN_TEST(test_nav_pod_is_imu_or_channel);
    RUN_TEST(test_every_pod_is_sourceable_and_in_range);
    RUN_TEST(test_no_gps_or_bearing_field);
    RUN_TEST(test_pods_inside_safe_area_all_boards);
    RUN_TEST(test_pod_layout_is_symmetric);
    RUN_TEST(test_tiny_pod_still_fits);
    RUN_TEST(test_quadrant_maps_each_corner);
    RUN_TEST(test_quadrant_centre_line_resolves_up_left);
    return UNITY_END();
}
