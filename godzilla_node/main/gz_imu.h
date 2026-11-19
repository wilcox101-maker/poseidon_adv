/*
 * gz_imu.h — QMI8658 6-axis IMU, read-only, BOARD-AWARE (#88 §3.3).
 *
 * The 1.85B and the AMOLED-2.06 carry a QMI8658 accel+gyro at 0x6B on their
 * shared I2C bus; the ES3C28P has none. The Wire half exists only where the
 * board says GZ_BOARD_HAS_IMU 1 — on other boards there is no IMU address
 * macro and nothing to call, so no motion or orientation is ever fabricated.
 * The part only reports (nothing is commanded to move). Used for a
 * "still / moving" + orientation line on the status screen and, later,
 * auto-rotate / wake (#88 §3.7).
 *
 * Rebuilt from the closed #85 (Claudius), keeping its register map and tested
 * scaling / tilt / motion math, with two fixes: the driver state is shared
 * across translation units (C++17 inline variables — #85's file-static copy
 * left the status screen reading 0 forever), and the orientation now has real
 * hysteresis (a switching margin) instead of an entry threshold alone.
 *
 * Split, same as gz_rtc.h / gz_touch.h: the scaling and geometry are pure
 * (stdint/math, C and C++, host-tested by test/test_godzilla_imu); the Wire
 * I2C transactions are ARDUINO-only, behind #ifdef.
 *
 * Register map and the WHO_AM_I value (0x05) are from the QMI8658A datasheet
 * (QMI8658A_Datasheet_Rev_A, §4 Register Map; the CTRL1/2/3/7 bring-up order
 * matches the public ESPHome and NuttX drivers). The boot WHO_AM_I read is the
 * hardware proof: a wrong bus or address does not return 0x05.
 *   https://files.waveshare.com/upload/5/5f/QMI8658A_Datasheet_Rev_A.pdf
 *
 * Passive w.r.t. the project RF rules: on-board I2C to a soldered sensor, no
 * radio.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "gz_board.h"   /* GZ_BOARD_HAS_IMU, GZ_I2C_ADDR_IMU */

/* ---- register map (QMI8658A datasheet Rev A) ---- */
#define GZ_IMU_REG_WHOAMI     0x00   /* -> 0x05                              */
#define GZ_IMU_REG_REVISION   0x01
#define GZ_IMU_REG_CTRL1      0x02   /* serial IF: addr auto-increment       */
#define GZ_IMU_REG_CTRL2      0x03   /* accel: aFS[6:4] range, aODR[3:0]     */
#define GZ_IMU_REG_CTRL3      0x04   /* gyro:  gFS[6:4] range, gODR[3:0]     */
#define GZ_IMU_REG_CTRL5      0x06   /* low-pass filters                     */
#define GZ_IMU_REG_CTRL7      0x08   /* aEN bit0, gEN bit1                   */
#define GZ_IMU_REG_STATUS0    0x2E   /* bit0 aDA (accel data ready)          */
#define GZ_IMU_REG_TEMP_L     0x33
#define GZ_IMU_REG_AX_L       0x35   /* ax,ay,az,gx,gy,gz: 12 bytes LE int16 */
#define GZ_IMU_REG_RESET      0x60   /* write 0xB0 -> soft reset             */

#define GZ_IMU_WHOAMI_VALUE   0x05
#define GZ_IMU_RESET_CMD      0xB0
#define GZ_IMU_CTRL1_AUTOINC  0x40   /* ADDR_AI: register address auto-inc   */
#define GZ_IMU_CTRL7_ACC_EN   0x01
#define GZ_IMU_CTRL7_GYR_EN   0x02

/* Defaults picked for a UI tilt/motion sensor, not for capture: wide ranges,
 * modest ODR. aFS = ±8 g (0b011 << 4 = 0x30), gFS = ±512 dps (0b100 << 4),
 * ODR nibble 0x04 (~250 Hz accel / ~250 Hz gyro — ample for a 4 Hz UI). */
#define GZ_IMU_ACC_RANGE_G    8
#define GZ_IMU_GYR_RANGE_DPS  512
#define GZ_IMU_CTRL2_VALUE    0x34   /* aFS ±8g | aODR ~250 Hz               */
#define GZ_IMU_CTRL3_VALUE    0x44   /* gFS ±512dps | gODR ~250 Hz           */

