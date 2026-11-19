/*
 * gz_rtc.h — PCF85063 real-time-clock driver for the GODZILLA node (P2a).
 *
 * The Waveshare 1.85B carries a PCF85063 RTC on the shared touch+RTC I2C bus
 * (gz_board.h: 0x51, SCL 10 / SDA 11, INT 6). This is the next P2a unit after
 * the ST77916/LVGL status screen (#71); it feeds a real clock to the status
 * view now and the pocket-watch face later (P2c). Per Claudius's P2a advice
 * (docs/proposals/2026-11-19-godzilla-p2a-display-advice.md §6): keep the
 * BCD/time conversion PURE and native-tested; only the I2C read/write is
 * on-target. If the oscillator-stop flag is set (cold boot, dead backup cell),
 * seed the clock from the firmware build time so the node never starts in the
 * year 2000.
 *
 * Passive w.r.t. the project's RF rules: it drives only the on-board I2C bus to
 * a soldered-on peripheral; it transmits no radio frames.
 *
 * Register map (PCF85063A/TP datasheet, time/date block 0x04..0x0A):
 *   0x04 Seconds  bit7 = OS (oscillator stop), bits6-0 = BCD seconds 0-59
 *   0x05 Minutes  bits6-0 = BCD minutes 0-59
 *   0x06 Hours    bits5-0 = BCD hours 0-23  (24-hour mode; see note below)
 *   0x07 Days     bits5-0 = BCD day-of-month 1-31
 *   0x08 Weekdays bits2-0 = 0-6, user-defined (we adopt 0=Sunday)
 *   0x09 Months   bits4-0 = BCD month 1-12
 *   0x0A Years    bits7-0 = BCD year 0-99 (offset from 2000)
 *
 * We assume 24-hour mode: the PCF85063 powers up with Control_1 bit1 (12_24)=0,
 * i.e. 24-hour, and gz_rtc_begin() never sets 12-hour mode. gz_rtc_decode()
 * therefore reads the Hours register as a plain 6-bit BCD field.
 *
 * Pure: stdint/stdbool/string only, no Arduino/Wire, so test_godzilla_rtc pins
 * the BCD, decode/encode round-trip, weekday, build-time parse and seed logic
 * on the host. The Wire transactions are ARDUINO-only, behind #ifdef.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "gz_board.h"   /* GZ_I2C_ADDR_RTC / GZ_PIN_I2C_* */

/* PCF85063 register addresses. */
#define GZ_RTC_REG_CONTROL1 0x00
#define GZ_RTC_REG_SECONDS  0x04   /* first byte of the 7-byte time block */
#define GZ_RTC_TIME_BYTES   7      /* 0x04..0x0A */

#define GZ_RTC_OS_FLAG      0x80   /* Seconds bit7: oscillator stop / data loss */

/* Decoded wall-clock time. year is the full year (e.g. 2026), not the 2-digit
 * register value. weekday is 0-6 with 0=Sunday. osc_stopped mirrors the OS flag
 * that was present in the Seconds register when this was read: true means the
 * oscillator stopped since the last valid set, so the time is not trustworthy. */
typedef struct {
    uint16_t year;       /* full year, e.g. 2026                 */
    uint8_t  month;      /* 1-12                                 */
    uint8_t  day;        /* 1-31                                 */
    uint8_t  weekday;    /* 0-6, 0=Sunday                        */
    uint8_t  hour;       /* 0-23                                 */
    uint8_t  minute;     /* 0-59                                 */
    uint8_t  second;     /* 0-59                                 */
    bool     osc_stopped;/* OS flag seen on read (decode only)   */
} gz_datetime_t;

/* ---- Pure BCD helpers ---- */

static inline uint8_t gz_bcd2bin(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10u + (v & 0x0Fu));
}

static inline uint8_t gz_bin2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10u) << 4) | (v % 10u));
}

/* ---- Pure decode / encode of the 7-byte time block ---- */

/* Decode regs[0..6] (register 0x04..0x0A as read from the device) into *out.
 * Masks each field to its valid bits before BCD conversion so stray high bits
 * (OS flag, reserved bits) never corrupt the value. */
