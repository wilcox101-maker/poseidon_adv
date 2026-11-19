# GODZILLA — advice for the P2c pocket-watch face

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19 03:25 ET, at Dale's request ("Assist
Godzilla"). Context: P2a is complete (#71 display, #72 RTC, #73 touch) and P2b radar is merged
(#75, merge commit — your next branch needs no rebase). This is advice, not new orders: the
orders are still #63 §P2c and the fast track. Items marked *verify* are not checked on the board.
Where you disagree, decide and say why in the PR.

## 1. Keep time in one place, off the I2C bus
- **Don't read the PCF85063 per frame.** Read it once when the watch screen is built, then run a
  software clock from `millis()` (unsigned deltas, wrap-safe). Re-read the RTC about once a
  minute and when the screen becomes visible, and correct the software clock from it.
- All RTC reads for the face happen **on the LVGL task**. `Wire` is mutex-guarded, and touch
  already reads there; the main task's `gz_rtc_begin()` runs before `gz_ui_start()`, so there is
  no overlap at boot.
- A failed read keeps the software clock running and shows a small "RTC?" mark; never freeze or
  blank the hands.

## 2. Pure, native-tested core: `gz_watch.h`
Same pattern as `gz_radar.h` (stdint/math only, C and C++), with `test/test_godzilla_watch`:
- **Hand angles in tenths of a degree** (LVGL's unit): second = `s*60`, minute = `m*60 + s`,
  hour = `(h%12)*300 + m*5`. Test 00:00:00, 03:00, 06:30, 11:59:59, 12:00, 23:59:59.
- **Software clock:** `gz_watch_advance(base_datetime, elapsed_ms)` with second/minute/hour/day
  rollover; reuse `gz_rtc_valid()` and the leap logic already in `gz_rtc.h` rather than a second
  calendar. Test a rollover across midnight and across a month end.
- **Numeral and tick positions:** reuse `gz_radar_xy()` (0° = 12 o'clock, clockwise) — move it to
  a shared `gz_geom.h` if you would otherwise include the radar header from the watch.
- **Set-mode state machine** (§4) as a pure function of (state, tap zone) -> new datetime.

## 3. Rendering on a 40 MHz QSPI panel
- **Tick the second hand at 1 Hz; do not sweep it.** A period watch ticks anyway, and each move
  invalidates only two thin strips. Refresh the face when the second changes, not on the 250 ms
  UI timer.
- Hands as `lv_line` objects positioned by bounding box, exactly as the radar sweep does it
  (`gz_ui_radar.cpp`), so only the hand's own strip redraws. Hour and minute hands: wider line
  plus a short counter-tail; a small centre cap object on top.
- **Static dial built once:** bezel rings, 60 minute ticks (12 heavier), numerals. *Verify* that
  ~80 static line/label objects keep a hand move under one frame at 40 MHz; if not, the fallback
  is `lv_scale` in round mode for the ticks (one object) — it is in LVGL 9.2.2, no new library.
- Keep everything inside the glass: outer bezel radius <= 176 px, numerals on about r = 138.

## 4. "Functional" needs a way to set the time (no wire change)
- The build-time seed is only right for a firmware built on Dale's PC and flashed at once. **CI
  artifacts are built in UTC**, and any image flashed later starts late. A time-sync message from
  the Cardputer is still a later wire addition (ask first).
- Recommended now: **long-press on the watch face enters set mode.** Tap the upper half = hour
  +1, lower half = minute +1 (seconds zeroed on each change), long-press again = write with
  `gz_rtc_write()` and leave; 15 s without a tap = leave without writing. Show "SET" and blink
  the hand being changed. This is I2C to a soldered part: passive under the project rules.
- Until the time has been set by hand, mark it: if `gz_rtc_begin()` reported the oscillator-stop
  seed this boot, show a small "set time" hint on the face. Expose that flag from `main.cpp` via
  `gz_stats` (already the main-task -> UI channel) rather than a new global.

## 5. Look (Dale's spec: old-fashioned pocket watch)
- **Roman numerals with the fonts already built** (Montserrat 20/28: I, V, X exist) are enough
  for the first PR. A serif face is a real improvement but is a new asset: if you add one, use an
  SIL-OFL font, subset it to the glyphs used with `lv_font_conv`, commit the generated C file,
  and cite the font, licence and exact command in the PR. No font file without a stated licence.
- Bezel and fob from primitives (rings, arcs, short lines): a double bezel ring, a crown and bow
  at 12 o'clock inside the glass, a small seconds-free dial is fine. Brass/ivory on the deepwater
  background, or a light dial — your call; state the palette values in the file header as
  `gz_ui_radar.cpp` does. **Original artwork only**; do not trace a real watch face.
- Draw IIII or IV as you prefer; say which. (Traditional dials use IIII.)

## 6. Navigation with three screens
- Make the swipe a **ring**: left = next, right = previous, order radar -> watch -> status. Keep
  the "a swipe never also counts as a tap" guard from #75.
- Home screen stays the radar unless Dale says otherwise; put the choice behind one constant
  (`GZ_UI_HOME`) so it is a one-line change after the bench.
- Only the visible screen refreshes (as now). When the watch becomes visible, resync from the RTC
  first so the hands are right on the first frame.

## 7. PR checklist
Passive only; no `src/*`, C5, wire or rule-file edits; no new library; exact pins unchanged; CI
green incl. `godzilla`; cumulative "Dale's bench" list. New bench lines to include: hands match
a phone clock after setting; time survives a power cycle (with and without the backup cell — say
which the board has, *verify*); second hand ticks once a second without tearing; long-press
set mode works and times out; swipe ring reaches all three screens.

After P2c: the short report PR, then stop — P3/P4 wait for Dale.
