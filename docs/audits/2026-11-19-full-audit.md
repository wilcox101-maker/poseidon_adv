# Poseidon ADV — full audit (fork · features · sanity)

**By:** Claudius Maximus (overseer), at Dale's request, 2026-11-19.
**Scope:** the fork vs upstream; a feature inventory of the Cardputer app and the
TX-arm gate; a sanity pass on the wire, versions, tests and docs. Read-only review;
this PR adds only this document. Every claim cites `file:line` at the commit audited
(`master` at the time of the PR). Findings marked **(verify on bench)** are not
checked on hardware.

## Verdict

Healthy. The fork is clean and additive, the wire protocol is internally
consistent and test-pinned, and the TX-arm safety gate is real and correctly
enforced on the menu-launch path. The issues below are documentation drift and a
set of **transmit paths that remain reachable while SAFE** — most already
acknowledged in `docs/tx-arm.md`, a few not. None is a regression from this
session's work.

---

## 1. Fork audit

- Fork of [`Evil0ctopus/poseidon_adv`](https://github.com/Evil0ctopus/poseidon_adv),
  MIT licensed, `LICENSE` present on both sides.
- **Synced from upstream `cc0b058`** (2026-10-03). Upstream has not advanced since
  (its newest branches are 1 commit ahead and unmerged here, by design).
- **Divergence: +195 / −0** — entirely additive, no upstream commits reverted.
- No secrets in the tree (scanned for key/token/PEM patterns; `tools/check.py` in
  the assistant repo and CI both guard this).
- Build pins are deliberate and documented in `docs/fork-status.md`: pioarduino
  `55.03.38`, Bruce's `libnet80211` libs, LVGL `9.2.2` (GODZILLA), RadioLib `7.4.0`.
  The C5 `libnet80211` binary patch is a tool (`tools/patch_libnet80211.sh`),
  applied to the toolchain, not vendored in-tree — correct.

**Finding F1 (doc drift, low).** `docs/fork-status.md:5` reads
`+109 / -0 · updated 2026-11-16`. Actual is **+195** — the GODZILLA node (P1–P2)
landed since. The ledger's "Divergence units" table also predates the
`godzilla_node/` unit. Refresh the counts and add the node as a unit.

## 2. Feature audit

**Inventory.** The menu is a `menu_node_t` tree in `src/menu.cpp` (root
`menu.cpp:1056-1142`), walked by both the terminal (`menu.cpp:1305`) and carousel
(`menu_carousel.cpp:57`) renderers, both dispatching through
`menu_execute_action()`. ~164 leaf-action slots, ~159 distinct `feat_*` functions
(a few are wired at two slots: `feat_wifi_connect`, the four finder/tracker Hunt
entries). 178 `feat_*` symbols are defined under `src/`. Categories: WiFi (21),
Bluetooth (18), IR (5 + 10 pranks), Network (15 + 6 attacks), BadUSB (8), nRF52 (8),
Radio = LoRa (8) / Sub-GHz (9) / nRF24 (6) / GPS+SATCOM, C5 nodes (16), Tools (9) +
Hunt (4), System (12), plus root tools (Argus, MIMIR, PC Bridge, Mass Storage,
KERBEROS, SaltyJack). SaltyJack is a separate RaspyJack-style renderer (7 actions),
not `menu_node_t` leaves.

- **No dangling menu entries** — every `action` resolves to a definition.
- **No feature orphans** — the nine `features/*.cpp` with no `feat_*` symbol are
  helpers (BLE clone profile, KERBEROS internals, wardrive matrix, PMF warn, the
  WiFi sanity override).
- `feat_wifi_wardrive_hybrid` is defined (`wifi_wardrive.cpp:1149`) and
  forward-declared (`menu.cpp:65`) but wired to no menu node — reachable only
  internally. **(minor; dead menu-wise, intentional? — confirm)**

**TX-arm gate.** The gated set is `TX_ACTIONS[]` (`menu.cpp:1226-1242`), 32
function pointers; `action_is_tx()` (`menu.cpp:1244`) scans it and
`menu_execute_action()` enforces at `menu.cpp:1254`
(`if (action_is_tx(...) && !tx_arm_require()) return;`). Both renderers go through
this path, so the launch-time gate is real and complete for the listed features.
Persistence: NVS `ptx/armed`, default SAFE (`tx_arm.cpp:17-18`); the ARMED badge is
driven independently at `ui.cpp:107`. This all matches `docs/tx-arm.md`.

The gaps below are transmit paths that fire **without** passing that gate. The doc
already owns most of this in its "Known gaps"; the **new** items are F2, F3 and F6.

**Finding F2 (doc under-claims coverage, low).** Two functions are gated in code but
absent from the doc's list: `feat_triton` (Argus) and `feat_nrf52_wifi_ble_combo` —
both auto-fire broadcast deauth on `loopTask` and are deliberately in the table
(`menu.cpp:1238-1241`). Add them to `docs/tx-arm.md` so the doc matches the code.

**Finding F3 (ungated TX reachable while SAFE, medium).** Transmitting features that
are reachable from a menu but are **not** in `TX_ACTIONS` and not covered by the
doc:
- `feat_ap_signal_test` (WiFi → AP Signal Test, `menu.cpp:270`) — brings up a live
  `POSEIDON-SIGTEST` SoftAP beaconing on ch 1/6/11.
- `feat_nrf52_mitm` (`menu.cpp:1037`), `feat_nrf52_scout_strike` (`menu.cpp:1045`),
  `feat_nrf52_ble_mitm_relay` (`menu.cpp:1041`) — the nRF52 strike/MITM/relay trio
  jams, disconnects and relays. nRF52 is absent from `docs/tx-arm.md` entirely.
- `feat_lora_beacon` (Radio → LoRa → Beacon, `menu.cpp:845`) — keys the SX1262 with
  a POSEIDON ping every 3 s. Benign range-test TX, but it does transmit.
- Mesh/ESP-NOW TX: `feat_mesh`, `feat_mesh_chat`, `feat_mesh_page`,
  `feat_mesh_position`.

**Finding F4 (in-feature attack hotkeys bypass the gate, medium — doc gap #1).**
The leaf TX functions contain no `tx_arm_require()` of their own, so any key that
calls one directly skips the gate. Confirmed live bypasses:

| File:line | Key | Transmits (ungated) |
|---|---|---|
| `features/wifi_scan.cpp:259` | `D` | `feat_wifi_deauth()` / `c5_deauth_dashboard(a,false)` |
| `features/wifi_scan.cpp:267` | `X` | `feat_wifi_deauth_broadcast_ap(a)` / `c5_deauth_dashboard(a,true)` |
| `features/ble_scan.cpp:658` | `G` | `feat_ble_gatt()` |
| `features/ble_scan.cpp:659` | `C` | `feat_ble_clone()` |
| `features/ble_scan.cpp:660` | `H` | `feat_ble_hid()` |
| `features/ble_scan.cpp:661` | `X` | `feat_ble_flood()` |
| `features/ble_scan.cpp:662` | `P` | `feat_ble_spam()` |
| `features/ble_scan.cpp:663` | `W` | `feat_ble_whisperpair_from_target()` |

`docs/tx-arm.md:38` already names this as known-gap #1. Counter-point: the gap is
**partial, not total** — the client-hunt screens *do* gate their D/X deauth
(`wifi_clients.cpp:268,578`, `wifi_clients_all.cpp:362,367`) and the PMKID HUNT
toggle self-gates (`wifi_pmkid.cpp:687`), so passive capture runs while SAFE and
only the active deauth is blocked.

**Finding F5 (IR TX ungated, low — doc gap #2).** No IR function is in `TX_ACTIONS`;
`tx_arm.h:6` still *claims* "IR pranks" are covered, which is aspirational. The doc
body (`tx-arm.md:42`) correctly lists this as known-gap #2. Fix the stale claim in
the header comment.

**Finding F6 (gated function dodged by an alternate symbol, medium).**
`feat_ble_whisperpair` **is** gated (`menu.cpp:1232`), but `ble_scan.cpp:663`'s `W`
key calls `feat_ble_whisperpair_from_target()` (`ble_whisperpair.cpp:780`) — a
second entry point to the same attack that is **not** in the table. So even a
gated feature has an ungated path. Worth gating the `_from_target` entry too.

## 3. Sanity check

- **Wire version consistent:** `POSEI_VERSION == C5_VERSION == 5`
  (`proto.h:36`, `c5_cmd.h:19`); firmware `0.9.2` (`version.h:10`,
  `platformio.ini:37`). `tools/check_release_consistency.py` passes.
- **Command/response enum parity:** the S3 (`C5_TYPE_*`, `c5_cmd.h`) and C5
  (`POSEI_TYPE_*`, `proto.h`) enums agree **30/30** on value (including the
  `CMD_HS` ↔ `CMD_HS_CAPTURE` rename). **Finding F7 (test gap, low):** no native
  test includes `c5_cmd.h`, so this mirror is maintained by hand and comment
  (`c5_cmd.h:9`), not enforced by CI. The GODZILLA detection path, by contrast,
  *is* cross-pinned: `test_godzilla_det` asserts
  `sizeof(posei_wifi_det_entry_t) == GZ_DET_ENTRY_SZ` and the type codes across
  both headers (`test/test_godzilla_det/test_main.cpp:58-63`). A small
  `test_c5_cmd_parity` would close the older surface the same way.
- **Tests:** 31 native suites under `test/`; `test_proto` pins header layout,
  offsets and sizes (`test/test_proto/test_main.cpp:25-99`). CI builds all seven
  envs incl. the C5 IDF build and guards the committed partition table.
- **Owed on hardware (unchanged, not a new finding):** the v5 flag day has never
  run on silicon (`docs/hardware-validation.md`, POS-AUDIT-127), and **no GODZILLA
  P2 screen has either** — all CI-built and host-tested only.

**Finding F8 (cosmetic, low — from the P2 screen renders).** Three UI nits, no
functional impact: the Cardputer GODZILLA-dets mini-radar icon overdraws the first
row's hit count; the watch set-mode hands cross the "POSEIDON" wordmark; the radar
detail card reads "1 hits" (should be "1 hit"). **(verify on bench)** — rendered
from code, not yet seen on a panel.

## 4. Suggested follow-ups (Dale's call; none done in this PR)

1. Refresh `docs/fork-status.md` (F1) and reconcile `docs/tx-arm.md` with the code
   (F2, F5). *Docs only — safe.*
2. Decide the SAFE-gate policy for F3/F4/F6: at minimum gate `feat_ap_signal_test`,
   the nRF52 strike trio and `feat_ble_whisperpair_from_target`; ideally gate at
   each TX function's transmit point so in-feature hotkeys are covered too. *Needs
   the on-target build loop to confirm passive paths still work.*
3. Add `test_c5_cmd_parity` (F7). *Native test — safe.*
4. Fold F8 into the next GODZILLA UI PR.

Branch hygiene (observation only): 82 remote branches, 35 `godzilla-*`, most merged.
Deletion is Dale's and the cloud proxy blocks it anyway.
