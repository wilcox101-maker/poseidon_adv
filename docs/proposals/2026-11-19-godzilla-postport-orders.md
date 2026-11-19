# GODZILLA — post-port work (next phase)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19, on Dale's "keep GODZILLA busy."
The three-board port is code-complete and screenshotted (1.85B, AMOLED-2.06, ES3C28P; three
green envs). This sequences what's next. Standing rules unchanged: passive only (no TX but the
existing ESP-NOW HELLO/publish), no `src/*`/C5/wire change, capability flags not stubs, pure
logic native-tested, one geometry source, CI green on every env, one topic per PR, merge commits
for stacked work. Claudius reviews/merges and renders; flag the §4 "ask Dale" items instead of
building them.

## 1. Polish — do first (small, high-value; the render pass found these)
1. **Radar detail card plural:** it reads "1 hits". Make it "1 hit" / "N hits" (same for any
   other count label that can be 1). Trivial; native-test the pluraliser if you add one.
2. **Hunt footer on 240 px (ES3C28P):** the "min/max/samples" line and the "heard … | hold to
   stop" line crowd and overlap at 240 px wide. Route their layout through `gz_geom.h` so they
   stack with the right spacing on a narrow screen (and stay as-is on the wider boards). Add a
   geometry native-test at 240×320.
3. **Screen auto-rotate / wake — finish it.** The status screen already reads the IMU orientation
   (`s_imu_rot`), but the UI is not actually rotated. On the two IMU boards, apply the quarter-turn
   with `lv_display_set_rotation` (hysteresis already in `gz_imu_rotation`), behind one constant so
   it's easy to disable. ES3C28P (no IMU) keeps a fixed orientation. Wake-on-motion/touch already
   exists (#97) — just wire the rotate.

## 2. Deferred P3 Labs (passive; resume now that the port is done)
4. **BLE Lab — passive viewer.** The GATT/service viewer and HID-report decoder over what the
   node already sniffs, reusing the pure decode logic by copying the header-only pure parts into
   the node tree (never editing `src/`). Nothing that connects, pairs or emulates — that's §4.
   Pure decode native-tested.
5. **Audio tones.** Boot/alert chirps over I2S on the ES8311. **Confirm the codec address on the
   bench first** (it's inferred on all three boards). Local output only, no RF. Optional — Dale's
   call whether v1 wants sound; propose, don't assume.

## 3. Bench runbook — the 2.8" is in-house
6. **One consolidated three-board bench runbook** (`docs/` doc, like the P2 report). Per board:
   flash command + env, the boot I2C scan lines to expect, the display/touch/SD bring-up checks,
   and what the status screen should read. Mark the ES3C28P checks as **bench-ready** (Dale has
   it) and the other two as **desk-only until in hand**. Fold in the standing confirmations: the
   FocalTech 0x38 address (Dale-confirmed for ES3C28P), the ES8311 address (still to confirm), and
   whether each board keeps time across a power cycle (RTC backup).

## 4. Reserved for Dale (don't start without a yes)
- **CSI motion monitor.** Net-new and the likely sticking point: passive CSI from ambient/RX is
  fine, but if it needs the node to *transmit* to sound the channel, that's a rule change — ask
  first, with the approach, before any code.
- **BLE connect / pair / emulate / clone**, **wardrive** (no GPS on any board), a **time-sync wire
  message** from the Cardputer, and **P4** (themes, other watch faces, Badge Studio, locksport).

Suggested order: 1→2→3 (polish, then the passive BLE viewer, then the runbook), audio and anything
in §4 only on Dale's word. Each PR lists its Dale-bench items.