static inline void gz_rtc_decode(const uint8_t regs[GZ_RTC_TIME_BYTES],
                                 gz_datetime_t *out)
{
    out->osc_stopped = (regs[0] & GZ_RTC_OS_FLAG) != 0;
    out->second  = gz_bcd2bin((uint8_t)(regs[0] & 0x7F));
    out->minute  = gz_bcd2bin((uint8_t)(regs[1] & 0x7F));
    out->hour    = gz_bcd2bin((uint8_t)(regs[2] & 0x3F));  /* 24-hour mode */
    out->day     = gz_bcd2bin((uint8_t)(regs[3] & 0x3F));
    out->weekday = (uint8_t)(regs[4] & 0x07);
    out->month   = gz_bcd2bin((uint8_t)(regs[5] & 0x1F));
    out->year    = (uint16_t)(2000u + gz_bcd2bin(regs[6]));
}

/* Encode *dt into regs[0..6] ready to write to register 0x04. The Seconds byte
 * is written with bit7 (OS) CLEAR, which is how the device is told the new time
 * is valid. Year is reduced to its 2-digit register form (year - 2000). */
static inline void gz_rtc_encode(const gz_datetime_t *dt,
                                 uint8_t regs[GZ_RTC_TIME_BYTES])
{
    regs[0] = (uint8_t)(gz_bin2bcd(dt->second) & 0x7F);   /* OS cleared */
    regs[1] = gz_bin2bcd(dt->minute);
    regs[2] = gz_bin2bcd(dt->hour);
    regs[3] = gz_bin2bcd(dt->day);
    regs[4] = (uint8_t)(dt->weekday & 0x07);
    regs[5] = gz_bin2bcd(dt->month);
    regs[6] = gz_bin2bcd((uint8_t)(dt->year % 100u));
}

/* ---- Validation ---- */

/* Days in a Gregorian month (1-12) of a full year; 0 for a bad month. Shared
 * by gz_rtc_valid() and the watch's software clock (gz_watch.h) so there is
 * one calendar. */
static inline uint8_t gz_rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return (uint8_t)((month == 2 && leap) ? 29 : mdays[month - 1]);
}

/* True if every field is in range for a real calendar date/time. Deliberately
 * strict (rejects month 0, day 0, hour 24) so a garbage read or a stopped clock
 * is caught and the caller seeds from build time. Day is bounded per-month,
 * including a Gregorian leap-year check on February. */
static inline bool gz_rtc_valid(const gz_datetime_t *dt)
{
    if (dt->year < 2000 || dt->year > 2099) return false;
    if (dt->month < 1 || dt->month > 12)    return false;
    if (dt->hour > 23 || dt->minute > 59 || dt->second > 59) return false;
    if (dt->weekday > 6) return false;
    uint8_t dmax = gz_rtc_days_in_month(dt->year, dt->month);
    if (dt->day < 1 || dt->day > dmax) return false;
    return true;
}

/* ---- Pure day-of-week (Sakamoto), 0=Sunday ---- */

/* Day of week for a Gregorian date, 0=Sunday..6=Saturday. Valid for year >= 1.
 * Used to fill the weekday register when seeding from build time, since
 * __DATE__ carries no weekday. */
static inline uint8_t gz_rtc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const int t[12] = {0,3,2,5,0,3,5,1,4,6,2,4};
    int y = year;
    if (month < 3) y -= 1;
    return (uint8_t)(((y + y/4 - y/100 + y/400 + t[month - 1] + day) % 7 + 7) % 7);
}

/* ---- Build-time parse (pure) ---- */

/* Parse the compiler's __DATE__ ("Mmm dd yyyy", e.g. "Nov 19 2026") and
 * __TIME__ ("hh:mm:ss") into *out, filling weekday from gz_rtc_weekday().
 * osc_stopped is set false. Returns true on a well-formed, valid date. This is
 * how the node recovers a sane wall clock on a cold boot with no backup cell. */
static inline bool gz_rtc_parse_build(const char *date, const char *time,
                                      gz_datetime_t *out)
{
    static const char *mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    if (!date || !time) return false;
    char m0 = date[0], m1 = date[1], m2 = date[2];
    uint8_t month = 0;
    for (uint8_t i = 0; i < 12; ++i) {
        if (mon[i*3] == m0 && mon[i*3+1] == m1 && mon[i*3+2] == m2) {
            month = (uint8_t)(i + 1);
            break;
        }
    }
    if (month == 0) return false;

    /* Day: __DATE__ pads a single-digit day with a leading space. */
    int day = (date[4] == ' ' ? 0 : (date[4] - '0') * 10) + (date[5] - '0');
    int year = (date[7]-'0')*1000 + (date[8]-'0')*100 + (date[9]-'0')*10 + (date[10]-'0');
    int hh = (time[0]-'0')*10 + (time[1]-'0');
    int mm = (time[3]-'0')*10 + (time[4]-'0');
    int ss = (time[6]-'0')*10 + (time[7]-'0');

    out->year    = (uint16_t)year;
    out->month   = month;
    out->day     = (uint8_t)day;
    out->hour    = (uint8_t)hh;
    out->minute  = (uint8_t)mm;
    out->second  = (uint8_t)ss;
    out->weekday = gz_rtc_weekday(out->year, out->month, out->day);
    out->osc_stopped = false;
    return gz_rtc_valid(out);
}

