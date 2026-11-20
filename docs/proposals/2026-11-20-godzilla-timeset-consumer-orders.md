# GODZILLA order — time-sync node consumer (step 3)

**From:** Claudius Maximus (overseer) · **To:** GODZILLA (node PSPE) · 2026-11-20
**Design:** `docs/proposals/2026-11-19-time-sync-design.md` (Dale greenlit the build 2026-11-19).
**Depends on:** wire type `POSEI_TYPE_CMD_TIMESET`=27 + `posei_timeset_t` (merged, #120) and the
Cardputer **sender** (#132). You can build and test the consumer before #132 merges — the wire
type is already on `master`.

## Goal
Let a node with an RTC that is unset or has drifted get the correct time from the Cardputer, so
its clock faces read true. Receive-only: nothing transmits, the passive rule holds.

> **Revised 2026-11-20 (Dale: "No RTC no clock faces").** This consumer is now **RTC-boards
> only** — set the hardware RTC from TIMESET. There is **no soft clock** and **no RTC-less path**:
> a board without an RTC has no clock faces (see the rtc-face-gating order), so there is nothing
> to seed. Build this only after the rtc-face-gating change lands.

## Scope — `godzilla_node/*` only
Do **not** touch `c5_node/*` or any `src/*` file (reading `c5_node/main/proto.h` for the POSEI
types is fine, as the node tree already does). One topic, one draft PR, native-tested.

## What to build
1. **Consume the frame.** In the node's ESP-NOW receive path (`gz_link.cpp` → `gz_on_recv`),
   add a case for `POSEI_TYPE_CMD_TIMESET`: validate `payload_len >= sizeof(posei_timeset_t)`,
   read `epoch` + `tz_offset_min`, and seed the node clock. Ignore a frame whose epoch fails the
   same validity floor the sender uses (reject a bogus/zero epoch).
2. **Seed `gz_clock`.** Convert `epoch` (+ `tz_offset_min`, display-only) to a `gz_watch_clock_t`
   and `gz_clock_publish()` it, so every consumer (watch UI, Recon writer) reads real time via
   `gz_clock_now()`, **and** set the hardware RTC. RTC boards only — a board with no RTC has no
   clock faces, so skip it entirely (no soft clock).
3. **Pure time math, native-tested.** Put the `epoch (+tz) → Y/M/D h:m:s (+ day-of-week)`
   conversion in a pure header (stdint only, no LVGL/ESP-IDF) and native-test it:
   known epochs → expected civil dates, a leap-year date, tz offset applied, and a rejected
   sub-floor epoch. Mirror the style of `test_godzilla_rtc` / `test_godzilla_faceclock`.
4. **Drop the Classic placeholder.** Once a TIMESET has been applied, the Classic face stops
   showing "set via Cardputer" and shows the real time/date/moon. Before any sync it keeps the
   placeholder. Gate on a "have we ever been time-set?" flag, not on RTC presence.

## Out of scope / reserved
No new transmit. No change to the wire type or struct (parity pins it). No `c5_node/*` edits —
if the TRIDENT C5 node also needs to consume TIMESET, that is a separate Claudius/PSPE task.

## Done when
Consumer merged, Classic face shows real time after a sync on all three boards (note "ready to
re-render" — Claudius re-renders), native tests green, all CI green.
