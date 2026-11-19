# GODZILLA node — three-board bench runbook

One page per board: how to flash it, what the boot log should print, what to check on the
display, touch and SD, and what the status screen should read. Post-port orders #98 §3.6.

| Board | Env | Status |
|---|---|---|
| 2.8" ESP32-S3 Display **ES3C28P** (lcdwiki) | `godzilla-es3c28p` | **Bench-ready**: Dale has it |
| Waveshare ESP32-S3-Touch-LCD-**1.85B** | `godzilla` | Desk-only until in hand |
| Waveshare ESP32-S3-Touch-**AMOLED-2.06** | `godzilla-amoled206` | Desk-only until in hand |

All three are ESP32-S3 N16R8 builds from one tree (pioarduino 55.03.38, Arduino core, LVGL
9.2.2), selected at build time by `-DGZ_BOARD_*`. Everything here is **passive**: the node
receives BLE and Wi-Fi management frames and publishes over ESP-NOW. Nothing on the bench
list transmits anything else.

Record each result in the PR or issue that asked for it, as **pass**, **fail + what you
saw**, or **n/a**. A boot log pasted from the serial monitor is the best evidence.

---

## Common to every board

### Flash and monitor

```sh
pio run -e <env> -t upload          # build + flash over the board's USB-C
pio device monitor -e <env>         # 115200, exception decoder on
```

`ARDUINO_USB_CDC_ON_BOOT=1`, so the log comes over the native USB port. If the port doesn't
appear, or the upload can't connect, put the S3 in download mode: hold **BOOT**, tap
**RST**, release **BOOT**, then upload again. Press **RST** after flashing to see the boot
log from the start.

### Boot log, in order

```text
  PSRAM: 8192 KB                       <- "!! PSRAM NOT DETECTED" is a hard fail
  I2C scan (SDA=.. SCL=..):            <- per board, below
  RTC ... / RTC: none on this board    <- per board, below
  LCD: ... up (backlight off until first frame)
  touch: ... at 0x.., ...
  SD: card mounted, N MB (...)  |  SD: no card (insert one, then long-press the radar)
  UI: LVGL 9.2.2, 2 x N B draw buffers, radar + hunt + BLE lab + [watch + ]status screens

POSEIDON GODZILLA node — P3: ...
  board: <GZ_BOARD_NAME>
  link: WiFi STA ch1, ESP-NOW, broadcast peer
  HELLO '<name>' role=.. every .. ms
  BLE: passive scan, publish every .. ms
  Wi-Fi: promiscuous mgmt, hop 1..13 @ .. ms, publish every .. ms
  passive only — capture is receive-only; publishes return to ch1
```

A line starting with `!!` is a failure. Copy it as it appears.

### Checks every board shares

1. **No white flash** at power-on. The backlight comes on only after the first frame.
2. **Swipe ring.** Swipe left/right through radar → hunt → BLE lab → [watch →] status and
   back. A swipe never also counts as a tap.
3. **Radar.** Contacts appear within a few seconds near any phone or laptop. Tap a dot to
   open its card; long-press the card to hunt it.
4. **Hunt.** The dBm number rises as you walk toward the device, and the trace scrolls.
   Long-press stops the hunt.
5. **BLE Lab.** Rows fill strongest first. Tap a row for the decoded detail, tap again to
   go back. A BLE keyboard or mouse shows in teal with "announces HID".
6. **Dim and wake.** After 60 s idle the panel dims; a touch brings it back (on IMU boards,
   so does motion).
7. **Recon.** With a FAT32 card in, long-press the radar: the log shows
   `recon: session started -> /...`. Long-press again to stop, then check the file on a PC.
8. **Cardputer side.** The node's HELLO name appears on the Cardputer, and detections arrive.

### Status screen, every board

```text
<HELLO name>
POSEIDON node | hh:mm:ss        (boards with an RTC, once set; else "POSEIDON detection node")
up 0m12s | ch 1
BLE  N sent | n last
Wi-Fi  N sent | n last
last publish 1s ago
heap N KB | PSRAM 8192 KB
I2C n dev | <panel>
<touch chip> | <gesture> x,y      (updates as you touch)
[IMU still|moving | rot 0..3]      (IMU boards)
[batt ...]                          (boards with a power readout)
```

---

