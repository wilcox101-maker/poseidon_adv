/*
 * gz_st77916_rev.h — ST77916 panel-revision detection for the 1.85B (P2a).
 *
 * The Waveshare ESP32-S3-Touch-LCD-1.85B has shipped with two ST77916 panel
 * revisions that need DIFFERENT vendor init tables. Waveshare's own demo reads
 * register 0x04 over QSPI at a low clock and keys the table off the reply:
 *     00 7F 7F 7F -> vendor table version 1
 *     00 02 7F 7F -> vendor table version 2
 * Source: waveshareteam/ESP32-S3-Touch-LCD-1.85B,
 *   examples/Arduino-V3.2.0/examples/01_lvgl_demo/Display_ST77916.cpp (QSPI_Init).
 *
 * Any other reply (failed read, floating bus, a third revision) is UNKNOWN: the
 * driver then uses the component's built-in table and says so on serial, rather
 * than forcing a vendor table onto glass it was not written for.
 *
 * Pure: stdint only — pinned by test/test_godzilla_display_rev on the host.
 */
#pragma once

#include <stdint.h>

#define GZ_ST77916_REV_UNKNOWN 0
#define GZ_ST77916_REV_1       1
#define GZ_ST77916_REV_2       2

static inline int gz_st77916_panel_rev(const uint8_t reg04[4])
{
    if (reg04[0] != 0x00 || reg04[2] != 0x7F || reg04[3] != 0x7F)
        return GZ_ST77916_REV_UNKNOWN;
    if (reg04[1] == 0x7F) return GZ_ST77916_REV_1;
    if (reg04[1] == 0x02) return GZ_ST77916_REV_2;
    return GZ_ST77916_REV_UNKNOWN;
}
