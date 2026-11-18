# Proposal: GODZILLA node — Dragon Detector feature port

**Status:** kickoff brief · **Owner chat:** *Godzilla Node for Poseidon* (PSPE + PUIE) ·
**Overseer:** Claudius Maximus (authored this brief, 2026-11-18) · **Dale decides** direction and
the reserved items below.

## 1. One-paragraph summary

Add a second Poseidon node, **GODZILLA**, on the **Waveshare ESP32-S3-Touch-LCD-1.85B**. Unlike the
headless TRIDENT C5 radio node, GODZILLA is a **full-UI second head**: it runs its own detection /
radar / lab interface on the round 360×360 LCD **and** publishes its detections to the
Cardputer-Advance over **ESP-NOW v2** as another node on the existing fabric. Its feature set is a
**native reimplementation** of [Dragon Detector](https://github.com/wilcox101-maker/Dragon-Detector)
(MIT, Valleytech / "Your Pal Kal"), credited, brought in as an in-tree, replayable fork unit under
`godzilla_node/`. It is **Poseidon-dependent**: no standalone release, it rides Poseidon's CI,
versioning and ESP-NOW protocol.

## 2. Decisions locked by Dale (2026-11-18)

| # | Decision | Value |
|---|----------|-------|
| D1 | Node topology | **Full-UI second head + ESP-NOW v2 node.** The 1.85B keeps a rich local UI *and* feeds Poseidon. |
| D2 | Feature scope | **Everything portable.** Where Poseidon already has parity, *evaluate and report* rather than duplicate; otherwise port as much as fits. |
| D3 | Provenance | **Reimplement natively** in Poseidon's style, Dragon Detector credited. Not vendored, not a DD fork. |
| D4 | Home | **In-tree, Poseidon-dependent** — `godzilla_node/`, no independent release. |

Still Dale's to confirm: hardware-in-hand for bench tests (brief assumes **desk-only** until he says
otherwise); whether cosmetic layers (Badge Studio, watch faces, locksport timers) ship in v1 or defer.

## 3. Hardware (Waveshare ESP32-S3-Touch-LCD-1.85B)

ESP32-S3R8 · 8 MB PSRAM · 16 MB flash · 1.85" round **360×360** LCD, **ST77916 QSPI** · **CST816S**
cap touch (I²C) · **QMI8658** 6-axis IMU · **ES8311** codec + **ES7210** AEC + dual mics + speaker
pads · **PCF85063-class RTC** · **BQ27220** fuel gauge, MX1.25 LiPo · microSD (4-bit SDMMC, separate
from the display QSPI) · PWR + BOOT buttons · USB-C · SH1.0 4-pin GPIO + SH1.0 4-pin UART (J8:
GPIO44 RX / GPIO43 TX, 3V3). [Datasheet](https://www.waveshare.com/esp32-s3-touch-lcd-1.85b.htm?sku=34554)
· [Wiki](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B).

This is **already a Dragon Detector target** (`waveshare_185b`), so the detection firmware has a
proven base on this exact board — the reimplementation is a port of known-working behaviour, not a
bring-up from zero.

## 4. Architecture

### 4.1 Where GODZILLA sits

```
          ESP-NOW v2 (PigSync, wire v5, magic "POSE")
Cardputer-Advance (S3) ──────────────┬───────────────── TRIDENT C5 node  (headless 5 GHz / 802.15.4 radio)
   POSEIDON main UI                   └───────────────── GODZILLA node    (full-UI 2.4 GHz detection head, 1.85B)
```

- **C5 / TRIDENT** is a remote *radio* the Cardputer drives (command → execute → stream back).
- **GODZILLA** is a peer *sensor + head*: it scans on its own S3 2.4 GHz radio, shows its own
  radar/lab UI, and **publishes** detections to the Cardputer for aggregation. The Cardputer may also
  send it commands (focus a target, start a capture), but GODZILLA is not a dumb radio.

### 4.2 ESP-NOW v2 — the enabling protocol work

Poseidon's current wire is **v5 / 1024-byte variable-length frames** (0.9.2 flag day; note
`c5_node/README.md` still shows the pre-flag-day 230-byte struct — reconcile that during the port).
Today it is **point-to-point S3↔C5**. GODZILLA needs **multi-node addressing**, which is already on
Poseidon's roadmap ("multi-node wire fields"). First deliverable of the port is therefore protocol,
not UI:

- Add a **node-id / role** field and per-node routing to `posei_msg_t` (or a v6 bump — PSPE decides).
- New message types for **detection publish** (GODZILLA→S3: Wi-Fi/BLE detection batches, RSSI,
  classifier family) distinct from the C5's command/response set.
- Keep passive scan/sniff **never gated**; any transmitter on the node stays behind Poseidon's master
  **TX-arm** SAFE/ARMED gate ([docs/tx-arm.md](../tx-arm.md)).

### 4.3 UI direction (PUIE, Dale 2026-11-18)

The node UI is **Poseidon-themed**, not a Dragon Detector reskin — Deepwater palette, Poseidon
iconography, and the master **TX-arm ARMED** status cue carried through on the node's own screen.

First-class watch face: a **functional, old-fashioned pocket-watch clock face** on the round
360×360 LCD — Roman or classic serif numerals, an ornate bezel/fob framing, moving hour/minute/second
hands driven by the board's PCF85063 RTC (RTC-accurate across power cycles, USB/Wi-Fi/GPS time sync as
available). It is a real timepiece, not a static graphic. This is the lead example of DD's "watch
face" feature reimagined in Poseidon's house style; other faces follow the same theming.

