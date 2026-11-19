/* Native Unity test for godzilla_node/main/gz_imu.h — the pure half: scaling,
 * tilt, orientation with hysteresis, and the rest/motion decision (tests 1-6
 * carried over from the closed #85). Built as the IMU-less ES3C28P on purpose:
 * the pure half must not need an IMU address, and the Wire half (ARDUINO and
 * GZ_BOARD_HAS_IMU only) is on-target. */
#define GZ_BOARD_ES3C28P 1
#include <unity.h>
#include <math.h>
#include "../../godzilla_node/main/gz_imu.h"

void setUp(void) {}
void tearDown(void) {}

/* Full-scale maps to the range; zero is zero; sign is preserved. */
static void test_accel_scaling(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, gz_imu_accel_g(0, 8));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, gz_imu_accel_g(32767, 8));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -8.0f, gz_imu_accel_g(-32768, 8));
    /* half-scale at ±8g ~ 4g; 1g ~ raw 4096 */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, gz_imu_accel_g(16384, 8));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, gz_imu_accel_g(4096, 8));
}

static void test_gyro_scaling(void)
{
    /* +32767 is one LSB short of full scale (512 * 32767/32768 = 511.984);
     * -32768 is exactly -512. 1 LSB here is 0.0156 dps, so test tighter than
     * that against the exact value rather than against 512 (#85's test was
     * off by one LSB and failed). */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 512.0f * 32767.0f / 32768.0f, gz_imu_gyro_dps(32767, 512));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -512.0f, gz_imu_gyro_dps(-32768, 512));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -256.0f, gz_imu_gyro_dps(-16384, 512));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, gz_imu_gyro_dps(0, 512));
}

/* Flat and face-up: magnitude 1 g, tilt ~0. */
static void test_tilt_flat(void)
{
    float pitch, roll;
    gz_imu_tilt_deg(0.0f, 0.0f, 1.0f, &pitch, &roll);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, pitch);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, roll);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, gz_imu_accel_mag_g(0, 0, 1));
}

/* Tipped fully onto the X axis: pitch ~ -90 deg. */
static void test_tilt_on_x(void)
{
    float pitch, roll;
    gz_imu_tilt_deg(1.0f, 0.0f, 0.0f, &pitch, &roll);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -90.0f, pitch);
}

/* Orientation picks the dominant in-plane axis, holds in the flat dead-zone,
 * and is hysteretic (returns `current` when ambiguous). */
static void test_rotation(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, gz_imu_rotation(0.0f,  1.0f, 2));  /* +Y up   */
    TEST_ASSERT_EQUAL_UINT8(2, gz_imu_rotation(0.0f, -1.0f, 0));  /* -Y up   */
    TEST_ASSERT_EQUAL_UINT8(1, gz_imu_rotation(1.0f,  0.0f, 0));  /* +X up   */
    TEST_ASSERT_EQUAL_UINT8(3, gz_imu_rotation(-1.0f, 0.0f, 0));  /* -X up   */
    /* near-flat: keep whatever we had */
    TEST_ASSERT_EQUAL_UINT8(2, gz_imu_rotation(0.1f, 0.1f, 2));
    TEST_ASSERT_EQUAL_UINT8(1, gz_imu_rotation(0.1f, 0.1f, 1));
}

/* Rest at 1 g with no rotation is still; a shove or a spin is motion. */
static void test_motion(void)
{
    TEST_ASSERT_FALSE(gz_imu_is_moving(0, 0, 1.0f, 0, 0, 0, 0.08f, 12.0f));
    TEST_ASSERT_FALSE(gz_imu_is_moving(0, 0, 1.02f, 1.0f, 0, 0, 0.08f, 12.0f));
    TEST_ASSERT_TRUE(gz_imu_is_moving(0, 0, 1.5f, 0, 0, 0, 0.08f, 12.0f));   /* 0.5g shove */
    TEST_ASSERT_TRUE(gz_imu_is_moving(0, 0, 1.0f, 0, 40.0f, 0, 0.08f, 12.0f)); /* spin */
}

/* Hysteresis (#88 §3.3 rebuild): a diagonal hold must not flicker, a clear
 * turn must switch, and a flip to the opposite edge must switch. */
static void test_rotation_hysteresis(void)
{
    TEST_ASSERT_EQUAL_INT(0, GZ_BOARD_HAS_IMU);              /* built as ES3C28P */
    /* Near-diagonal, +Y a hair stronger: stay on +X (1). */
    TEST_ASSERT_EQUAL_UINT8(1, gz_imu_rotation(0.70f, 0.72f, 1));
    /* ...and from +Y (0), the same pose stays +Y. */
    TEST_ASSERT_EQUAL_UINT8(0, gz_imu_rotation(0.72f, 0.70f, 0));
    /* Clear turn from +X to +Y. */
    TEST_ASSERT_EQUAL_UINT8(0, gz_imu_rotation(0.10f, 0.90f, 1));
    /* Flip to the opposite edge. */
    TEST_ASSERT_EQUAL_UINT8(2, gz_imu_rotation(0.0f, -1.0f, 0));
    TEST_ASSERT_EQUAL_UINT8(3, gz_imu_rotation(-1.0f, 0.0f, 1));
    /* Just past the margin switches; just under holds. */
    TEST_ASSERT_EQUAL_UINT8(0, gz_imu_rotation(0.60f, 0.81f, 1));    /* lead 0.21 */
    TEST_ASSERT_EQUAL_UINT8(1, gz_imu_rotation(0.60f, 0.79f, 1));    /* lead 0.19 */
    /* A stale `current` above 3 is folded to 0..3 when a side is chosen. */
    TEST_ASSERT_EQUAL_UINT8(2, gz_imu_rotation(0.0f, 0.3f, 6));       /* too flat: 6&3 */
    TEST_ASSERT_EQUAL_UINT8(1, gz_imu_rotation(0.9f, 0.0f, 5));       /* 5&3 = 1 */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accel_scaling);
    RUN_TEST(test_gyro_scaling);
    RUN_TEST(test_tilt_flat);
    RUN_TEST(test_tilt_on_x);
    RUN_TEST(test_rotation);
    RUN_TEST(test_motion);
    RUN_TEST(test_rotation_hysteresis);
    return UNITY_END();
}
