/* Native Unity test for godzilla_node/main/gz_faceclock.h — the digital-time
 * and 2x2 slot-grid helpers the Modular and Minimal faces share
 * (watch-faces orders, unit 5):
 *   1. HH:MM formatting (zero-padded 24-hour, clamped), the 12-hour split, and
 *      the once-a-second colon blink;
 *   2. the 2x2 slot grid: four equal cells tile a box, stay inside it, don't
 *      overlap, and mirror about the centre.
 * Pure host TU, same style as test_godzilla_faces / test_godzilla_customiser. */
#include <unity.h>
#include <string.h>
#include "../../godzilla_node/main/gz_faceclock.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- 1. digital time ----------------------------------------------------- */
static void test_hhmm_format(void)
{
    char b[6];
    gz_fmt_hhmm(0, 0, b);   TEST_ASSERT_EQUAL_STRING("00:00", b);
    gz_fmt_hhmm(9, 5, b);   TEST_ASSERT_EQUAL_STRING("09:05", b);
    gz_fmt_hhmm(23, 59, b); TEST_ASSERT_EQUAL_STRING("23:59", b);
    gz_fmt_hhmm(13, 7, b);  TEST_ASSERT_EQUAL_STRING("13:07", b);
    /* out-of-range clamps rather than overruns */
    gz_fmt_hhmm(25, 70, b); TEST_ASSERT_EQUAL_STRING("23:59", b);
}

static void test_hour12_split(void)
{
    bool pm;
    TEST_ASSERT_EQUAL_UINT8(12, gz_hour12(0, &pm));  TEST_ASSERT_FALSE(pm); /* 12 am */
    TEST_ASSERT_EQUAL_UINT8(11, gz_hour12(11, &pm)); TEST_ASSERT_FALSE(pm);
    TEST_ASSERT_EQUAL_UINT8(12, gz_hour12(12, &pm)); TEST_ASSERT_TRUE(pm);  /* 12 pm */
    TEST_ASSERT_EQUAL_UINT8(1,  gz_hour12(13, &pm)); TEST_ASSERT_TRUE(pm);
    TEST_ASSERT_EQUAL_UINT8(11, gz_hour12(23, &pm)); TEST_ASSERT_TRUE(pm);
    /* NULL pm is allowed */
    TEST_ASSERT_EQUAL_UINT8(1, gz_hour12(1, NULL));
}

static void test_colon_blink(void)
{
    TEST_ASSERT_TRUE (gz_colon_on(0));
    TEST_ASSERT_FALSE(gz_colon_on(1));
    TEST_ASSERT_TRUE (gz_colon_on(58));
    TEST_ASSERT_FALSE(gz_colon_on(59));
}

/* ---- 2. 2x2 slot grid ---------------------------------------------------- */
static void test_grid_cells_inside_and_disjoint(void)
{
    gz_rect_t box = { 20, 40, 200, 160 };
    const int gap = 8;
    gz_rect_t c[4];
    for (int i = 0; i < 4; ++i) gz_grid2x2_cell(box, gap, i, &c[i]);

    for (int i = 0; i < 4; ++i) {
        /* each cell lies wholly inside the box */
        TEST_ASSERT_TRUE(c[i].x >= box.x && c[i].y >= box.y);
        TEST_ASSERT_TRUE(c[i].x + c[i].w <= box.x + box.w);
        TEST_ASSERT_TRUE(c[i].y + c[i].h <= box.y + box.h);
        TEST_ASSERT_TRUE(c[i].w > 0 && c[i].h > 0);
    }
    /* equal cell sizes */
    for (int i = 1; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT(c[0].w, c[i].w);
        TEST_ASSERT_EQUAL_INT(c[0].h, c[i].h);
    }
    /* the gap between columns and rows is exactly `gap` */
    TEST_ASSERT_EQUAL_INT(gap, c[1].x - (c[0].x + c[0].w));
    TEST_ASSERT_EQUAL_INT(gap, c[2].y - (c[0].y + c[0].h));
    /* top/bottom and left/right pairs share rows/columns */
    TEST_ASSERT_EQUAL_INT(c[0].y, c[1].y);
    TEST_ASSERT_EQUAL_INT(c[2].y, c[3].y);
    TEST_ASSERT_EQUAL_INT(c[0].x, c[2].x);
    TEST_ASSERT_EQUAL_INT(c[1].x, c[3].x);
}

static void test_grid_degenerate_box(void)
{
    /* a box narrower than the gap yields zero-width cells, never negative. */
    gz_rect_t box = { 0, 0, 4, 4 };
    gz_rect_t cell;
    gz_grid2x2_cell(box, 8, 0, &cell);
    TEST_ASSERT_TRUE(cell.w >= 0 && cell.h >= 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hhmm_format);
    RUN_TEST(test_hour12_split);
    RUN_TEST(test_colon_blink);
    RUN_TEST(test_grid_cells_inside_and_disjoint);
    RUN_TEST(test_grid_degenerate_box);
    return UNITY_END();
}
