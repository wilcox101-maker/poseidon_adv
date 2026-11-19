# GODZILLA — port to the Waveshare ESP32-S3-Touch-AMOLED-2.06 (design)

**From:** GODZILLA, 2026-11-19. **Asked by:** Dale, 09:11 ET: "Port Godzilla to this device also"
(<https://www.waveshare.com/product/esp32-s3-touch-amoled-2.06.htm>).
**Status:** design only. No code in this PR. Implementation follows as the units in §6.

"Also" means **a second board**, not a replacement. The 1.85B stays the reference target, and
both boards build from the one `godzilla_node/` tree.

## 1. The board

The sources are the Waveshare wiki (<https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.06>) and
Waveshare's demo repo `waveshareteam/ESP32-S3-Touch-AMOLED-2.06` @ `3de8684` (Arduino_GFX
`Arduino_CO5300`, Arduino_DriveBus `Arduino_FT3x68`, the LVGL v9 example).

| Part | 1.85B (today) | AMOLED-2.06 | Port impact |
|---|---|---|---|
| SoC | ESP32-S3R8, 8 MB OPI PSRAM, 16 MB flash | ESP32-S3R8, 8 MB OPI PSRAM, **32 MB** flash | none: the vendor demo uses the same 16 MB layout + OPI PSRAM as `[env:godzilla]` |
| Display | ST77916, 360×360 **round** LCD, QSPI | **CO5300**, 410×502 **rectangular AMOLED**, QSPI | new init table + 22 px column gap; UI geometry pass |
| Display pins | CS 21, CLK 40, D0–3 46/45/42/41, RST 3 | CS 12, CLK 11, D0–3 4/5/6/7, RST 8, **TE 13** | pin map |
| Brightness | LEDC PWM backlight, GPIO 5 | **no backlight**: DCS `0x51` brightness over QSPI | `gz_display_backlight()` maps to `0x51` |
| Touch | CST816S @ 0x15, INT 4, RST 1 | **FT3168 @ 0x38**, INT 38, RST 9 | new touch parser |
| I²C | SDA 11, SCL 10 | SDA 15, SCL 14 | pin map |
| RTC | PCF85063 @ 0x51 | PCF85063 @ 0x51, INT 39 | **reuse `gz_rtc.h` as-is** |
| IMU | QMI8658 @ 0x6B | QMI8658 @ 0x6B, INT 21 | reuse: #82 item 1 is written once for both |
| Power | BQ27220 gauge @ 0x55 | **AXP2101 PMIC** (0x34) | new read-only status decode |
| microSD | SDMMC 4-bit (15/14/16/17/12/13) | **SPI**: MOSI 1, SCK 2, MISO 3, CS 17 | Recon needs an SPI back end |
| Audio | ES8311 + ES7210 | ES8311 + ES7210 + NS4150B amp (EN 46) | out of scope (same as 1.85B) |
| Buttons | — | BOOT 0 (low = pressed), **PWR readable on GPIO 10** (high = pressed) | optional input |
| USB | native | native (19/20) | none |

The FT3168 address (0x38) comes from `Arduino_FT3x68.h` and the QMI8658 address (0x6B) from
SensorLib. The AXP2101 address (0x34) is the chip default, **unconfirmed on this board**: the boot
I²C scan will settle it (bench).

## 2. Architecture: one tree, board chosen at build time

- **New env `[env:godzilla-amoled206]`** extends `[env:godzilla]`, adds `-DGZ_BOARD_AMOLED206`
  and keeps the same memory keys, lib pins and `build_src_filter`. It gets its own CI matrix row.
  CRLF in `platformio.ini` is preserved (byte edit).
- **`gz_board.h` becomes a dispatcher**: `gz_board_185b.h` (the current content, unchanged) or
  `gz_board_amoled206.h`, plus capability flags: `GZ_LCD_ROUND`, `GZ_LCD_HAS_BACKLIGHT`,
  `GZ_TOUCH_CST816S` / `GZ_TOUCH_FT3168`, `GZ_SD_SDMMC` / `GZ_SD_SPI`, `GZ_PWR_BQ27220` /
  `GZ_PWR_AXP2101`. Each board header keeps its own `static_assert` collision checks.
  `test_godzilla_board` runs both maps.
- **Interfaces stay the same.** `gz_display.h`, `gz_touch.h`'s point struct, `gz_recon_task.h` and
  the UI module APIs don't change, so the radar, Hunt, watch, Recon, link and scan code is
  shared. Only the back ends are board-specific.

## 3. Back ends

**Display (CO5300).** Reuse the vendored `esp_lcd_st77916` driver. It already takes a vendor
init table, frames QSPI commands (`0x02`) and pixels (`0x32`) the way the CO5300 expects, and
implements `set_gap`. The CO5300 table comes from Arduino_GFX's `co5300_init_operations`: SLPOUT,
120 ms, `FE 00`, `C4 80` (SPI mode), `3A 55` (RGB565), `53 20`, `63 FF`, DISPON, `51 xx`,
`58 00`. Then `esp_lcd_panel_set_gap(22, 0)`, the vendor's column offset. The ST77916
revision probe is compiled out, and `gz_display_panel_rev()` returns "n/a". If the driver's
built-in pre-table writes (MADCTL / COLMOD) misbehave on the CO5300, the fallback is a ~150-line
CO5300 panel in the tree on the same `esp_lcd_panel_io` QSPI path. **Still no new library either
way.**
- Brightness: `gz_display_backlight(pct)` writes `0x51` (0..255). The panel comes up at
  brightness 0 and is raised after the first flush, keeping the existing "no flash at boot" rule.
- Window alignment: the CO5300 wants even x/width. The existing rounder already sends
  full-width, even-row strips, so it carries over.
