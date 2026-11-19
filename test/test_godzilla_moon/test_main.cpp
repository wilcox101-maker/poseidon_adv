/* Native Unity test for godzilla_node/main/gz_moon.h — the pure moon-phase and
 * printed sub-scale math of the Classic face (watch-faces orders, unit 2):
 *   1. moon phase: known new moons fall in bucket 0 and known full moons in
 *      bucket 4, the illuminated fraction runs 0 (new) .. 1 (full), the waxing
 *      sense flips at full, and the age stays in [0, synodic);
 *   2. sub-scale index math: the month, day-of-week and date rings put their
 *      first division at 12 o'clock, step by an even 360/N, and wrap cleanly —
 *      so a printed numeral and the pointer that reads it share one convention.
 * Pure host TU, same style as test_godzilla_faces / test_godzilla_geom. */
#include <unity.h>
#include <math.h>
#include "../../godzilla_node/main/gz_moon.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- 1. moon phase ------------------------------------------------------- */
static void test_known_new_moons_are_bucket_0(void)
{
    /* Astronomical new moons: the reference epoch, and three later ones. */
    TEST_ASSERT_EQUAL_INT(0, gz_moon_bucket(2000, 1, 6));
    TEST_ASSERT_EQUAL_INT(0, gz_moon_bucket(2024, 1, 11));
    TEST_ASSERT_EQUAL_INT(0, gz_moon_bucket(2025, 1, 29));
    TEST_ASSERT_EQUAL_INT(0, gz_moon_bucket(2024, 4, 8));   /* total-eclipse new moon */
}

static void test_known_full_moons_are_bucket_4(void)
{
    TEST_ASSERT_EQUAL_INT(4, gz_moon_bucket(2024, 1, 25));
    TEST_ASSERT_EQUAL_INT(4, gz_moon_bucket(2025, 1, 13));
    TEST_ASSERT_EQUAL_INT(4, gz_moon_bucket(2025, 7, 10));
}

static void test_bucket_always_in_range(void)
{
    /* Walk a whole lunation day by day from a new moon: every bucket is 0..7
     * and the sequence is new(0) -> ... -> full(4) -> ... -> back to new. */
    int seen_full = 0, seen_new_after = 0;
    for (int d = 0; d < 30; ++d) {
        double age = fmod((double)d, GZ_MOON_SYNODIC);
        int b = gz_moon_bucket_age(age);
        TEST_ASSERT_TRUE(b >= 0 && b < GZ_MOON_BUCKETS);
        if (b == 4) seen_full = 1;
        if (seen_full && b == 0) seen_new_after = 1;
    }
    TEST_ASSERT_TRUE(seen_full);
    TEST_ASSERT_TRUE(seen_new_after);
}

static void test_illumination_endpoints(void)
{
    /* New ~0, full ~1, quarters ~0.5. */
    TEST_ASSERT_FLOAT_WITHIN(0.02, 0.0, gz_moon_illum_age(0.0));
    TEST_ASSERT_FLOAT_WITHIN(0.02, 1.0, gz_moon_illum_age(GZ_MOON_SYNODIC / 2.0));
    TEST_ASSERT_FLOAT_WITHIN(0.02, 0.5, gz_moon_illum_age(GZ_MOON_SYNODIC / 4.0));
    TEST_ASSERT_FLOAT_WITHIN(0.02, 0.5, gz_moon_illum_age(3.0 * GZ_MOON_SYNODIC / 4.0));
}

static void test_waxing_sense_flips_at_full(void)
{
    TEST_ASSERT_TRUE (gz_moon_waxing_age(1.0));                         /* young crescent */
    TEST_ASSERT_TRUE (gz_moon_waxing_age(GZ_MOON_SYNODIC / 2.0 - 0.5)); /* just before full */
    TEST_ASSERT_FALSE(gz_moon_waxing_age(GZ_MOON_SYNODIC / 2.0 + 0.5)); /* just after full */
    TEST_ASSERT_FALSE(gz_moon_waxing_age(GZ_MOON_SYNODIC - 0.5));       /* old crescent */
}

