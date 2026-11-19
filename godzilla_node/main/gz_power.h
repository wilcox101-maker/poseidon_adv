/*
 * gz_power.h — battery / supply readout, read-only, per board (three-board
 * port, completion orders #88 §3.5 / §3.6). Shown on the status screen.
 *
 *   GZ_BOARD_POWER 0  none driven (1.85B: its BQ27220 gauge is a later unit)
 *   GZ_BOARD_POWER 1  AXP2101 PMIC over I2C — AMOLED-2.06
 *   GZ_BOARD_POWER 2  battery divider on an ADC pin — ES3C28P
 *
 * AXP2101 — READ-ONLY BY DESIGN (#84 §3): this code never writes a PMIC
 * register, so it cannot brown out the board or cut a rail. Register facts from
 * XPowersLib's AXP2101Constants.h / XPowersAXP2101.tpp (MIT, Lewis He; as
 * shipped in Waveshare's 2.06 demo — facts cited, no code taken):
 *   0x00 STATUS1  bit5 VBUS good, bit3 battery present
 *   0x01 STATUS2  bits7-5 00 standby / 01 charging / 10 discharging
 *   0x18 bit3     fuel gauge enabled          (0xA4 = battery %)
 *   0x30 bit0     battery-voltage ADC enabled (0x34[4:0]:0x35 = mV)
 * The gauge and the voltage ADC are switched on by *writes* in Waveshare's own
 * demo. We only READ their enable bits, and show a percentage or a voltage
 * only when the PMIC already has that measurement running — otherwise the
 * readout says so, rather than showing a number the part did not produce.
 *
 * ES3C28P — battery through R14 / R15 = 200 k / 200 k (2:1, from lcdwiki's
 * schematic) into IO9 (ADC1). Shown as volts only: a percentage from a LiPo
 * voltage would be an estimate, and #88 §4 rules out fabricated readings.
 *
 * Pure: the decode and scaling are stdint-only and host-tested
 * (test/test_godzilla_power); the Wire / ADC reads are ARDUINO-only and run on
 * the LVGL task (the status refresh), like touch and the IMU.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gz_board.h"

/* ---- AXP2101 registers (read only) ---- */
#define GZ_AXP_REG_STATUS1      0x00
#define GZ_AXP_REG_STATUS2      0x01
#define GZ_AXP_REG_GAUGE_CTRL   0x18
#define GZ_AXP_REG_ADC_CTRL     0x30
#define GZ_AXP_REG_VBAT_H       0x34
#define GZ_AXP_REG_VBAT_L       0x35
#define GZ_AXP_REG_BATT_PCT     0xA4

/* Plausible single-cell LiPo voltage window; outside it the reading is not
 * a battery (absent, floating, or an ADC not yet converted). */
#define GZ_POWER_MV_MIN         2500u
#define GZ_POWER_MV_MAX         4600u

typedef enum { GZ_CHG_UNKNOWN = 0, GZ_CHG_STANDBY, GZ_CHG_CHARGING, GZ_CHG_DISCHARGING } gz_chg_t;

typedef struct {
    bool     vbus;          /* external power present (AXP2101 only)         */
    bool     batt;          /* battery detected (AXP2101 only)               */
    gz_chg_t chg;
    bool     pct_valid;
    uint8_t  pct;           /* 0..100                                        */
    bool     mv_valid;
    uint16_t mv;            /* battery voltage                               */
} gz_power_t;

/* Raw AXP2101 register snapshot, as read in one pass. */
typedef struct {
    uint8_t status1, status2, gauge_ctrl, adc_ctrl, vbat_h, vbat_l, pct;
} gz_axp2101_regs_t;

static inline bool gz_power_mv_plausible(uint32_t mv)
{
    return mv >= GZ_POWER_MV_MIN && mv <= GZ_POWER_MV_MAX;
}

/* Decode an AXP2101 snapshot. Percentage only when a battery is present, the
 * gauge is on and the byte is 0..100; voltage only when a battery is present,
 * the ADC channel is on and the value is plausible. */
