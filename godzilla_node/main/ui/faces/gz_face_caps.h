/*
 * gz_face_caps.h — this board's watch-faces capability mask (firmware side).
 *
 * Kept out of the pure gz_faces.h so that header stays board-agnostic and
 * host-testable. Includes gz_board.h first, so GZ_FACE_BOARD_CAPS is defined
 * regardless of the order a translation unit pulls these headers in.
 *
 * Battery gates on GZ_BOARD_POWER != 0 (a driven readout): the 1.85B's BQ27220
 * is fitted-but-not-driven (POWER 0), so no battery complication there, matching
 * the orders' honest-bindings table (Battery: AMOLED / ES3C28P).
 */
#pragma once

#include "gz_board.h"
#include "gz_faces.h"

#define GZ_FACE_BOARD_CAPS                                             \
    ((GZ_BOARD_HAS_RTC     ? GZ_CAP_RTC  : 0) |                        \
     (GZ_BOARD_HAS_IMU     ? GZ_CAP_IMU  : 0) |                        \
     (GZ_BOARD_HAS_SD      ? GZ_CAP_SD   : 0) |                        \
     ((GZ_BOARD_POWER != 0) ? GZ_CAP_BATT : 0))
