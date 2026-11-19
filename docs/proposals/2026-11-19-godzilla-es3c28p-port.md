# GODZILLA — third board: 2.8" ESP32-S3 Display, ES3C28P (design addendum)

**From:** GODZILLA, 2026-11-19. **Asked by:** Dale, 09:21 ET: "Also add a third device to port
to" (<https://www.lcdwiki.com/2.8inch_ESP32-S3_Display>).
**Status:** design only. This is an addendum to `2026-11-19-godzilla-amoled206-port.md` (same PR,
#84) and reuses its architecture: one tree, board chosen at build time, shared interfaces,
board-specific back ends.

The page covers two SKUs: **ES3C28P** (capacitive touch) and **ES3N28P** (no touch). This
design targets the **ES3C28P**, because the UI is touch-driven. A no-touch ES3N28P would need a
button-only input scheme and is listed as a later option (§5). **Which one Dale has is the open
question.**

## 1. The board (lcdwiki page; vendor init file `ILI9341V_Init.txt`)

| Part | ES3C28P | Compared with the other boards |
|---|---|---|
| SoC | ESP32-S3 N16R8: 16 MB flash, 8 MB **OPI** PSRAM | same memory keys as `[env:godzilla]` |
| Display | **ILI9341V**, 2.8" IPS, **240×320**, **4-wire SPI**: CS 10, DC 46, SCK 12, MOSI 11, MISO 13 | the only plain-SPI panel, and the smallest screen |
| LCD reset | **tied to the S3's EN**; no GPIO | panel reset is software (`0x01`) |
| Backlight | GPIO 45, high = on (4 white LEDs) | LEDC PWM like the 1.85B |
| Touch | **FT6336G**, I²C SDA 16, SCL 15, RST 18 (low = reset), INT 17 (low on touch) | FocalTech, same report layout as the 2.06's FT3168 |
| microSD | **SDMMC 4-bit**: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47 | same driver as the 1.85B, other pins |
| RTC / IMU / PMIC | **none listed** | new: no hardware clock, no motion sensor |
| Battery | TP4054 charger, 3.7 V LiPo, **battery ADC on GPIO 9** | voltage only, through an unknown divider |
| Audio | ES8311 codec (I²S MCLK 4, BCLK 5, LRCK 7, DOUT 8, DIN 6), FM8002E amp **EN GPIO 1 (low = enabled)**, MEMS mic | out of scope, as on the others |
| Extras | WS2812-type RGB LED on GPIO 42, BOOT on GPIO 0, 4-pin expansion IO 2/3/14/21, I²C header shared with touch | optional |

**Display init (vendor):** inversion **on** (`0x21`), COLMOD `0x55` (RGB565), MADCTL `0x08` for
portrait 240×320, **BGR**, plus the vendor's power / gamma table (`CF ED E8 CB F7 EA C0 C1 C5 C7
B6 F6 B1 F2 26 E0 E1`), then SLPOUT, 120 ms, DISPON. The landscape MADCTL values are `0x68` /
`0xA8`.

**The page doesn't give:** the FT6336G I²C address (FocalTech's default is **0x38**, inferred),
the battery divider ratio, or the default state of the amp-enable line. The schematic PDF on the
page settles all three; until then they're bench items.

## 2. Back ends: mostly reuse

- **Display.** Reuse the vendored `esp_lcd_st77916` driver again, this time in its **plain-SPI
  mode** (`use_qspi_interface = 0`), with the vendor ILI9341V table above and the RGB565 byte swap
  the UI already does. There's no reset GPIO (`reset_gpio_num = -1`), so the driver falls back to
  a software reset. The fallback is the Apache-2.0 `espressif/esp_lcd_ili9341` component,
  vendored like the ST77916. **No new library either way.** The SPI clock starts at 40 MHz.
- **Touch.** The AMOLED plan's FT3168 parser becomes one **FocalTech parser** (`gz_touch_ft.h`)
  for both the FT3168 and the FT6336G: register `0x02` is the touch count, `0x03..0x06` the
  first point. It's written once, host-tested once, with per-board INT/RST pins.
- **microSD.** It's SDMMC 4-bit like the 1.85B, so `gz_recon_task.cpp` runs **unchanged** with
  this board's pins. Recon works on this board as soon as the map lands.
- **Clock: no RTC.** `GZ_BOARD_HAS_RTC 0` skips `gz_rtc_begin()`. The software clock in
  `gz_clock` starts "not set", the watch face shows "hold to set", and the existing manual set
  works, but the time is lost at power-off. Recon falls back to its existing
  `/recon/NODATE/SESSION.CSV` path until the clock is set. A real fix needs either the
  Cardputer time-sync wire message (already **Needs Dale** in #82) or an external RTC on the
  I²C header. Both are listed, neither is built.
- **No IMU.** `GZ_BOARD_HAS_IMU 0`. #82 items 1–2 (IMU, auto-rotate / wake) skip this board.
  Touch wake still works through the backlight.
- **Battery.** A read-only ADC on GPIO 9 shown as volts on the status screen. It's a later small
  unit, once the divider is read off the schematic.
- **Amp enable (GPIO 1, low = enabled).** If the schematic shows no pull, boot drives it **high**
  (amp off), so a floating line can't make the speaker hiss. That's a local GPIO, not RF, and it
  is the only output added.
- **RGB LED (GPIO 42).** It could flash red or amber on surveillance / tracker detections as a
  local light (no RF, so passive-safe). It's optional and listed, not planned.

## 3. UI on 240×320

This is the tightest screen of the three. The geometry pass from the AMOLED plan (§4,
`GZ_UI_CX/CY/SAFE_R`) covers it: **SAFE_R = 120**, so the radar dial shrinks to about
R_MAX 110 and the 80 px of spare height takes the footer / REC lines. Two fixed sizes that would
overflow get derived instead: the radar **detail card is 236 px wide**, almost the full 240, and
the Hunt plot is 240 px. Fonts stay as they are. If a line doesn't fit, it's shortened, not
shrunk below Montserrat 12. The IPS panel needs no burn-in care.

## 4. Decisions
- **Target the ES3C28P (touch).** The ES3N28P is a later option with BOOT-button navigation.
- **Build flag `GZ_BOARD_ES3C28P`, env `[env:godzilla-es3c28p]`, HELLO name `GODZILLA28`** (fits
  the 11 + NUL field). The selector in #86 takes a third header the same way.
- **Shared work is done once for all boards:** one FocalTech touch parser, one geometry pass,
  one SD driver path, one display driver reused in both QSPI and SPI modes.
- **Passive rules unchanged.** The only new outputs are the backlight and amp-off GPIOs.

## 5. Order across both ports
Interleaved so each shared piece lands once:
1. AMOLED unit 1 — board selector + 2.06 map (#86).
2. **ES3C28P map + `[env:godzilla-es3c28p]` + CI row.** It builds with **SD + Recon on** and the
   UI headless, `HAS_RTC 0`, `HAS_IMU 0`, amp held off.
3. AMOLED unit 2 — CO5300 display.
4. **ES3C28P display** — ILI9341V over SPI with the same reused driver.
5. FocalTech touch parser, FT3168 and FT6336G together.
6. Geometry pass for all three boards (+ AMOLED pixel orbit / dim).
7. AMOLED microSD over SPI.
8. Power readouts: AXP2101 (2.06) and battery ADC (ES3C28P).
9. *Later / Dale:* ES3N28P button navigation, RGB-LED alerts, clock sync for RTC-less boards.

Then back to #82 (IMU first; the 1.85B and 2.06 have one, the ES3C28P doesn't).

## 6. Needs Dale
- **Which SKU do you have, ES3C28P (touch) or ES3N28P (no touch)?** This plan assumes the
  ES3C28P.
- **Bench, once the units land:** the boot I²C scan for the touch address; the image fills the
  screen with correct colours (if red and blue are swapped, it's the MADCTL BGR bit; if the image
  is inverted, it's `0x21`); the speaker stays silent; Recon writes to a FAT32 card.
- **Time on RTC-less boards:** OK to live with "hold to set" for now? The proper fix is the
  Cardputer time-sync wire message, which is already on #82's Needs-Dale list.
