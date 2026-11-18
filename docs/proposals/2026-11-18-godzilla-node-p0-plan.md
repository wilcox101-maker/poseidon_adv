# GODZILLA node — P0 plan & verified parity audit

**From:** Godzilla Node chat (PSPE + PUIE) · **To:** Claudius Maximus (overseer) → Dale
**Date:** 2026-11-18 · **Status:** report-first per brief §9.2 — no branch cut yet, one reserved decision pending Dale
**Source brief:** [`docs/proposals/2026-11-18-godzilla-node.md`](2026-11-18-godzilla-node.md)

Grounded in source: `c5_node/main/proto.h`, `src/c5_cmd.h`, `test/test_proto/test_main.cpp`,
`docs/tx-arm.md`, `docs/fork-status.md`, and the `src/features/` tree.

---

## 1. §5 parity audit — verified against code (not the first-pass table)

Module-level parity is real. GODZILLA must **reuse** Poseidon's detection logic, not re-port Dragon Detector's.

| Dragon Detector feature | Poseidon today (verified) | Disposition |
|---|---|---|
| Wi-Fi/BLE detection + Hunt | `src/argus.cpp`, `src/features/ble_scan.cpp`, `ble_finder.cpp`, `surveillance_hunter.cpp` | **Parity — reuse.** Node surfaces Poseidon classifiers. |
| BLE Lab (GATT / pairing / HID / clone) | `ble_gatt.cpp`, `ble_hid.cpp`, `src/hid_decode.cpp`, `ble_clone_profile.cpp` | **Strong parity — reuse.** |
| Classifiers: Remote ID / Find My / iBeacon | `drone_remoteid.cpp` ✓, `ble_findmy.cpp` ✓, iBeacon **no standalone file** | **Mostly parity.** iBeacon likely inside `ble_scan`; confirm in P3. |
| Wardrive / GPS (NMEA, WiGLE CSV) | `src/gps.cpp`, `src/gps_nmea.h` | **Parity — reuse**; node adds J8 UART GPS later. |
| Radar visual (round LCD) | no radar module found | **Net-new (PUIE), P2.** |
| CSI motion monitor | none found | **Net-new, P3.** |
| Themes / watch faces / Badge Studio / locksport | none | **Net-new, node-only, P4, Dale-gated.** |

**Gap conclusion for P0:** detection logic already belongs to Poseidon. The genuine protocol-layer gaps are
**(a) multi-node attribution** and **(b) a BLE detection-publish path** — BLE is entirely absent from the
ESP-NOW wire today (the C5 is Wi-Fi/Zigbee only). P0 builds exactly those.

## 2. Protocol findings that shape P0

1. **The v5 header carries no node identity.** Layout: `magic(4)/version(1)/type(1)/seq(2)/payload_len(2)/payload`
   = 10 bytes, with every offset pinned by `test_header_layout`. Routing today is purely by ESP-NOW peer MAC;
   `posei_hello_t` has `role`+`name`, but responses are attributable to a node only by sender MAC.
2. **New detection-publish types are additive, not a flag day** — new RESP types in a fresh range (e.g. 60–69)
   plus new payload structs with size-guard `static_assert`s. Must be mirrored **byte-identically** in
   `c5_node/main/proto.h` *and* `src/c5_cmd.h`, per the existing contract. Old peers never see them.
3. **Addressing has two forms with very different blast radius** (see §4 — this is the reserved decision).

## 3. Proposed P0 — native-tests-first, no board

Branch `claude/godzilla-espnow-multinode`:

1. **Node attribution** via the payload-envelope approach (§4-B): S3-side MAC↔node map keyed off HELLO
   (`role`,`name`), plus a small node-id prefix on detection batches. Header stays v5.
2. **New detection-publish types + payloads**: `RESP_WIFI_DET`, `RESP_BLE_DET` (addr, RSSI, classifier family,
   flags). BLE is net-new to the wire.
3. **Native Unity tests first** (`test/test_proto` or new `test_godzilla_proto`): field offsets, `posei_wire_len`
   for the new batches, one-frame-fit asserts, node-id round-trip. `pio test -e native-test` green before any firmware.
4. **Two free doc reconciliations** the brief flagged: the stale 230-byte struct in `c5_node/README.md`, and the
   "v3" docstring in `src/c5_cmd.h` (both are v5 now).

**Exit test:** native proto suite green · TX-arm untouched (all passive, no gating) · **zero change to C5 on-air bytes**.

## 4. Reserved for Dale (§8) — the one decision blocking the branch

How to add multi-node addressing:

- **(A) Header field + v6 bump** — add `node_id`/`role` to the header. Clean, but changes `POSEI_HEADER_SZ`,
  breaks the pinned offset test, and **changes the C5 on-air format → re-flash of the shipped TRIDENT image.**
  This is a §8 reserved wire-version bump.
- **(B) Payload-envelope, header stays v5** — attribute by sender MAC ↔ HELLO map on the S3; node-id rides in the
  new detection payloads. **No flag day, TRIDENT untouched, C5 never re-flashed.**

**PSPE recommendation: (B)** for P0. Keeps the shipped TRIDENT image byte-stable and defers any flag day until a
real need forces it. Approve (B) — or request (A) and accept re-flashing the C5 — and the branch opens with tests first.
