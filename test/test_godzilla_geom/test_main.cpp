/* Native Unity test for godzilla_node/main/gz_geom.h — the single UI geometry
 * source (#88 §3.2). Two jobs:
 *   1. on the 1.85B (360x360 round) every function returns exactly the
 *      constant it replaced, so nothing moves on the reference board;
 *   2. on all three boards the layouts fit the panel (the dial inscribes in the
 *      short side, the card and plot fit the width, the watch bow stays on
 *      screen, Hunt's last line stays above the bottom edge). */
#include <unity.h>
#include "../../godzilla_node/main/gz_geom.h"

void setUp(void) {}
void tearDown(void) {}

typedef struct { const char *name; int w, h; bool round; } board_t;
static const board_t B185  = { "1.85B",   360, 360, true  };
static const board_t B206  = { "2.06",    410, 502, false };
static const board_t BES3  = { "ES3C28P", 240, 320, false };
static const board_t ALL[] = { B185, B206, BES3 };

/* Watch-face design constants (gz_ui_watch.cpp, 360 canvas). */
#define D_R_CASE 150
#define D_WCY_DY 8
#define D_BOW_ABOVE 21   /* bow centre sits 21 px above the case top */
#define D_BOW_R 13

static void test_185b_is_unchanged(void)
{
    const int w = B185.w, h = B185.h;
    TEST_ASSERT_EQUAL_INT(180, gz_geom_safe_r(w, h));
    TEST_ASSERT_EQUAL_INT(158, gz_geom_radar_r_max(w, h));       /* was R_MAX 158 */
    TEST_ASSERT_EQUAL_INT(236, gz_geom_card_w(w));                 /* card 236 */
    TEST_ASSERT_EQUAL_INT(240, gz_geom_hunt_plot_w(w, true));      /* PLOT_W 240 */
    TEST_ASSERT_EQUAL_INT(60,  gz_geom_hunt_plot_x(w, true));      /* PLOT_X 60 */
    TEST_ASSERT_EQUAL_INT(250, gz_geom_col_w(250, w));             /* status column */
    TEST_ASSERT_EQUAL_INT(260, gz_geom_col_w(260, w));             /* Hunt labels */
    TEST_ASSERT_EQUAL_INT(0,   gz_geom_content_dy(h));
    /* The watch's radial constants scale by exactly 1. */
    const int watch[] = { 150, 7, 139, 135, 129, 123, 103, 62, 96, 112, 14, 24, 8, 44, 46 };
    for (unsigned i = 0; i < sizeof watch / sizeof watch[0]; ++i)
        TEST_ASSERT_EQUAL_INT(watch[i], gz_geom_scale(watch[i], w, h));
}

static void test_scale_values(void)
{
    TEST_ASSERT_EQUAL_INT(205, gz_geom_safe_r(B206.w, B206.h));
    TEST_ASSERT_EQUAL_INT(120, gz_geom_safe_r(BES3.w, BES3.h));
    TEST_ASSERT_EQUAL_INT(171, gz_geom_scale(150, B206.w, B206.h));   /* 150*205/180 = 170.8 */
    TEST_ASSERT_EQUAL_INT(100, gz_geom_scale(150, BES3.w, BES3.h));
    TEST_ASSERT_EQUAL_INT(5,   gz_geom_scale(8, BES3.w, BES3.h));      /* 5.33 -> 5 */
    TEST_ASSERT_EQUAL_INT(71,  gz_geom_content_dy(B206.h));
    TEST_ASSERT_EQUAL_INT(-20, gz_geom_content_dy(BES3.h));
    TEST_ASSERT_EQUAL_INT(232, gz_geom_card_w(BES3.w));                /* 236 clamped */
    TEST_ASSERT_EQUAL_INT(378, gz_geom_hunt_plot_w(B206.w, false));
    TEST_ASSERT_EQUAL_INT(208, gz_geom_hunt_plot_w(BES3.w, false));
    TEST_ASSERT_EQUAL_INT(16,  gz_geom_hunt_plot_x(BES3.w, false));
    TEST_ASSERT_EQUAL_INT(224, gz_geom_col_w(250, BES3.w));
}

static void test_layouts_fit_every_board(void)
{
    for (unsigned i = 0; i < sizeof ALL / sizeof ALL[0]; ++i) {
        const board_t *b = &ALL[i];
        const int cx = b->w / 2, cy = b->h / 2;

        /* Radar dial inscribes in the short side, with the hub clear. */
        int rmax = gz_geom_radar_r_max(b->w, b->h);
        TEST_ASSERT_TRUE_MESSAGE(rmax > GZ_GEOM_RADAR_R_MIN + 40, b->name);
        TEST_ASSERT_TRUE_MESSAGE(cx - rmax >= 0 && cx + rmax < b->w, b->name);
        TEST_ASSERT_TRUE_MESSAGE(cy - rmax >= 0 && cy + rmax < b->h, b->name);

        /* Card and plot fit the width. */
        TEST_ASSERT_TRUE_MESSAGE(gz_geom_card_w(b->w) <= b->w - 8, b->name);   /* 4 px a side */
        int pw = gz_geom_hunt_plot_w(b->w, b->round), px = gz_geom_hunt_plot_x(b->w, b->round);
        TEST_ASSERT_TRUE_MESSAGE(px >= 0 && px + pw <= b->w, b->name);
        TEST_ASSERT_TRUE_MESSAGE(gz_geom_col_w(260, b->w) <= b->w - 16, b->name);

        /* Watch: case on screen, bow above it still on screen. */
        int wcy  = cy + gz_geom_scale(D_WCY_DY, b->w, b->h);
        int rc   = gz_geom_scale(D_R_CASE, b->w, b->h);
        int bowy = wcy - rc - D_BOW_ABOVE;
        TEST_ASSERT_TRUE_MESSAGE(bowy - D_BOW_R >= 0, b->name);
        TEST_ASSERT_TRUE_MESSAGE(wcy + rc < b->h, b->name);
        TEST_ASSERT_TRUE_MESSAGE(cx - rc >= 0 && cx + rc < b->w, b->name);

        /* Hunt: first label (design y 52) and last line (design y 192+76+28+16)
         * stay on the panel once shifted. */
        int dy = gz_geom_content_dy(b->h);
        TEST_ASSERT_TRUE_MESSAGE(52 + dy >= 0, b->name);
        TEST_ASSERT_TRUE_MESSAGE(192 + 76 + 28 + 16 + dy <= b->h, b->name);
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_185b_is_unchanged);
    RUN_TEST(test_scale_values);
    RUN_TEST(test_layouts_fit_every_board);
    return UNITY_END();
}
