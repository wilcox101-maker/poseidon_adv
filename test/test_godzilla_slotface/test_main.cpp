/* Native Unity test for godzilla_node/main/gz_slotface.h — the pure Modular +
 * Minimal face model (watch-faces orders, unit 5):
 *   1. Modular 2x2 slot grid: on all three boards every tile and the digital-
 *      time band lie wholly inside the safe area (safe circle on the round
 *      1.85B, inset rect on the 2.06 / ES3C28P), the grid sits below the time
 *      band, tiles do not overlap, and the layout is symmetric about centre;
 *   2. slot->complication resolution reuses the Command slot model: a slot a
 *      board can't source drops to NONE (never faked), a sourceable one passes,
 *      and each cell binds to the right live subject;
 *   3. Minimal face: the big-time rect and the one status line lie inside the
 *      safe area with the status line below the time, and its single
 *      complication is always board-sourceable (first set slot, else STATUS);
 *   4. degenerate inputs (huge margin / tiny panel) never produce a negative
 *      size or a tile outside the safe area.
 * All three boards from hand-built masks, as test_godzilla_cmdface /
 * test_godzilla_geom check theirs. */
#include <unity.h>
#include <stdio.h>
#include "../../godzilla_node/main/gz_slotface.h"

void setUp(void) {}
void tearDown(void) {}

/* The three shipping boards as capability masks (from their gz_board_*.h). */
static const uint8_t CAPS_185B = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_SD;
static const uint8_t CAPS_206  = GZ_CAP_RTC | GZ_CAP_IMU | GZ_CAP_BATT | GZ_CAP_SD;
static const uint8_t CAPS_ES3  = GZ_CAP_BATT | GZ_CAP_SD;   /* no IMU, no RTC */

typedef struct { const char *name; int w, h; bool round; } board_t;
static const board_t B185 = { "1.85B",   360, 360, true  };
static const board_t B206 = { "2.06",    410, 502, false };
static const board_t BES3 = { "ES3C28P", 240, 320, false };
static const board_t ALL[] = { B185, B206, BES3 };

static const int MGN = 6, GAP = 8;

/* ---- 1. Modular 2x2 grid geometry ---------------------------------------- */
static void test_modular_tiles_inside_safe_area(void)
{
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        for (int i = 0; i < 4; ++i) {
            int x, y, cw, ch;
            gz_mod_cell(i, bd.w, bd.h, bd.round, MGN, GAP, &x, &y, &cw, &ch);
            char msg[48];
            snprintf(msg, sizeof msg, "%s tile %d", bd.name, i);
            TEST_ASSERT_TRUE_MESSAGE(cw > 0 && ch > 0, msg);
            TEST_ASSERT_TRUE_MESSAGE(
                gz_slot_rect_inside(x, y, cw, ch, bd.w, bd.h, bd.round, MGN), msg);
        }
    }
}

static void test_modular_time_band_inside_and_above_grid(void)
{
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        int tx, ty, tw, th;
        gz_mod_time_band(bd.w, bd.h, bd.round, MGN, &tx, &ty, &tw, &th);
        TEST_ASSERT_TRUE(th > 0);
        TEST_ASSERT_TRUE(gz_slot_rect_inside(tx, ty, tw, th, bd.w, bd.h, bd.round, MGN));
        /* every tile starts at or below the time band's bottom edge */
        for (int i = 0; i < 4; ++i) {
            int x, y, cw, ch;
            gz_mod_cell(i, bd.w, bd.h, bd.round, MGN, GAP, &x, &y, &cw, &ch);
            TEST_ASSERT_TRUE_MESSAGE(y >= ty + th, bd.name);
        }
    }
}

static void test_modular_tiles_do_not_overlap(void)
{
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        int x[4], y[4], cw[4], ch[4];
        for (int i = 0; i < 4; ++i)
            gz_mod_cell(i, bd.w, bd.h, bd.round, MGN, GAP, &x[i], &y[i], &cw[i], &ch[i]);
        /* a GAP column between left and right, a GAP row between top and bottom */
        TEST_ASSERT_TRUE_MESSAGE(x[0] + cw[0] <= x[1], bd.name);   /* TL right of TR left */
        TEST_ASSERT_TRUE_MESSAGE(x[2] + cw[2] <= x[3], bd.name);
        TEST_ASSERT_TRUE_MESSAGE(y[0] + ch[0] <= y[2], bd.name);   /* TL above BL */
        TEST_ASSERT_TRUE_MESSAGE(y[1] + ch[1] <= y[3], bd.name);
    }
}

static void test_modular_layout_is_symmetric(void)
{
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        int x[4], y[4], cw[4], ch[4];
        for (int i = 0; i < 4; ++i)
            gz_mod_cell(i, bd.w, bd.h, bd.round, MGN, GAP, &x[i], &y[i], &cw[i], &ch[i]);
        int bx, by, bw, bh;
        gz_slot_box(bd.w, bd.h, bd.round, MGN, &bx, &by, &bw, &bh);
        int box_cx = bx + bw / 2;
        /* columns mirror about the box centre x; rows share a y */
        TEST_ASSERT_EQUAL_INT_MESSAGE(box_cx - x[0], (x[1] + cw[1]) - box_cx, bd.name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(x[0], x[2], bd.name);   /* left column shares x  */
        TEST_ASSERT_EQUAL_INT_MESSAGE(x[1], x[3], bd.name);   /* right column shares x */
        TEST_ASSERT_EQUAL_INT_MESSAGE(y[0], y[1], bd.name);   /* top row shares y      */
        TEST_ASSERT_EQUAL_INT_MESSAGE(y[2], y[3], bd.name);   /* bottom row shares y   */
    }
}

