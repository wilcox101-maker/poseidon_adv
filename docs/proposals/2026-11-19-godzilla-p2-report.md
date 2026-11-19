# GODZILLA — P2 report (fast track complete)

**From:** GODZILLA. **To:** Claudius Maximus, Dale. 2026-11-19 04:00 ET. Closes the fast-track
run in [`2026-11-19-godzilla-fast-track.md`](2026-11-19-godzilla-fast-track.md). Nothing in P2 has
run on hardware yet: every unit is CI-built and its pure logic is native-tested, no more.
GODZILLA now stops; P3 and P4 wait for Dale.

## What landed
| Step | PR | State |
|---|---|---|
| P2a pin map | #65 | merged |
| P2a I2C scan + PSRAM config | #67 (as #70) | merged |
| ESP-NOW send-complete wait | #69 | merged |
| P2a ST77916 display + LVGL 9.2.2 status screen | #71 | merged |
| P2a PCF85063 RTC | #72 | merged |
| P2a CST816S touch | #73 | merged |
| P2b radar | #75 | merged |
| P2c pocket-watch face | #77 | merged |

The node boots to the radar. Swipe left for the watch, again for status, again for the radar.

## Decisions worth knowing (each is recorded in its PR)
- **Display:** vendored Espressif `esp_lcd_st77916` plus both Waveshare init tables; the panel
  revision is picked from register 0x04. QSPI at 40 MHz (the vendor demo runs 80).
- **Touch:** the bus is read only after the INT edge or while a finger is down, because the
  CST816S sleeps and NACKs. Orientation defaults to 1:1.
- **Radar:** fed at publish time, so it shows exactly what went on the wire. A dot's bearing is a
  hashed layout slot, not a direction.
- **Watch:** software clock resynced from the RTC about once a minute. Long-press sets the time
  (hours and minutes only), so no wire change was needed. No date on the dial. Numerals use the
  Montserrat 20 already built: no new font asset.
- **Screens:** swipe ring; the boot screen is `GZ_UI_HOME` in `gz_ui.cpp` (radar).

## Dale's bench — cumulative, one sitting
`pio run -e godzilla -t upload`, serial 115200.

Boot log
1. `PSRAM: ~8192 KB`.
2. I2C scan lists `0x15` (touch) and `0x51` (RTC).
3. `LCD: reg04 = .. .. .. .. -> panel rev N` — **send those four bytes**.
4. `touch: CST816S (id 0xB4, fw 0x..) at 0x15, INT=4`.
5. An RTC line: either the time, or "oscillator was stopped — seeded ... (build)".

Radar (boot screen)
6. Dots appear within about two seconds of anything nearby transmitting; an AirTag or Tile is amber.
7. Tap a dot: the card opens. Tap again: it closes.
8. The sweep line moves without tearing.

Watch (swipe left)
9. The second hand ticks once a second without tearing; numerals don't crowd (VII, VIII); the
   bow at the top is not clipped by the glass.
10. Long-press, set the time against a phone (upper half = hour, lower half = minute), long-press
    again. Serial prints `watch: set HH:MM:00 -> RTC written`; "hold to set" disappears.
11. Enter set mode and leave it: it exits after 15 s with the time unchanged.
12. Power-cycle: is the time kept? (Depends on whether the board has an RTC backup cell.)

Status (swipe left again)
13. Counters tick; BLE and Wi-Fi "sent" match the Cardputer's C5 -> x "GODZILLA dets" frames.
14. The subtitle shows the clock. The bottom line shows the touch chip and last point: top-left
    of the glass reads near `0,0`, bottom-right near `359,359`.

Cardputer
15. C5 -> Diagnostics lists peer `GODZILLA`.

## If something is off
| Symptom | One-line fix |
|---|---|
| Touch axes flipped | `GZ_TOUCH_SWAP_XY` / `_MIRROR_X` / `_MIRROR_Y` in `gz_touch.h` |
| Blank or garbled panel | the reg04 bytes pick the init table: `gz_st77916_rev.h` |
| Holding a finger never long-presses | CST816S may stop reporting a held finger: needs a driver change, tell GODZILLA |
| Slow or tearing hand moves | swap the 60 tick lines for one `lv_scale`, or raise QSPI from 40 MHz |
| Want a different boot screen | `GZ_UI_HOME` in `gz_ui.cpp` |

## Open items for Dale or Claudius
- **Time sync from the Cardputer** would be a new wire message type. Not started; needs asking
  first, per the orders.
- **Serif numerals** need a font asset (OFL, subset). Skipped for now; Dale's call after seeing
  the face.
- **The hourly repo-watch scheduled task** (`trig_01766qCYkWnF2DUZmg3qcnVQ`) is disabled and its
  prompt is stale. Leave it off, or rewrite it before re-enabling.
- P3 (labs: IMU, SD, audio) and P4 (other faces, themes) are not started.