static void test_age_in_range_and_aperture_frac(void)
{
    for (int y = 2024; y <= 2027; ++y)
        for (int m = 1; m <= 12; ++m)
            for (int d = 1; d <= 28; d += 3) {
                double age = gz_moon_age(y, m, d);
                TEST_ASSERT_TRUE(age >= 0.0 && age < GZ_MOON_SYNODIC);
                double f = gz_moon_aperture_frac(age);
                TEST_ASSERT_TRUE(f >= 0.0 && f < 1.0);
            }
}

/* ---- 2. sub-scale index math --------------------------------------------- */
static void test_ring_first_division_at_top(void)
{
    /* Division 0 of any ring sits at 12 o'clock (0 tenths). */
    TEST_ASSERT_EQUAL_INT32(0, gz_ring_angle_tenths(0, 12));
    TEST_ASSERT_EQUAL_INT32(0, gz_month_angle_tenths(1));     /* January */
    TEST_ASSERT_EQUAL_INT32(0, gz_weekday_angle_tenths(0));   /* Sunday  */
    TEST_ASSERT_EQUAL_INT32(0, gz_date_angle_tenths(1, 31));  /* the 1st */
}

static void test_ring_even_steps_and_wrap(void)
{
    /* Twelve-division ring steps by 300 tenths (30 deg); the step before wrap
     * is 11/12, and index == count wraps back to the top. */
    TEST_ASSERT_EQUAL_INT32(300,  gz_ring_angle_tenths(1, 12));
    TEST_ASSERT_EQUAL_INT32(1800, gz_ring_angle_tenths(6, 12));   /* 6 o'clock */
    TEST_ASSERT_EQUAL_INT32(3300, gz_ring_angle_tenths(11, 12));
    TEST_ASSERT_EQUAL_INT32(0,    gz_ring_angle_tenths(12, 12));  /* wrap */
    TEST_ASSERT_EQUAL_INT32(300,  gz_ring_angle_tenths(13, 12));
    /* A negative index wraps the same way (no crash, no out-of-range angle). */
    TEST_ASSERT_EQUAL_INT32(3300, gz_ring_angle_tenths(-1, 12));
}

static void test_month_weekday_date_rings(void)
{
    /* December is the 12th of twelve -> 11/12 of a turn. */
    TEST_ASSERT_EQUAL_INT32(3300, gz_month_angle_tenths(12));
    /* Saturday is 6 of seven; 6*3600/7 = 3085 tenths (integer division). */
    TEST_ASSERT_EQUAL_INT32((int32_t)(6L * 3600L / 7L), gz_weekday_angle_tenths(6));
    /* The last day of a 30-day month is 29/30 of a turn; a 31-day month differs,
     * so the ring really uses the month's own length. */
    TEST_ASSERT_EQUAL_INT32((int32_t)(29L * 3600L / 30L), gz_date_angle_tenths(30, 30));
    TEST_ASSERT_EQUAL_INT32((int32_t)(30L * 3600L / 31L), gz_date_angle_tenths(31, 31));
    TEST_ASSERT_TRUE(gz_date_angle_tenths(15, 30) != gz_date_angle_tenths(15, 31));
    /* Guard: a zero/negative division count never divides by zero. */
    TEST_ASSERT_EQUAL_INT32(0, gz_ring_angle_tenths(3, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_new_moons_are_bucket_0);
    RUN_TEST(test_known_full_moons_are_bucket_4);
    RUN_TEST(test_bucket_always_in_range);
    RUN_TEST(test_illumination_endpoints);
    RUN_TEST(test_waxing_sense_flips_at_full);
    RUN_TEST(test_age_in_range_and_aperture_frac);
    RUN_TEST(test_ring_first_division_at_top);
    RUN_TEST(test_ring_even_steps_and_wrap);
    RUN_TEST(test_month_weekday_date_rings);
    return UNITY_END();
}