/* ---- scaling (pure) ------------------------------------------------------ */

/* Full-scale int16 maps to ±range, so 1 LSB = range / 32768. */
static inline float gz_imu_accel_g(int16_t raw, uint16_t range_g)
{
    return (float)raw * (float)range_g / 32768.0f;
}

static inline float gz_imu_gyro_dps(int16_t raw, uint16_t range_dps)
{
    return (float)raw * (float)range_dps / 32768.0f;
}

/* ---- geometry (pure) ----------------------------------------------------- */

/* Magnitude of the acceleration vector in g. At rest this is ~1.0; the
 * departure from 1 g is linear acceleration (motion). */
static inline float gz_imu_accel_mag_g(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

/* Tilt from the gravity vector, degrees. pitch = nose up/down about X,
 * roll = left/right about Y. Valid when the device is roughly at rest. */
static inline void gz_imu_tilt_deg(float ax, float ay, float az,
                                   float *pitch, float *roll)
{
    *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2957795f;
    *roll  = atan2f(ay, az) * 57.2957795f;
}

#define GZ_IMU_ROT_ENTER_G   0.55f   /* in-plane gravity needed to pick a side   */
#define GZ_IMU_ROT_MARGIN_G  0.20f   /* lead the new side needs over the current */

/* In-plane gravity toward the edge `rot` names (+Y, +X, -Y, -X for 0..3). */
static inline float gz_imu_rot_axis_g(float ax_g, float ay_g, uint8_t rot)
{
    switch (rot & 3u) {
    case 0:  return  ay_g;
    case 1:  return  ax_g;
    case 2:  return -ay_g;
    default: return -ax_g;
    }
}

/* Which screen edge is up, as a quarter-turn 0/1/2/3 (0 = normal, 1 = rotated
 * 90 deg clockwise, ...), from the dominant in-plane gravity axis. Hysteresis:
 * a near-flat device holds `current`, and a new side must beat the current
 * side's gravity by GZ_IMU_ROT_MARGIN_G, so holding the node on a diagonal
 * does not flicker between two orientations. */
static inline uint8_t gz_imu_rotation(float ax_g, float ay_g, uint8_t current)
{
    float axm = fabsf(ax_g), aym = fabsf(ay_g);
    if (axm < GZ_IMU_ROT_ENTER_G && aym < GZ_IMU_ROT_ENTER_G) return (uint8_t)(current & 3u);   /* too flat: hold */
    uint8_t cand = (aym >= axm) ? (ay_g > 0 ? 0 : 2) : (ax_g > 0 ? 1 : 3);
    if (cand == (current & 3u)) return cand;
    float lead = gz_imu_rot_axis_g(ax_g, ay_g, cand) - gz_imu_rot_axis_g(ax_g, ay_g, current);
    return (lead > GZ_IMU_ROT_MARGIN_G) ? cand : (uint8_t)(current & 3u);
}

/* Rest vs motion: either the accel magnitude departs from 1 g by more than
 * thresh_g, or any gyro axis exceeds thresh_dps. */
static inline bool gz_imu_is_moving(float ax, float ay, float az,
                                    float gx, float gy, float gz,
                                    float thresh_g, float thresh_dps)
{
    if (fabsf(gz_imu_accel_mag_g(ax, ay, az) - 1.0f) > thresh_g) return true;
    return fabsf(gx) > thresh_dps || fabsf(gy) > thresh_dps || fabsf(gz) > thresh_dps;
}

/* ---- sample container ---------------------------------------------------- */

typedef struct {
    float   ax, ay, az;      /* g   */
    float   gx, gy, gz;      /* dps */
    bool    moving;
    uint8_t rotation;        /* 0..3, hysteretic */
} gz_imu_sample_t;

#if defined(ARDUINO) && GZ_BOARD_HAS_IMU
#include <Arduino.h>
#include <Wire.h>

/* Driver state, ONE object across every translation unit (C++17 inline
 * variable): main.cpp runs gz_imu_begin() at boot, gz_ui.cpp reads on the
 * LVGL task, and both must see the same WHO_AM_I. */
inline uint8_t s_gz_imu_whoami = 0;

static inline bool gz_imu_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    Wire.beginTransmission(GZ_I2C_ADDR_IMU);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;        /* repeated start */
    if (Wire.requestFrom((int)GZ_I2C_ADDR_IMU, (int)n) != (int)n) return false;
    for (uint8_t i = 0; i < n; ++i) buf[i] = (uint8_t)Wire.read();
    return true;
}