## 1. ES3C28P — 2.8" ESP32-S3 Display, touch SKU · **bench-ready**

Env `godzilla-es3c28p` · HELLO `GODZILLA28` · ILI9341V 240x320 over SPI @ 40 MHz ·
FT6336G touch · SDMMC 4-bit TF · no RTC, no IMU, no PMIC · battery on an ADC pin.

### Expected I2C scan (SDA 16, SCL 15)

```text
    0x18  ES8311 codec? (unused)     <- address still to confirm
    0x38  FT6336G touch              <- Dale-confirmed
    2 device(s); expect 0x38 touch
```

### Expected boot lines

```text
  RTC: none on this board — clock starts unset
  LCD: ILI9341V 240x320 SPI @ 40 MHz up (backlight off until first frame)
  touch: FT6336G touch at 0x38, id reg 0xA3 = 0x.., INT=17
  SD: card mounted, N MB (SDMMC 4-bit)
  board: 2.8in ESP32-S3 Display ES3C28P
  HELLO 'GODZILLA28' ...
```

### Checks

| # | Check | Pass looks like | If not |
|---|---|---|---|
| E1 | Touch address | `0x38 FT6336G touch` in the scan | Confirmed already; re-check only if it ever goes missing |
| E2 | FT6336G **id byte** | Note the value printed for `id reg 0xA3` | It's logged only; report the byte so it can be pinned |
| E3 | **Colours** | Teal headings, red surveillance dots, amber trackers | Red and blue swapped: the BGR element order. Image looks like a negative: the inversion-on command (`0x21`) |
| E4 | Image fills the screen | Edge to edge, upright, portrait | Report which edge and how far off |
| E5 | Touch axes | The status screen's x,y track your finger (0,0 top-left) | `GZ_TOUCH_SWAP_XY` / `_MIRROR_X` / `_MIRROR_Y` |
| E6 | **Speaker silent** | No hiss or pop at boot or after | Amp-enable (IO1) is driven high (off); report it |
| E7 | **ES8311 address** | Does `0x18` ACK? | Report the address it answers on, or "absent" |
| E8 | SD / Recon | `SD: card mounted ... (SDMMC 4-bit)`; a Recon file on the card | Report the `!!` line |
| E9 | **Battery divider** | Status reads `batt ~V`. Meter the cell: reading ≈ cell voltage | The divider is assumed 2:1 (R14/R15 200k/200k). Report both numbers |
| E10 | USB only, no cell | `batt: not measured`, or a value you can explain | Report the value shown |
| E11 | BLE Lab layout | 10 rows, readable; taps land on the row you aimed at; detail page not clipped by a long name | Report the row it picked, or a photo |
| E12 | Ring | radar → hunt → BLE lab → status (no watch on this board) | — |

**Time:** this board has no RTC. The clock starts unset each boot ("hold to set"), so
**it does not keep time across a power cycle**. That's expected until the Cardputer
time-sync wire message, which is Dale's call (#98 §4).

---

## 2. 1.85B — Waveshare ESP32-S3-Touch-LCD-1.85B · desk-only until in hand

Env `godzilla` · HELLO `GODZILLA` · ST77916 360x360 round, QSPI @ 40 MHz · CST816S touch ·
SDMMC 4-bit TF · PCF85063 RTC · QMI8658 IMU · BQ27220 gauge (fitted, not driven).

### Expected I2C scan (SDA 11, SCL 10)

```text
    0x15  CST816S touch              <- may not ACK while asleep; the touch line decides
    0x18  ES8311 codec? (unused)     <- inferred
    0x40  ES7210 mic ADC? (unused)   <- inferred
    0x51  PCF85063 RTC
    0x55  BQ27220 battery gauge (unused)
    0x6B  QMI8658 IMU
    n device(s); expect 0x15 touch + 0x51 RTC
```

### Expected boot lines

```text
  RTC PCF85063: yyyy-mm-dd hh:mm:ss    (or "oscillator was stopped — seeded ... (build)")
  LCD: reg04 = .. .. .. .. -> panel rev N
  LCD: ST77916 360x360 QSPI @ 40 MHz up (backlight off until first frame)
  touch: CST816S (id 0x.., fw 0x..) at 0x15, INT=..
  SD: card mounted, N MB (SDMMC 4-bit)
```

### Checks

