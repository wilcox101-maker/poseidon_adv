# GODZILLA — P3 (Labs) orders

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19, at Dale's "back to work on GODZILLA."
Dale merged the first P3 unit himself (#80, microSD + Recon session log), which opens P3. The
rules are unchanged from the fast track; this note only sequences the remaining Labs units and
flags what is net-new or needs Dale. Where you disagree, decide and say why in the PR.

## Standing rules (unchanged)
Passive only — nothing transmits but the ESP-NOW HELLO/publish already in P1. No `src/*` edits,
no C5 or wire-format change without asking Dale first. Pure logic gets a native test; one topic
per branch `claude/godzilla-<topic>`, merged with a **merge commit** when stacked (squash left
#73 conflicted). CI green incl. `godzilla`. Each PR lists what needs Dale's bench. Original
artwork only. Claudius reviews and merges on the 15-minute loop (re-armed now).

## Done
- **Recon / microSD** (#80): session CSV of the node's own detections, pure `gz_recon.h` + ring
  queue + SD task, `test_godzilla_recon`. Bench items are in that PR.

## Suggested order for the rest of P3
Smallest-risk, most-reuse first; each is its own PR.

1. **IMU bring-up — QMI8658 (0x6B, shared I2C bus).** Read-only: who-am-I, then accel/gyro. A
   pure `gz_imu.h` (register decode, scaling to g / dps, a tilt/that's-moving helper) with a
   native test, and the Arduino I2C read behind `#ifdef ARDUINO`, same split as `gz_rtc.h`. Label
   it on the boot I2C scan (the audit's F3 list already expects 0x6B). Surface it on the status
   screen (orientation / "still|moving") and expose a motion flag other units can read. No new
   library — raw I2C.

2. **Screen auto-rotate / wake from the IMU.** Once (1) lands: rotate the round UI to the held
   orientation and/or wake the backlight on motion. Small, visible, and it proves the IMU on the
   bench. Keep it one constant to disable.

3. **CSI motion monitor — net-new, the real P3 engineering.** Wi-Fi channel-state-information
   presence/motion sense. This is the one unit with no Poseidon parity to reuse, so scope it
   carefully: a pure detector core (variance/threshold over a CSI frame, native-tested with
   captured sample buffers) separate from the `esp_wifi_set_csi` plumbing. **Passive** (it reads
   CSI from ambient or from the node's own ESP-NOW RX — do not add a transmit just to sound the
   channel without asking Dale). Expect this to want its own advice note; open a short design PR
   first (approach + what it needs) before the implementation, as with P2.

4. **BLE Lab — evaluate before porting.** The brief says reuse Poseidon's `ble_gatt` / `ble_hid`
   / `hid_decode` / `ble_clone_profile` where parity holds. On the node these are `src/*` and
   off-limits, and most are **active** (GATT connect, HID) — which the node's passive rule
   forbids without Dale. So P3's BLE-Lab unit is **passive only**: a GATT/service *viewer* and
   HID-report *decoder* over what the node already sniffs, reusing the pure decode logic by
   copying the header-only pure parts into the node tree (not editing `src/`). Anything that
   connects or emulates is P4 / Dale-gated — list it, don't build it.

5. **Audio tones — ES8311 (0x18?) + speaker.** Lowest priority. Boot/alert tones over I2S. Needs
   the codec address confirmed on the bench first (audit F3: 0x18 is inferred). Optional; Dale's
   call whether v1 wants sound.

## Needs Dale (don't start without a yes)
- **Wardrive / GPS.** The brief's P3 list includes wardrive, but the 1.85B has **no GPS** — it
  needs the J8 UART GPS add-on. Until Dale confirms that hardware, skip wardrive; the Recon CSV
  already covers "what was seen" minus position.
- **Anything that transmits** to drive a feature (CSI sounding, BLE connect/emulate, audio is
  fine — it's local). Ask first; it's a rule change.
- **A time-sync wire message** from the Cardputer (still the open P2c item) — new wire type.

## P3 exit (per the brief §7)
IMU + Recon working, CSI presence demonstrated, BLE-Lab passive viewer, each native-tested and
CI-green, each with its bench list. Then a short P3 report PR and stop — P4 (themes, other
faces, Badge Studio, locksport) stays Dale-gated.
