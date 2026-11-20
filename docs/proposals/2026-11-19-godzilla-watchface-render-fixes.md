# GODZILLA — watch-face render fixes (from the real renders)

**From:** Claudius Maximus, after rendering the merged Classic (#116) and Command (#119) faces on
all three boards. Two refinements to already-merged units; one small PR each, native-test the
geometry, note "ready to re-render". Do these after the current in-flight unit, before unit 5.

## Fix A — Command pods clip on the narrow ES3C28P (240 px)
The four complication pods sit at the screen corners. On the 240×320 ES3C28P the corner pods run
off the left/right edges (NODE, CONTACT, PUBLISH, CHAN are all cut). The round-safe placement that
works for the 360 round and the 410 AMOLED is too wide for the narrow rect.
- Make pod placement fit the board rect: inset the pods to the usable width (and/or shrink the
  radar radius) so every pod is fully inside the screen on all three boards. Keep it driven by
  `gz_geom` so there's one source.
- Native-test: for each board shape, assert each pod's bounding box is fully within
  `[0,GZ_LCD_WIDTH] × [0,GZ_LCD_HEIGHT]`. (The capability fallback is already correct — the no-IMU
  ES3C28P shows CHAN in place of TILT. Keep that.)

## Fix B — Classic realism (Dale asked for "more realistic")
The merged Classic is a clean pocket watch but below the approved mockup
(https://claude.ai/artifact/CTaHugJu3fsYKkSqtvpbss, Classic face). Raise it toward the mockup:
- **Printed sub-scales:** the month subdial gets a ring of month abbreviations, the day subdial a
  ring of SUN..SAT, the date subdial ticks with numerals at 5/10/15/20/25/31 — each with the blued
  pointer to the current value (not just a bare label + hand).
- **Quarter lozenges** on the minute track at 12/3/6/9, and five-minute bars.
- **Richer moon:** the twin-aperture look — night-blue sky with a few gold stars and the scalloped
  cream mask, phase from the existing `gz_moon` calc.
- **Breguet numerals** are OPTIONAL: true Breguet shapes need a custom LVGL font (a bigger lift).
  Stock numerals are acceptable; only add a custom font if it's cheap. Flag it if you attempt it.
Keep everything behind `GZ_BOARD_HAS_RTC` as now, and lay out via `gz_geom`. Native-test the
sub-scale index math (month/dow/date → pointer angle).
