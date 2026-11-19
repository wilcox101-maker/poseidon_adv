/*
 * gz_st77916_tables.h — Waveshare's ST77916 vendor init tables (defined in C,
 * gz_st77916_tables.c). See that file for the source citation.
 */
#pragma once

#include <stddef.h>
#include "esp_lcd_st77916.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const st77916_lcd_init_cmd_t gz_st77916_init_v1[];
extern const size_t                 gz_st77916_init_v1_len;
extern const st77916_lcd_init_cmd_t gz_st77916_init_v2[];
extern const size_t                 gz_st77916_init_v2_len;

#ifdef __cplusplus
}
#endif
