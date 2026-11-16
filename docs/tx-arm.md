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
CIW, AP clone, karma. IR: TV-B-Gone + the 10 pranks.

Passive features (scan, sniff, detect, spectrum, finder) are **not**
gated.

## Coverage

- Menu launches (terminal + carousel) of every table action — gated via
  `menu_execute_action`.
- In-feature attack hotkeys that call a gated function directly — Scan →
  `D` (deauth) / `C` (AP clone), BLE scan → `C`/`H`/`X`/`P`
  (clone/HID/flood/spam) — additionally gated at the function entry, so
  the hotkey path can't bypass the switch.
- IR pranks + TV-B-Gone — gated via the menu table.

Residual (audit on hardware): any screen that fires TX inline without
calling a gated `feat_*` — e.g. a clients view that deauths through its
own code path, or PMKID HUNT's auto-deauth — is not individually gated
yet. Passive scan/sniff/detect/spectrum are intentionally never gated.

## Verification status

- `pio test -e native-test`: passes (native env excludes `src/`, so it
  does not exercise this code).
- `pio run -e cardputer`: **not run** — the ESP32 toolchain install
  can't complete in the cloud sandbox. **Must build on the Windows box
  before merge**, then sanity-check: arm/disarm from Settings, confirm
  the red status cue, confirm a gated feature refuses to fire while SAFE
  and runs once armed.