### 4.4 In-tree layout (mirrors `c5_node/`)

```
godzilla_node/
  CMakeLists.txt  partitions.csv  sdkconfig.defaults  README.md
  main/            # ESP-IDF or Arduino/PlatformIO node firmware for the 1.85B
```
Add a `godzilla` env to `platformio.ini` alongside `cardputer*` and `native-test`. Node-side
reimplemented modules are credited to Dragon Detector in a `CREDITS`/README section.

## 5. Dragon Detector feature inventory → Poseidon parity (PSPE/PUIE to verify)

D2 says: where Poseidon already has it, **evaluate and report**; otherwise port. First engineering
task is to turn this first-pass map into a verified audit.

| Dragon Detector feature | Poseidon today | Disposition (first pass) |
|---|---|---|
| Wi-Fi/BLE **detection + Hunt** (RSSI trace, watch/ignore, history) | `argus`, `ble_scan`, `ble_finder`, per-contact hunt | **Parity — evaluate.** Reuse Poseidon classifiers; node surfaces them. |
| **Radar** visual (dot select, family colours) | No radar paradigm | **Port (PUIE)** — net-new round-LCD radar. |
| **Recon** (surveys, captures, sessions, CSV; PCAP) | PCAP/pcapng export is "what's coming" | **Port + align** with Poseidon's planned pcap export. |
| **BLE Lab** (GATT, pairing, HID decode, emulation, clone) | `ble_gatt`, `ble_hid`, `hid_decode`, `ble_clone_profile` | **Strong parity — evaluate**, likely reuse. |
| Classifiers: Remote ID, AirTag/Find My, iBeacon, Flock | `drone_remoteid`, `ble_findmy`, `ibeacon` | **Parity — evaluate.** |
| **CSI** motion monitor | None found | **Port** — net-new. |
| **Wardrive / GPS** (NMEA, WiGLE CSV) | `gps.cpp`, `gps_nmea.h` | **Parity — evaluate**; node adds J8 UART GPS. |
| **Themes / logo watch faces** (45 themes) | None | **Port (PUIE), node-only**, Poseidon-themed. Lead face: functional old-fashioned **pocket-watch** clock (§4.3). Other faces defer-able (Dale). |
| **Badge Studio** | None | **Port, node-only** — cosmetic, defer-able (Dale). |
| **Locksport timers** | None | **Port, node-only** — cosmetic, defer-able (Dale). |
| Clock / stopwatch, RTC (PCF85063), ES8311 audio tones | Audio exists; no clock UI | **Port, node-only.** |

Net-new engineering is concentrated in: the **round-LCD radar/watch UI**, **CSI**, the **multi-node
ESP-NOW fields**, and the **detection-publish bridge**. Most RF detection logic should be *reused*
from Poseidon rather than re-ported from DD — confirm per row.

## 6. Provenance & licence

Both projects are MIT. Approach is **native reimplementation** (D3): rebuild behaviour in Poseidon's
own structure, no DD source vendored, no DD binaries. Credit Dragon Detector (Valleytech /
@valleytechsolutions) in `godzilla_node/README.md` and `CREDITS`. Keep the work as a **focused,
replayable fork unit** per [docs/fork-status.md](../fork-status.md), so upstream Poseidon syncs
replay it cleanly. No stock/vendor blobs; no artwork copied without its own licence check (DD's
`CREDITS.md` lists third-party art — some watch faces/logos may be character art that must **not** be
reproduced; the engineer screens these before porting any theme).

## 7. Suggested phasing (desk-only until Dale confirms hardware)

1. **P0 — Protocol.** Multi-node ESP-NOW v2 fields + detection-publish types; native protocol tests
   green in CI. No board needed.
2. **P1 — Node skeleton.** `godzilla_node/` builds; 1.85B brings up LCD (ST77916 QSPI), touch
   (CST816S), IMU, RTC, SD; `godzilla` PlatformIO env; CI builds it.
3. **P2 — Detection + radar.** Wi-Fi/BLE scan on the node, radar UI, publish to Cardputer; Cardputer
   shows GODZILLA as a node and aggregates its detections.
4. **P3 — Labs.** BLE Lab, Recon/captures, CSI, wardrive — reusing Poseidon logic where parity exists.
5. **P4 — Cosmetic (optional, Dale-gated).** Themes, watch faces, Badge Studio, locksport timers.

Each phase ends at an exit test + Dale's sign-off. Everything "desk only" is labelled as not
hardware-verified per Poseidon rules.

## 8. Reserved for Dale

- Hardware in hand for bench testing (sets whether P1+ can be validated or stay desk-only).
- Cosmetic scope in v1 vs deferred (§5, §7 P4).
- Any protocol **wire-version bump** that changes the C5 on-air format (affects the shipped TRIDENT
  image) — flag before merging.
- Rule-file and release-metadata changes, per `AGENTS.md`.

## 9. First actions for the Godzilla chat

1. Attach `wilcox101-maker/poseidon_adv` (push) and `wilcox101-maker/Dragon-Detector` (read); read
   this brief, `README.md`, `docs/fork-status.md`, `c5_node/README.md`, `docs/tx-arm.md`.
2. Turn §5 into a **verified parity audit** (report back; don't duplicate Poseidon code).
3. Open P0 as the first branch: `claude/godzilla-espnow-multinode`, native protocol tests first.
4. Report to Dale at the P0 exit test.