- Starting at 40 MHz QSPI, as on the 1.85B, and raising only after a clean bench run.

**Touch (FT3168).** A pure parser (`gz_touch_ft3168.h`) for the FocalTech report: `0x02` touch
count, `0x03..0x06` X/Y with event bits. It fills the same `gz_touch_point_t`. The `#ifdef
ARDUINO` half keeps the 1.85B's INT-gated read pattern (INT 38, RST 9). It is host-tested like
`gz_touch.h`, and the existing SWAP/MIRROR constants still apply.

**Power (AXP2101), read-only.** A pure decode of battery %, the charging / VBUS bits and battery
voltage, with the `#ifdef ARDUINO` read split as in `gz_rtc.h`, shown on the status screen. **The
firmware never writes a PMIC rail register.** The vendor's LVGL demo drives the panel and touch
with no PMIC setup at all, so the power-on rails already feed them. A wrong rail write can
brown out the board or cut the panel. The PWR key (GPIO 10) is read for the "wake" behaviour in
#82 item 2.

**microSD over SPI.** A small `gz_sd` shim (`begin / exists / mkdir / open`) picks `SD_MMC` or
`SD` (SPI, bundled with the core, so no new library) by board flag. `gz_recon_task.cpp` calls the
shim and is otherwise unchanged. Pins are MOSI 1, SCK 2, MISO 3, CS 17, on their own SPI host,
off the display's QSPI bus.

## 4. UI on a 410×502 rectangle

Today's screens assume a 360×360 circle: fixed `R_MAX 158`, a 250 px column and "inscribed
radius 180". The geometry pass replaces those constants with values derived from `gz_board.h`:
`GZ_UI_CX / GZ_UI_CY` and `GZ_UI_SAFE_R`, which is 180 on the round glass and 205 on the 2.06
(half the width; the 2.06's corners are rounded but the circle fits). The 1.85B gets the same numbers
it has now, so **nothing moves on the reference board**.
- **Radar and watch:** the dial stays round and centred. On the 2.06 the extra 92 px of height
  holds the footer / REC lines below the dial, so they no longer overlap the outer ring.
- **Hunt (#83) and status:** widths come from `GZ_UI_SAFE_R`. The 2.06 gets a wider trace and
  one line of text more.
- **AMOLED burn-in:** the radar's rings, crosshair and trident are static, high-contrast pixels.
  On the 2.06 the whole radar layer shifts by ±2 px once a minute (pixel orbit). The panel also dims
  to a low `0x51` after 60 s with no touch and restores on touch / PWR. Both are compile-time
  constants. The black deepwater background already keeps AMOLED power low.

## 5. Decisions (fast track)
- **Same firmware, build flag, no runtime board detection.** The two boards share no display or
  touch pins, so a wrong guess would drive the wrong pins. The build flag is explicit and
  CI proves both builds.
- **Reuse the vendored ST77916 driver before writing a CO5300 one.** It means less code and the
  bus path is already proven on hardware. The fallback is spelled out above.
- **The AXP2101 is read-only.** The reason is in §3.
- **No new libraries** (no Arduino_GFX, Arduino_DriveBus, SensorLib or XPowersLib). Their register
  facts are cited; the drivers are raw I²C / `esp_lcd`, the same as the 1.85B.
- **Audio stays out**, as on the 1.85B (#82 item 5 covers both boards later).
- **Passive rules unchanged.** Same radios, same publish path, same wire (header v5), no
  `src/*` or `c5_node` change. The node sends its own HELLO name, so the Cardputer sees a second
  GODZILLA; the default name gets a `-206` suffix so the two can be told apart.

## 6. Units (each its own PR, `claude/godzilla-amoled206-<topic>`)
1. **Board split + env + CI.** `gz_board.h` dispatcher, `gz_board_amoled206.h`, both maps in
   `test_godzilla_board`, `[env:godzilla-amoled206]` and a CI row. The 2.06 build runs headless
   (display/touch/SD compiled out behind flags) and **no byte changes for the 1.85B**.
2. **CO5300 display.** Init table + gap + `0x51` brightness. The UI is up on the 2.06.
3. **FT3168 touch.** Pure parser, native-tested, plus the INT-gated read.
4. **Geometry pass.** `GZ_UI_*` constants through radar / Hunt / watch / status, plus AMOLED
   pixel-orbit and idle dim.
5. **microSD over SPI.** The `gz_sd` shim; Recon on the 2.06.
6. **AXP2101 status + PWR key.** Read-only decode, native-tested, on the status screen.

After that, back to #82's order, starting with **IMU (QMI8658)**, written once with both boards'
INT pins.

## 7. Needs Dale's bench (2.06), once the units land
- Boot I²C scan: `0x38` touch, `0x51` RTC, `0x6B` IMU, and confirm the **AXP2101 address**.
- The image is centred with no 22 px stripe on either edge. If there is one, the gap is the
  one-constant fix.
- Colours are right (RGB, not BGR). If red and blue are swapped, the MADCTL BGR bit fixes it.
- Touch axes are correct. If not, `GZ_TOUCH_SWAP_XY` / `_MIRROR_*`.
- Brightness: no flash at boot, the panel dims after 60 s idle, and a touch restores it.
- A FAT32 card in the TF slot gives Recon the same results as on the 1.85B.
- Battery % / charging shows on the status screen with the battery connected and with USB-only.

## Needs Claudius
- This goes **ahead of #82 item 1** because Dale asked for it directly. The IMU unit follows
  it and covers both boards.
- A second GODZILLA on the bus means two nodes publishing on the same ESP-NOW broadcast. Per the
  P1 multinode work the Cardputer already keys per-node, so no wire change is expected. Please
  flag it if the S3 side assumes a single GODZILLA anywhere.
