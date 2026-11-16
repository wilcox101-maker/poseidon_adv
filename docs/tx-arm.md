# TX-arm — master transmit safety switch

Resolves UI review finding #1: the menu fires a letter mnemonic
immediately, so a mis-key could launch a transmitting feature. TX-arm is
a global switch that must be deliberately armed before any offensive RF
transmission can start.

## Behavior

- **Default: SAFE.** State persists across reboots (NVS `ptx/armed`) — the
  device powers up as last left. To reset to SAFE on every boot instead,
  force `false` in `tx_armed()`'s first load (one line, noted in
  `tx_arm.h`).
- **Arm/disarm:** Settings → `[T] TX transmit`. Arming requires a
  confirm; disarming is instant.
- **Always-visible cue:** the status-bar separator turns red whenever
  armed, plus a `TX ON` badge when the status slot is free. *(Badge
  placement is provisional — verify/tune on hardware.)*
- **The gate:** `tx_arm_require()` shows a brief notice and returns false
  when SAFE. `menu_execute_action()` calls it before launching any action
  in the offensive table, so both terminal and carousel menu styles are
  covered.

## Gated actions

WiFi: deauth, deauth-all, AP clone, evil-twin, beacon spam, karma, CIW,
portal. BLE: spam, HID, beacon, clone, flood, karma, sourapple, findmy,
blueducky, whisperpair. Sub-GHz: jammer, broadcast, replay, bruteforce.
nRF24: jammer, mousejack, BLE-spam. C5: deauth-5G, nuke-5G, beacon spam,
CIW, AP clone, karma.

Passive features (scan, sniff, detect, spectrum, finder) are **not**
gated.

## Known gaps (follow-ups)

1. **In-feature attack hotkeys** (e.g. Scan → `D` deauth) call the TX
   functions directly, bypassing `menu_execute_action`. Covering them
   means gating at each function's transmit point — best done with the
   on-target build loop to confirm the passive paths still work.
2. **IR pranks** are not gated yet (low-power, non-RF). Easy to add to
   the table if wanted.

## Verification status

- `pio test -e native-test`: passes (native env excludes `src/`, so it
  does not exercise this code).
- `pio run -e cardputer`: **not run** — the ESP32 toolchain install
  can't complete in the cloud sandbox. **Must build on the Windows box
  before merge**, then sanity-check: arm/disarm from Settings, confirm
  the red status cue, confirm a gated feature refuses to fire while SAFE
  and runs once armed.