static inline bool gz_imu_write_reg(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(GZ_I2C_ADDR_IMU);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static inline uint8_t gz_imu_chip_id(void) { return s_gz_imu_whoami; }

/* Boot bring-up: verify WHO_AM_I, soft-reset, enable auto-increment, configure
 * accel+gyro and enable both. Read-only afterwards. Returns true if the part
 * answered with the expected id. Assumes Wire.begin() already ran (the boot
 * I2C scan does it on the shared bus). */
static inline bool gz_imu_begin(void)
{
    uint8_t id = 0;
    if (!gz_imu_read_regs(GZ_IMU_REG_WHOAMI, &id, 1)) {
        Serial.printf("  IMU QMI8658 (0x%02X): no ACK — check I2C wiring\n",
                      (unsigned)GZ_I2C_ADDR_IMU);
        s_gz_imu_whoami = 0;
        return false;
    }
    s_gz_imu_whoami = id;
    if (id != GZ_IMU_WHOAMI_VALUE) {
        Serial.printf("  IMU: unexpected WHO_AM_I 0x%02X (want 0x%02X)\n",
                      (unsigned)id, (unsigned)GZ_IMU_WHOAMI_VALUE);
        return false;
    }
    gz_imu_write_reg(GZ_IMU_REG_RESET, GZ_IMU_RESET_CMD);
    delay(15);
    gz_imu_write_reg(GZ_IMU_REG_CTRL1, GZ_IMU_CTRL1_AUTOINC);
    gz_imu_write_reg(GZ_IMU_REG_CTRL2, GZ_IMU_CTRL2_VALUE);
    gz_imu_write_reg(GZ_IMU_REG_CTRL3, GZ_IMU_CTRL3_VALUE);
    gz_imu_write_reg(GZ_IMU_REG_CTRL7,
                     GZ_IMU_CTRL7_ACC_EN | GZ_IMU_CTRL7_GYR_EN);
    Serial.printf("  IMU QMI8658 (0x%02X): id 0x%02X — accel+gyro enabled\n",
                  (unsigned)GZ_I2C_ADDR_IMU, (unsigned)id);
    return true;
}

/* Read one accel+gyro sample and fill *out (scaled, with motion + rotation).
 * `prev_rotation` feeds the hysteresis. Returns false on a bus error. */
static inline bool gz_imu_read(gz_imu_sample_t *out, uint8_t prev_rotation)
{
    uint8_t b[12];
    if (!gz_imu_read_regs(GZ_IMU_REG_AX_L, b, 12)) return false;
    int16_t ax = (int16_t)(b[0]  | (b[1]  << 8));
    int16_t ay = (int16_t)(b[2]  | (b[3]  << 8));
    int16_t az = (int16_t)(b[4]  | (b[5]  << 8));
    int16_t gx = (int16_t)(b[6]  | (b[7]  << 8));
    int16_t gy = (int16_t)(b[8]  | (b[9]  << 8));
    int16_t gz = (int16_t)(b[10] | (b[11] << 8));
    out->ax = gz_imu_accel_g(ax, GZ_IMU_ACC_RANGE_G);
    out->ay = gz_imu_accel_g(ay, GZ_IMU_ACC_RANGE_G);
    out->az = gz_imu_accel_g(az, GZ_IMU_ACC_RANGE_G);
    out->gx = gz_imu_gyro_dps(gx, GZ_IMU_GYR_RANGE_DPS);
    out->gy = gz_imu_gyro_dps(gy, GZ_IMU_GYR_RANGE_DPS);
    out->gz = gz_imu_gyro_dps(gz, GZ_IMU_GYR_RANGE_DPS);
    out->moving   = gz_imu_is_moving(out->ax, out->ay, out->az,
                                     out->gx, out->gy, out->gz, 0.08f, 12.0f);
    out->rotation = gz_imu_rotation(out->ax, out->ay, prev_rotation);
    return true;
}
#endif /* ARDUINO && GZ_BOARD_HAS_IMU */
