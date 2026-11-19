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
- **Always-visible cue:** a filled red `ARMED` badge (white on red) in
  the status bar plus a red baseline, shown on every screen whenever
  armed — independent of radio/extra/C5 content. SAFE shows neither.
- **The gate:** `tx_arm_require()` shows a brief notice and returns false
  when SAFE. `menu_execute_action()` calls it before launching any action
  in the offensive table, so both terminal and carousel menu styles are
  covered.

## Gated actions

WiFi: deauth, deauth-all, AP clone, evil-twin, beacon spam, karma, CIW,
portal, **AP Signal Test** (SoftAP beacon). BLE: spam, HID, beacon, clone,
flood, karma, sourapple, findmy, blueducky, whisperpair. Sub-GHz: jammer,
broadcast, replay, bruteforce. nRF24: jammer, mousejack, BLE-spam. C5:
deauth-5G, nuke-5G, beacon spam, CIW, AP clone, karma. **nRF52:** MITM,
MITM-Relay, Scout+Strike, WiFi+BLE combo. **Argus** (auto-deauth).

Passive features (scan, sniff, detect, spectrum, finder) are **not**
gated.

**In-feature attack hotkeys are gated too.** The deauth/clone/portal keys on
the WiFi-scan detail screen (`D`/`X`/`C`/`P`) and the active BLE keys on the
BLE-scan detail screen (`G`/`H`/`X`/`P`/`C`/`W`, including the
`whisperpair_from_target` entry point) call `tx_arm_require()` at their own
transmit point, so they refuse while SAFE. Passive PMKID capture, client-hunt
and the RSSI tracker still run while SAFE — only their active deauth sub-steps
self-gate.

## Known gaps (follow-ups)

1. **IR TX** (TV-B-Gone, remote, clone, the IR pranks) is not gated —
   low-power, non-RF. Add the IR entry points to the gate if wanted.

## Verification status

- `pio test -e native-test`: passes (native env excludes `src/`, so it
  does not exercise this code).
- `pio run -e cardputer`: **not run** — the ESP32 toolchain install
  can't complete in the cloud sandbox. **Must build on the Windows box
  before merge**, then sanity-check: arm/disarm from Settings, confirm
  the red status cue, confirm a gated feature refuses to fire while SAFE
  and runs once armed.
