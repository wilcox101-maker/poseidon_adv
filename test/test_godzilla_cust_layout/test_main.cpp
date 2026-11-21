/* Native Unity tests for the customiser list geometry (godzilla_node/main/
 * gz_cust_layout.h), item M part 1. Proves, for all three boards'
 * GZ_LCD_WIDTH/HEIGHT/ROUND, that the customiser main page (10 rows) and the
 * Alerts subpage (8 rows, no scroll) lay out inside the shape-aware safe box:
 *   - the title (and the Alerts footer) sit inside the box, on their own lines;
 *   - no row overlaps the title band;
 *   - every row is at least the Montserrat-14 line height tall (so no font is
 *     shrunk and nothing is scrolled off — item M removed the dead scroll);
 *   - a tap on any drawn row maps back to that row, and a tap in the title /
 *     footer band or outside the box x-range maps to no row.
 *
 * Pure: gz_geom.h / gz_safearea.h maths only, no LVGL — the real font line
 * heights are passed in exactly as the firmware passes lv_font_get_line_height. */
#include <unity.h>
#include "../../godzilla_node/main/gz_cust_layout.h"
#include "../../godzilla_node/main/gz_geom.h"
#include "../../godzilla_node/main/gz_alerts_page.h"   /* GZ_ALERTS_ROW_COUNT */

void setUp(void) {}
void tearDown(void) {}

/* Customiser main-page row count (gz_face_customiser.cpp: ROW_COUNT). */
#define MAIN_ROWS 10

/* The real built-in Montserrat line heights the firmware lays rows by. Using
 * values no smaller than the real ones keeps "row >= line height" a true test. */
#define LH_TITLE 15   /* montserrat_12 */
#define LH_ROW   17   /* montserrat_14 */

struct board { const char *name; int w, h, round; };
static const struct board BOARDS[] = {
    { "ES3C28P", 240, 320, 0 },
    { "1.85B",   360, 360, 1 },
    { "AMOLED",  410, 502, 0 },
};
#define N_BOARDS ((int)(sizeof BOARDS / sizeof BOARDS[0]))

/* Rebuild a board's safe box and the two lists exactly as the firmware does:
 * GZ_UI_S(6) margin, GZ_UI_S(4) band gaps. */
static gz_rect_t board_box(const struct board *b)
{
    return gz_safe_box(b->w, b->h, b->round != 0, gz_geom_scale(6, b->w, b->h));
}
static int board_band(const struct board *b)
{
    return gz_cust_band_h(LH_TITLE, gz_geom_scale(4, b->w, b->h));
}

static void check_list(const char *tag, const struct board *b, gz_rect_t box,
                       gz_cust_list_t L, int n_rows, int footer_h)
{
    /* Title on its own line at the top of the box. */
    TEST_ASSERT_EQUAL_INT_MESSAGE(box.y, L.title_y, tag);
    /* No row overlaps the title band, and the title band is at least one line. */
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(box.y, L.rows_y, tag);
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(LH_TITLE, L.rows_y - box.y, tag);
    /* Every row at least the row font's line height. */
    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(LH_ROW, L.row_h, tag);
    /* All rows, and the footer band, stay inside the box bottom. */
    TEST_ASSERT_TRUE_MESSAGE(L.rows_y + L.row_h * n_rows <= box.y + box.h, tag);
    TEST_ASSERT_TRUE_MESSAGE(L.footer_y + footer_h <= box.y + box.h, tag);
    /* The whole box is inside the safe area, so everything in it is too. */
    TEST_ASSERT_TRUE_MESSAGE(
        gz_safe_rect_inside(box, b->w, b->h, b->round != 0,
                            gz_geom_scale(6, b->w, b->h)), tag);

    /* A tap on each row's centre maps back to that row. */
    for (int r = 0; r < n_rows; ++r) {
        int cy = gz_cust_row_center_y(&L, r);
        int cx = box.x + box.w / 2;
        TEST_ASSERT_EQUAL_INT_MESSAGE(r, gz_cust_row_at(&L, box, cx, cy, n_rows), tag);
        /* The centred text sits within its row. */
        int ty = gz_cust_row_text_y(&L, r, LH_ROW);
        TEST_ASSERT_TRUE_MESSAGE(ty >= L.rows_y + r * L.row_h, tag);
        TEST_ASSERT_TRUE_MESSAGE(ty + LH_ROW <= L.rows_y + (r + 1) * L.row_h, tag);
    }
    /* Taps in the title band, past the last row, and outside the box x-range hit
     * no row. */
    int cx = box.x + box.w / 2;
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, gz_cust_row_at(&L, box, cx, L.rows_y - 1, n_rows), tag);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1,
        gz_cust_row_at(&L, box, cx, L.rows_y + L.row_h * n_rows, n_rows), tag);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1,
        gz_cust_row_at(&L, box, box.x - 1, gz_cust_row_center_y(&L, 0), n_rows), tag);
}

static void test_main_page_all_boards(void)
{
    for (int i = 0; i < N_BOARDS; ++i) {
        const struct board *b = &BOARDS[i];
        gz_rect_t box = board_box(b);
        int th = board_band(b);
        gz_cust_list_t L = gz_cust_list(box, th, 0, MAIN_ROWS);   /* no footer */
        check_list(b->name, b, box, L, MAIN_ROWS, 0);
    }
}

static void test_alerts_page_all_boards(void)
{
    for (int i = 0; i < N_BOARDS; ++i) {
        const struct board *b = &BOARDS[i];
        gz_rect_t box = board_box(b);
        int th = board_band(b);                                  /* title + footer */
        gz_cust_list_t L = gz_cust_list(box, th, th, GZ_ALERTS_ROW_COUNT);
        check_list(b->name, b, box, L, GZ_ALERTS_ROW_COUNT, th);
        /* Footer sits below the last row, inside the box. */
        TEST_ASSERT_TRUE(L.footer_y >= L.rows_y + L.row_h * GZ_ALERTS_ROW_COUNT - 1);
    }
}

/* n_rows = 0 or a box too small never divides by zero or returns a bogus row. */
static void test_degenerate(void)
{
    gz_rect_t box = { 0, 0, 10, 10 };
    gz_cust_list_t L = gz_cust_list(box, 4, 0, 0);
    TEST_ASSERT_EQUAL_INT(0, L.row_h);
    TEST_ASSERT_EQUAL_INT(-1, gz_cust_row_at(&L, box, 5, 5, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_main_page_all_boards);
    RUN_TEST(test_alerts_page_all_boards);
    RUN_TEST(test_degenerate);
    return UNITY_END();
}