/* Decide whether to seed. If the read time is osc-stopped or out of range, copy
 * *build into *dt and return true (caller should write it back). Otherwise
 * leave *dt untouched and return false. Pure, so the policy is host-tested. */
static inline bool gz_rtc_seed_if_stopped(gz_datetime_t *dt,
                                          const gz_datetime_t *build)
{
    if (dt->osc_stopped || !gz_rtc_valid(dt)) {
        *dt = *build;
        dt->osc_stopped = false;
        return true;
    }
    return false;
}

#if defined(ARDUINO) && GZ_BOARD_HAS_RTC   /* no Wire half on RTC-less boards */
#include <Arduino.h>
#include <Wire.h>

/* Read the 7-byte time block from the PCF85063 into *out. Returns true if the
 * device ACKed and 7 bytes came back. Assumes Wire.begin() has already run
 * (gz_i2c_scan_report() does it at boot on the shared bus). */
static inline bool gz_rtc_read(gz_datetime_t *out)
{
    Wire.beginTransmission(GZ_I2C_ADDR_RTC);
    Wire.write((uint8_t)GZ_RTC_REG_SECONDS);
    if (Wire.endTransmission(false) != 0) return false;       /* repeated start */
    if (Wire.requestFrom((int)GZ_I2C_ADDR_RTC, (int)GZ_RTC_TIME_BYTES)
            != (int)GZ_RTC_TIME_BYTES) return false;
    uint8_t regs[GZ_RTC_TIME_BYTES];
    for (uint8_t i = 0; i < GZ_RTC_TIME_BYTES; ++i) regs[i] = (uint8_t)Wire.read();
    gz_rtc_decode(regs, out);
    return true;
}

/* Write *dt to the PCF85063 time block (clears the OS flag). Returns true on
 * ACK. Does not stop/start the oscillator: a plain block write is atomic enough
 * for a one-shot seed, and the datasheet's OS clear-on-write marks it valid. */
static inline bool gz_rtc_write(const gz_datetime_t *dt)
{
    uint8_t regs[GZ_RTC_TIME_BYTES];
    gz_rtc_encode(dt, regs);
    Wire.beginTransmission(GZ_I2C_ADDR_RTC);
    Wire.write((uint8_t)GZ_RTC_REG_SECONDS);
    for (uint8_t i = 0; i < GZ_RTC_TIME_BYTES; ++i) Wire.write(regs[i]);
    return Wire.endTransmission() == 0;
}

/* Boot bring-up: read the clock; if it stopped or is garbage, seed from the
 * firmware build time (__DATE__/__TIME__) and write it back. Prints a one-line
 * status to Serial. Leaves *now holding the current best time. Returns true if
 * the RTC answered on the bus at all. *seeded (optional) is set true when the
 * clock was seeded from build time this boot — i.e. nobody has set it yet. */
static inline bool gz_rtc_begin(gz_datetime_t *now, bool *seeded)
{
    if (seeded) *seeded = false;
    if (!gz_rtc_read(now)) {
        Serial.printf("  RTC PCF85063 (0x%02X): no ACK — check I2C wiring\n",
                      (unsigned)GZ_I2C_ADDR_RTC);
        return false;
    }
    gz_datetime_t build;
    bool built = gz_rtc_parse_build(__DATE__, __TIME__, &build);
    if (built && gz_rtc_seed_if_stopped(now, &build)) {
        gz_rtc_write(now);
        if (seeded) *seeded = true;
        Serial.printf("  RTC PCF85063: oscillator was stopped — seeded %04u-%02u-%02u %02u:%02u:%02u (build)\n",
                      now->year, now->month, now->day,
                      now->hour, now->minute, now->second);
    } else {
        Serial.printf("  RTC PCF85063: %04u-%02u-%02u %02u:%02u:%02u\n",
                      now->year, now->month, now->day,
                      now->hour, now->minute, now->second);
    }
    return true;
}
#endif /* ARDUINO && GZ_BOARD_HAS_RTC */