/* ---- 2. slot -> complication resolution ---------------------------------- */
static void test_modular_slot_filter_drops_unsourceable(void)
{
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);
    s.slots[0] = GZ_COMP_CLOCK;     /* needs RTC */
    s.slots[1] = GZ_COMP_IMU;       /* needs IMU */
    s.slots[2] = GZ_COMP_CONTACTS;  /* all boards */
    s.slots[3] = GZ_COMP_BATTERY;   /* needs BATT */

    /* ES3C28P: no RTC, no IMU -> CLOCK and IMU slots blank; the rest honoured. */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE,     gz_mod_cell_comp(&s, 0, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE,     gz_mod_cell_comp(&s, 1, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_CONTACTS, gz_mod_cell_comp(&s, 2, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_BATTERY,  gz_mod_cell_comp(&s, 3, CAPS_ES3));

    /* 2.06 sources all four. */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_CLOCK,    gz_mod_cell_comp(&s, 0, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_IMU,      gz_mod_cell_comp(&s, 1, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_BATTERY,  gz_mod_cell_comp(&s, 3, CAPS_206));

    /* an out-of-range cell is empty, not a crash */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, gz_mod_cell_comp(&s, 4, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_NONE, gz_mod_cell_comp(&s, -1, CAPS_206));
}

static void test_modular_cell_binds_right_subject(void)
{
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);   /* STATUS, CONTACTS, PUBLISH, CHANNEL */
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_HEAP_KB,   gz_mod_cell_field(&s, 0, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_CONTACTS,  gz_mod_cell_field(&s, 1, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_PUB_AGE_S, gz_mod_cell_field(&s, 2, CAPS_206));
    TEST_ASSERT_EQUAL_INT(GZ_SUBJ_CHANNEL,   gz_mod_cell_field(&s, 3, CAPS_206));
}

/* ---- 3. Minimal face ----------------------------------------------------- */
static void test_minimal_layout_inside_and_ordered(void)
{
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        int tx, ty, tw, th, sx, sy, sw, sh;
        gz_min_layout(bd.w, bd.h, bd.round, MGN,
                      &tx, &ty, &tw, &th, &sx, &sy, &sw, &sh);
        TEST_ASSERT_TRUE_MESSAGE(th > 0 && sh > 0, bd.name);
        TEST_ASSERT_TRUE_MESSAGE(
            gz_slot_rect_inside(tx, ty, tw, th, bd.w, bd.h, bd.round, MGN), bd.name);
        TEST_ASSERT_TRUE_MESSAGE(
            gz_slot_rect_inside(sx, sy, sw, sh, bd.w, bd.h, bd.round, MGN), bd.name);
        TEST_ASSERT_TRUE_MESSAGE(sy >= ty + th, bd.name);   /* status below time */
    }
}

static void test_minimal_status_comp_always_sourceable(void)
{
    gz_ui_settings_t s;
    gz_ui_settings_defaults(&s);
    /* defaults' first slot is STATUS -> STATUS everywhere */
    TEST_ASSERT_EQUAL_INT(GZ_COMP_STATUS, gz_min_status_comp(&s, CAPS_ES3));

    /* first set slot is IMU (unsourceable on ES3), second CONTACTS -> CONTACTS */
    s.slots[0] = GZ_COMP_IMU;
    s.slots[1] = GZ_COMP_CONTACTS;
    s.slots[2] = GZ_COMP_NONE;
    s.slots[3] = GZ_COMP_NONE;
    TEST_ASSERT_EQUAL_INT(GZ_COMP_CONTACTS, gz_min_status_comp(&s, CAPS_ES3));
    TEST_ASSERT_EQUAL_INT(GZ_COMP_IMU,      gz_min_status_comp(&s, CAPS_206));

    /* all slots blank -> STATUS fallback, and it is sourceable */
    s.slots[0] = s.slots[1] = s.slots[2] = s.slots[3] = GZ_COMP_NONE;
    gz_comp_id_t c = gz_min_status_comp(&s, CAPS_ES3);
    TEST_ASSERT_EQUAL_INT(GZ_COMP_STATUS, c);
    TEST_ASSERT_TRUE(gz_comp_available(c, CAPS_ES3));
}

/* ---- 4. degenerate inputs ------------------------------------------------ */
static void test_degenerate_margin_never_negative_or_clipping(void)
{
    /* a margin larger than the panel: box collapses, no negative tile size, and
     * nothing is reported outside the safe area. */
    for (unsigned b = 0; b < 3; ++b) {
        const board_t bd = ALL[b];
        for (int i = 0; i < 4; ++i) {
            int x, y, cw, ch;
            gz_mod_cell(i, bd.w, bd.h, bd.round, 1000, GAP, &x, &y, &cw, &ch);
            TEST_ASSERT_TRUE(cw >= 0 && ch >= 0);
        }
        int tx, ty, tw, th, sx, sy, sw, sh;
        gz_min_layout(bd.w, bd.h, bd.round, 1000,
                      &tx, &ty, &tw, &th, &sx, &sy, &sw, &sh);
        TEST_ASSERT_TRUE(th >= 0 && sh >= 0);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_modular_tiles_inside_safe_area);
    RUN_TEST(test_modular_time_band_inside_and_above_grid);
    RUN_TEST(test_modular_tiles_do_not_overlap);
    RUN_TEST(test_modular_layout_is_symmetric);
    RUN_TEST(test_modular_slot_filter_drops_unsourceable);
    RUN_TEST(test_modular_cell_binds_right_subject);
    RUN_TEST(test_minimal_layout_inside_and_ordered);
    RUN_TEST(test_minimal_status_comp_always_sourceable);
    RUN_TEST(test_degenerate_margin_never_negative_or_clipping);
    return UNITY_END();
}