static inline void gz_power_axp2101_decode(const gz_axp2101_regs_t *r, gz_power_t *p)
{
    p->vbus = (r->status1 & 0x20u) != 0;
    p->batt = (r->status1 & 0x08u) != 0;
    switch (r->status2 >> 5) {
    case 0:  p->chg = GZ_CHG_STANDBY;     break;
    case 1:  p->chg = GZ_CHG_CHARGING;    break;
    case 2:  p->chg = GZ_CHG_DISCHARGING; break;
    default: p->chg = GZ_CHG_UNKNOWN;     break;
    }
    p->pct_valid = p->batt && (r->gauge_ctrl & 0x08u) && r->pct <= 100u;
    p->pct       = p->pct_valid ? r->pct : 0;
    uint16_t mv  = (uint16_t)(((r->vbat_h & 0x1Fu) << 8) | r->vbat_l);
    p->mv_valid  = p->batt && (r->adc_ctrl & 0x01u) && gz_power_mv_plausible(mv);
    p->mv        = p->mv_valid ? mv : 0;
}

/* Battery mV from the ADC-pin mV through a num/den divider (ES3C28P: 2/1).
 * Valid only when the result is a plausible single-cell voltage. */
static inline void gz_power_adc_decode(uint32_t pin_mv, uint32_t num, uint32_t den,
                                       gz_power_t *p)
{
    uint32_t mv = den ? (pin_mv * num + den / 2u) / den : 0u;
    p->vbus = false;
    p->batt = false;          /* this board has no presence signal */
    p->chg  = GZ_CHG_UNKNOWN; /* ...nor a charge-state one (TP4054 not wired) */
    p->pct_valid = false;
    p->pct  = 0;
    p->mv_valid = gz_power_mv_plausible(mv);
    p->mv   = p->mv_valid ? (uint16_t)mv : 0;
}

static inline const char *gz_power_chg_name(gz_chg_t c)
{
    switch (c) {
    case GZ_CHG_STANDBY:     return "standby";
    case GZ_CHG_CHARGING:    return "charging";
    case GZ_CHG_DISCHARGING: return "on battery";
    default:                 return "";
    }
}

/* ---- hardware half -------------------------------------------------------- */
#if defined(ARDUINO) && GZ_BOARD_POWER == 1
#include <Arduino.h>
#include <Wire.h>

static inline bool gz_power_axp_read(uint8_t reg, uint8_t *v)
{
    Wire.beginTransmission(GZ_I2C_ADDR_GAUGE);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)GZ_I2C_ADDR_GAUGE, 1) != 1) return false;
    *v = (uint8_t)Wire.read();
    return true;
}

/* One read-only pass over the AXP2101. False if the PMIC did not answer. */
static inline bool gz_power_read(gz_power_t *p)
{
    gz_axp2101_regs_t r;
    if (!gz_power_axp_read(GZ_AXP_REG_STATUS1, &r.status1) ||
        !gz_power_axp_read(GZ_AXP_REG_STATUS2, &r.status2) ||
        !gz_power_axp_read(GZ_AXP_REG_GAUGE_CTRL, &r.gauge_ctrl) ||
        !gz_power_axp_read(GZ_AXP_REG_ADC_CTRL, &r.adc_ctrl) ||
        !gz_power_axp_read(GZ_AXP_REG_VBAT_H, &r.vbat_h) ||
        !gz_power_axp_read(GZ_AXP_REG_VBAT_L, &r.vbat_l) ||
        !gz_power_axp_read(GZ_AXP_REG_BATT_PCT, &r.pct))
        return false;
    gz_power_axp2101_decode(&r, p);
    return true;
}
#elif defined(ARDUINO) && GZ_BOARD_POWER == 2
#include <Arduino.h>

static inline bool gz_power_read(gz_power_t *p)
{
    uint32_t pin_mv = (uint32_t)analogReadMilliVolts(GZ_PIN_BATT_ADC);
    gz_power_adc_decode(pin_mv, GZ_BATT_DIV_NUM, GZ_BATT_DIV_DEN, p);
    return true;
}
#endif
