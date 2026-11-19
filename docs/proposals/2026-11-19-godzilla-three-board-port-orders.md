# GODZILLA — three-board port: completion orders

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19, on Dale's "finish the Dragon→GODZILLA
port across three devices; full internal testing; don't stop until done."

These orders ratify and sequence the work in your three merged designs
([P3 Labs](2026-11-19-godzilla-p3-orders.md), [AMOLED-2.06](2026-11-19-godzilla-amoled206-port.md),
[2.8" ES3C28P](2026-11-19-godzilla-es3c28p addendum)) into one plan, with the corrections in §4.
Division of labour (Dale, 2026-11-19): **GODZILLA engineers** the node, boards and features;
**Claudius reviews, merges, resolves conflicts, and produces the screenshots / UI-flow.** Don't
author the screenshots yourself; don't wait on them to ship code.

## 1. The three targets
| Board | Screen | Touch | RTC | IMU | SD | Power |
|---|---|---|---|---|---|---|
| **Waveshare 1.85B** (reference, built) | ST77916 360×360 **round** QSPI | CST816S | PCF85063 | QMI8658 | SDMMC | BQ27220 |
| **Waveshare AMOLED-2.06** | CO5300 410×502 QSPI | FT3168 | PCF85063 | QMI8658 | SPI | AXP2101 |
| **2.8" ES3C28P** (in-house — bench-testable) | ILI9341V 240×320 **SPI** | FT6336G | — | — | SDMMC | batt ADC |

Pin maps are verified against the Waveshare wikis and the lcdwiki page; they match your board
headers. Done = all three build their env green, bring up display + touch + SD, run the radar /
watch (where an RTC exists) / status / Hunt UI at the board's own geometry, and every pure core is
native-tested. The 2.8" being in-house means its bring-up is bench-verified, not desk-only.

## 2. Completion bar (Dale)
1. Three PlatformIO envs green in CI: `godzilla` (1.85B), `godzilla-amoled206`, `godzilla-es3c28p`.
2. Each board: display + touch + SD up; UI adapts to its resolution; features that need absent
   hardware are compiled out by capability flag, never stubbed to fake data.
3. Full **internal** testing: a native Unity suite for every pure core (touch parse, IMU math,
   geometry, display init-table selection). The on-target back-ends stay desk-only except the
   2.8", which gets a bench pass.
4. Claudius delivers screenshots + UI flow for all three; you keep each PR's "Dale's bench" list.

## 3. Build order (one topic per PR; stack with merge commits, never squash)
**Shared, write-once (do these first — they unblock two boards each):**
1. **FocalTech touch parser** `gz_touch_ft.h` — one parser for FT3168 and FT6336G (reg 0x02 =
   count, 0x03..0x06 = point 1), per-board INT/RST and address. Pure parse + orientation
   native-tested. The CST816S stays its own driver.
2. **Geometry pass** — one source of truth for screen width/height/shape
   (`GZ_LCD_WIDTH`/`HEIGHT`/`GZ_LCD_ROUND`, already in the board headers). Every UI module reads
   those instead of hard-coded 360/180. Radar inscribes in `min(W,H)/2`; cards clamp to width.
   Native-test the layout math (e.g. a 236 px card on a 240 px panel).
3. **IMU driver** `gz_imu.h`, **board-aware behind `GZ_BOARD_HAS_IMU`** — pure scaling / tilt /
   motion / orientation core + native test; Wire behind `#ifdef ARDUINO`; boot WHO_AM_I (==0x05)
   is the proof. 1.85B + AMOLED only; the 2.8" compiles it out. (Supersedes the closed #85, which
   was unconditional — that was Claudius's error, not yours; reuse its tested math if useful.)
4. **Display driver reuse** — the vendored `esp_lcd_st77916` in QSPI (CO5300 table) and SPI
   (ILI9341V table, `reset_gpio_num=-1`, software reset) modes. If its built-in MADCTL/COLMOD
   writes fight the CO5300 or ILI9341V, fall back to the Apache-2.0 `esp_lcd_co5300` /
   `esp_lcd_ili9341` vendored components — **no new library pin either way**.

**Per board, once the shared pieces land:**
5. AMOLED-2.06: CO5300 bring-up → FT3168 wiring → SPI microSD → AXP2101 read-only power → full UI.
6. 2.8" ES3C28P: ILI9341V bring-up → FT6336G wiring → SDMMC (reuse `gz_recon_task`) → battery
   ADC → amp held **off** at boot → full UI.
7. 1.85B: IMU bring-up + auto-rotate/wake (now that §3.3 exists).

## 4. Corrections and guard-rails
- **Capability flags, not stubs.** A board without an RTC/IMU sets `GZ_BOARD_HAS_RTC/IMU 0` and
  the code path is `#if`-compiled out. Never feed the watch or status a fabricated time or motion.
- **FocalTech address 0x38 is inferred** on both touch parts — the boot I2C scan must confirm it,
  and the driver must not hard-fault if another FocalTech address ACKs; log and continue.
- **Passive stays passive.** The only new outputs across all three boards are the LCD backlight /
  brightness and the 2.8"'s amp-enable (**driven off at boot**). Nothing transmits but the
  existing ESP-NOW HELLO/publish. No `src/*`, no C5, no wire-format change — if the port ever
  seems to need one, stop and ask Dale.
- **One geometry source.** Don't copy 360/180 constants into the new boards; route everything
  through §3.2 so a fourth board is just a header.
- **CI must stay green on every env**, including `native-test`. A pure core doesn't land until its
  Unity suite passes in CI, not just on a desk compile.
- **Stack with merge commits.** Squashing a stacked port PR re-conflicts the next one (seen at
  #72→#73). Claudius merges; flag anything in §4's "ask Dale" set instead of merging.

## 5. Reserved for Dale
- Any transmit added for a feature (CSI channel-sounding, BLE connect/emulate), wardrive (no GPS
  on any of the three), a Cardputer time-sync wire type, or a **fourth** board.
- Whether the in-house 2.8" is the touch ES3C28P or the no-touch ES3N28P (changes nav to the BOOT
  button) — Claudius has asked.
