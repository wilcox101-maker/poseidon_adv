/*
 * gz_rotate.h — screen auto-rotate from the IMU (post-port orders #98 §1.3).
 *
 * The IMU gives a hysteretic quarter-turn (gz_imu_rotation, #92); this picks the
 * display rotation to apply. LVGL 9.2.2's lv_display_set_rotation() swaps the
 * logical resolution for 90 / 270 and rotates touch points itself
 * (indev_pointer_proc), but leaves the pixels to the flush callback
 * (lv_display_rotate_area + lv_draw_sw_rotate) — gz_ui.cpp does that part.
 *
 * Rule: the UI is laid out at compile time for the board's W x H (gz_geom.h),
 * so a rotation may not change the logical resolution unless the panel is
 * square. The 1.85B (360 x 360) takes all four quarter-turns; a non-square
 * panel (AMOLED-2.06, 410 x 502) takes 0 / 180 only and holds its current
 * rotation through a sideways tilt. Boards without an IMU never rotate.
 *
 * One constant to disable (#98): GZ_UI_AUTO_ROTATE 0. If the bench shows the
 * screen turning the wrong way on a quarter-turn, GZ_UI_ROT_REVERSE 1 swaps
 * 90 and 270.
 *
 * Pure: host-tested by test/test_godzilla_rotate.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef GZ_UI_AUTO_ROTATE
#define GZ_UI_AUTO_ROTATE  1
#endif
#ifndef GZ_UI_ROT_REVERSE
#define GZ_UI_ROT_REVERSE  0
#endif

/* Display quarter-turn (0..3 = 0/90/180/270 deg) to apply for an IMU
 * orientation, given the current one and the panel size. */
static inline uint8_t gz_rotate_target(uint8_t imu_rot, uint8_t current,
                                       int w, int h, bool reverse)
{
    uint8_t r = (uint8_t)(imu_rot & 3u);
    if (reverse && (r & 1u)) r ^= 2u;              /* 90 <-> 270 */
    if (w != h && (r & 1u)) return (uint8_t)(current & 3u);   /* would change W x H */
    return r;
}
