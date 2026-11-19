/* Native Unity test for godzilla_node/main/gz_imu.h — the pure half: scaling,
 * tilt, orientation hysteresis and the rest/motion decision. The Wire I2C is
 * ARDUINO-only and not exercised here. */
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
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 512.0f, gz_imu_gyro_dps(32767, 512));
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accel_scaling);
    RUN_TEST(test_gyro_scaling);
    RUN_TEST(test_tilt_flat);
    RUN_TEST(test_tilt_on_x);
    RUN_TEST(test_rotation);
    RUN_TEST(test_motion);
    return UNITY_END();
}
