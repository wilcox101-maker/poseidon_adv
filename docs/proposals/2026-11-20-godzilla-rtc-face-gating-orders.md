# GODZILLA order — no RTC, no clock faces (priority)

**From:** Claudius Maximus (overseer) · **To:** GODZILLA (node PSPE) · 2026-11-20
**Decision (Dale, 2026-11-20):** "No RTC no clock faces." A board without a real-time clock does
not offer clock faces at all — not a clock face with the time blanked, and no soft-clock fallback.

## Why this changes current behaviour
`gz_faces.h` today sets every face's cap requirement to 0 with the note "no face requires RTC —
RTC gating is per-element." That let the clock faces appear on the RTC-less ES3C28P showing
"set via Cardputer". Dale's rule reverses that: a clock face is gated out whole on a board with
no `GZ_CAP_RTC`.

## What to change (`godzilla_node/` only, one draft PR, native-tested)
1. **`gz_faces.h` caps table** — require `GZ_CAP_RTC` for the clock faces, leave Command at 0:
   - `CLASSIC` → `GZ_CAP_RTC` (antique analog dial — a clock)
   - `MODULAR` → `GZ_CAP_RTC` (digital time + slot grid — a clock)
   - `MINIMAL` → `GZ_CAP_RTC` (big time + status line — a clock)
   - `COMMAND` → `0` (detection radar + pods — not a clock; stays on every board)
2. **`gz_face_select` fallback** — the final/unreachable return must be a face that fits every
   board. `CLASSIC` no longer does; return `GZ_FACE_COMMAND` (caps 0) and fix the comment. On an
   RTC-less board the loop now lands on `COMMAND` as the default face.
3. **Customiser face picker** — the face row must cycle only faces that pass
   `gz_face_available(f, board_caps)`, so on an RTC-less board the only selectable face is Command.
   Make sure a stored `face` index that is now unavailable resolves through `gz_face_select` at
   render (never a blank screen).
4. **Rewrite the stale comment block** above `gz_face_caps[]` to state the new rule.
5. **Tests** — update `test_godzilla_faces` to the new truth and add cases:
   - on a no-RTC board (`CAPS_ES3`): `gz_face_available` is false for CLASSIC/MODULAR/MINIMAL,
     true for COMMAND; `gz_face_select` of any clock face → `COMMAND`; the garbage/overflow
     fallback on `CAPS_ES3` → `COMMAND` (not CLASSIC).
   - on an RTC board (`CAPS_185B`/`CAPS_206`): all four faces still available; requests honoured.

## Note
The per-element `GZ_COMP_CLOCK → GZ_CAP_RTC` complication gating stays as-is (a clock pod on a
non-clock face still hides without an RTC). This order is about whole clock *faces*, not pods.
Display change → note "ready to re-render".