| # | Check | Pass looks like | If not |
|---|---|---|---|
| B1 | Panel revision | `panel rev` is a known value and the image is clean | `!! LCD: unknown panel revision` → report the four reg04 bytes |
| B2 | Image centred on the round glass | No ring cut off at any edge | Report which edge |
| B3 | Touch axes | Status x,y track your finger | `GZ_TOUCH_SWAP_XY` / `_MIRROR_*` |
| B4 | IMU | `IMU still`, then `moving` when shaken; `rot` follows the board | `IMU: not found` → confirm 0x6B in the scan |
| B5 | Auto-rotate | Turn the board a quarter: the UI follows, and touch still lands right | Report the angle and what happened |
| B6 | Codec / mic ADC addresses | Do 0x18 / 0x40 ACK? | Report the addresses seen |
| B7 | **RTC keeps time** | Set the time, power off for a minute (battery connected), power on: the time is right | `oscillator was stopped` on every boot means no backup on this unit. Report with and without the battery |
| B8 | SD / Recon | As common check 7 | Report the `!!` line |
| B9 | BLE Lab layout | 9 rows, all inside the glass; taps land right | Photo |

Power line: none on this board yet (the BQ27220 isn't driven).

---

## 3. AMOLED-2.06 — Waveshare ESP32-S3-Touch-AMOLED-2.06 · desk-only until in hand

Env `godzilla-amoled206` · HELLO `GODZILLA206` · CO5300 410x502 AMOLED, QSPI @ 40 MHz,
column gap 22 · FT3168 touch · SPI TF @ 20 MHz · PCF85063 RTC · QMI8658 IMU · AXP2101 PMIC
(read-only).

### Expected I2C scan (SDA 15, SCL 14)

```text
    0x18  ES8311 codec? (unused)     <- inferred
    0x34  AXP2101 PMIC (unused)      <- inferred, confirm
    0x38  FT3168 touch
    0x40  ES7210 mic ADC? (unused)   <- inferred
    0x51  PCF85063 RTC
    0x6B  QMI8658 IMU
```

### Expected boot lines

```text
  RTC PCF85063: yyyy-mm-dd hh:mm:ss
  LCD: CO5300 410x502 AMOLED QSPI @ 40 MHz up, col gap 22 ...
  touch: FT3168 touch at 0x38, id reg 0x.. = 0x.., INT=..
  SD: card mounted, N MB (SPI)
```

### Checks

| # | Check | Pass looks like | If not |
|---|---|---|---|
| A1 | **AXP2101 address** | It ACKs at `0x34` | Report the address seen |
| A2 | Image centred | No 22 px stripe on either edge | `GZ_LCD_COL_GAP` is the one-constant fix; report the stripe width |
| A3 | Colours | RGB correct | Red and blue swapped: the MADCTL BGR bit |
| A4 | Touch axes | Status x,y track your finger | `GZ_TOUCH_SWAP_XY` / `_MIRROR_*` |
| A5 | Dim / wake | Dims to 8 % after 60 s; a touch or motion restores it | Report |
| A6 | IMU + auto-rotate | As B4 / B5 | As B4 / B5 |
| A7 | Battery | Battery connected: `batt N% ~V | <charge state>`. USB only: `USB power, no battery` | Report the line |
| A8 | **RTC keeps time** | As B7 | As B7 |
| A9 | SD / Recon over SPI | `SD: card mounted ... (SPI)`; Recon file OK | If it fails at 20 MHz, report; it stays at 20 MHz until a clean run |
| A10 | Codec / mic ADC addresses | As B6 | As B6 |
| A11 | BLE Lab layout | 12 rows; taps land right | Photo |

The PMIC stays read-only. Enabling its gauge or ADC means writing PMIC registers, and that
is Dale's call.

---

## Standing confirmations

| Item | ES3C28P | 1.85B | AMOLED-2.06 |
|---|---|---|---|
| FocalTech touch at 0x38 | **Confirmed (Dale)** | n/a (CST816S 0x15) | To confirm |
| ES8311 codec address (0x18 inferred) | To confirm (E7) | To confirm (B6) | To confirm (A10) |
| Keeps time across a power cycle | **No**, no RTC | To confirm (B7) | To confirm (A8) |
| PMIC / gauge address | n/a | 0x55 (not driven) | To confirm 0x34 (A1) |
