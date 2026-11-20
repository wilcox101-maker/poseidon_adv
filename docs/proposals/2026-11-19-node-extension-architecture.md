# Poseidon node-extension architecture + HaleHound-CYD findings triage

**From:** Claudius Maximus, 2026-11-19. **Dale:** look at JesseCHale/HaleHound-CYD; fold findings
into Poseidon as main functions; define how Trident (ESP32-C5) and GODZILLA (ESP32-S3) interact as
extensions. **This is a design doc for Dale's review — no code until he picks what to build.**
Everything below is **passive / receive-only / defensive**; the offensive items are listed only to
mark them out of scope.

## 1. The model: hub + specialized sensor nodes
HaleHound puts every radio (CC1101 SubGHz, NRF24 2.4 GHz, PN532 NFC, GPS) on one ESP32 sharing one
SPI bus with firmware arbitration. Poseidon is already past that: the **Cardputer is the hub** (UI,
watch faces, radar, severity, alert, recon log) and **nodes are specialized sensors** that publish
detections to it over the existing additive ESP-NOW wire (v5). So instead of one contended bus, each
band gets its own node and the hub merges the feeds.

| Node | Radio(s) it owns | Detects (passive) |
|---|---|---|
| **Cardputer** (hub) | 2.4 GHz Wi-Fi + BLE | its own scans + **aggregates every node feed** |
| **GODZILLA** (ESP32-S3) | 2.4 GHz Wi-Fi + BLE | Wi-Fi AP/STA, BLE beacons/trackers, surveillance §4 |
| **Trident** (ESP32-C5) | **5 GHz + 2.4 GHz Wi-Fi** | the band the S3 can't see — 5 GHz APs/clients, 5 GHz Remote ID |
| *(optional sensor module on a node's spare SPI)* | CC1101 (SubGHz RX) / PN532 (NFC) | SubGHz spectrum/interference; NFC card type ID |

Trident's value is **5 GHz** — the S3 has no 5 GHz radio, the C5 does, so Trident covers detections
GODZILLA structurally cannot. That is the clean division, no bus contention, each node independently
replaceable.

## 2. Wire mechanics (all additive, header stays v5)
- **Node capability HELLO.** Extend the HELLO each node already sends with a small capabilities
  field: which bands (2.4/5/SubGHz) and sensors (NFC) it carries. The Cardputer then draws a live
  **sensor map** ("GODZILLA: 2.4+BLE · TRIDENT: 2.4+5 · +SubGHz") and knows which feeds to expect.
- **New detection families** (append-only, like #124) for anything new below.
- **No new wire direction** — nodes still only publish; the hub still only receives from them. The
  one downlink is the already-agreed time-sync (CMD_TIMESET).

## 3. Findings to fold in as main functions (passive — need Dale's pick)
Grouped by what hardware they need:

**a. No new hardware (BLE/Wi-Fi the nodes already have):**
- **KARR car-alarm beacons** → new `CARALARM` family (BLE signature). Benign, easy, low risk.
- **RF-interference / jam watch (defensive):** surface when a band is being drowned out — Wi-Fi
  deauth-flood (Poseidon already has DEAUTH_DETECT), and a 2.4 GHz broadband-noise indicator from the
  existing scan. Alerts "someone may be jamming 2.4 GHz near you." Detection only; never transmits.
- **Skimmer scoring** — already have the `SKIMMER` family (#124); fold HaleHound's scoring *idea*
  (confidence from multiple signals) into that classifier.

**b. Needs a receive-only sensor module on a node (Dale's hardware call):**
- **SubGHz interference/spectrum watch** via a CC1101 in **RX-only** mode on a node's spare SPI —
  a passive spectrum/carrier-presence monitor (315/433/868/915 MHz), publishing a `SUBGHZ_NOISE`
  indicator. No TX, no capture/replay.
- **NFC/RFID card-type identification** via a PN532 — identify a card's type/standard when one is
  presented to the reader. Identify only; no emulation, no cloning, no key cracking.

**c. Offensive — see §3.5 (Dale: include offensive tools).** Routed through the existing TX-arm gate.
GPS wardriving stays out (no GPS; already Dale-reserved).

## 3.5 Offensive tools — behind the existing TX-arm SAFE/ARMED gate
Poseidon already ships a gated offensive arsenal; the **TX-arm master gate** (audited this session,
F2–F6) is exactly the mechanism to fold the offensive side onto the nodes. Invariants:
- **GODZILLA stays passive.** The S3 detection node never transmits beyond its ESP-NOW publish/HELLO.
  Offensive TX lives on the **Cardputer hub** and, if Dale builds it, **Trident** — never on GODZILLA.
- **Every offensive op is TX-arm gated.** Master **SAFE by default** (NVS); each op calls
  `tx_arm_require()` at launch; nothing self-arms. A node never arms itself — the **hub arms and
  commands** it (a new gated downlink, SAFE-default, mirrors CMD_TIMESET's additive style).
- **Authorized-use, logged.** Each armed op writes a recon-log line (op, time, target) for an audit
  trail; AGENTS.md "hardware and networks Dale owns or has written permission to test" governs.
- **Already in the tree** (organize under node control, no new capability): deauth + deauth-detect,
  beacon spam, probe/PMKID + handshake capture, AP clone / KARMA, the nRF52 MITM trio, IR, LoRa
  beacon, WhisperPair — standard Wi-Fi/BLE pentest primitives, all gated.
- **New, foldable behind the gate:**
  - **Network recon** — join a Wi-Fi Dale controls, enumerate the subnet (hub app).
  - **NFC/RFID read + clone + emulate** (PN532 / Chameleon-style) — physical-red-team, Proxmark/
    Flipper territory, used on badges/cards in an engagement's authorized scope.
  - **RF jamming / broadband interference** (Wi-Fi continuous-TX and/or an NRF24 carrier emitter) —
    for **RF research and durability / resilience testing** (how equipment behaves under a saturated
    band). Full-power transmit by nature.

### Offensive governance (Dale: SAFE/ARMED + per-op logging)
Every offensive op — the Wi-Fi/BLE primitives above, NFC, network recon, and RF jamming — sits
behind the master **TX-arm gate**: **SAFE by default** (NVS), `tx_arm_require()` at launch,
**hub-armed** (a node never self-arms), and **each armed op writes an audit line** (op, time,
target/band, power, duration) to the recon log. Jamming is a broadband emitter, so the firmware
**labels it as such** and keeps it behind the same gate so it's never armed by accident; the gate,
the log, and the operator's authorization are what bound it — the same framework as every other
transmitter, scaled for higher energy. We also keep jamming **detection** (§3a) alongside it.

GODZILLA staying passive is a **role split** (it's the sensor), not a limit on the project — the
full arsenal lives on the hub and, if built, Trident.

## 4. Licensing / sourcing caveat
HaleHound's license was **not visible on the page** — confirm it before any closer reference, and
regardless build every signature table from **primary sources** (IEEE OUI registry, Bluetooth SIG
Assigned Numbers, ASTM F3411 for Remote ID, vendor docs), never copied lists or code.

## 5. Recommended first step (if Dale agrees)
Smallest safe increment, no new hardware: **(1)** the node-capability HELLO + hub sensor map
(Claudius, wire), **(2)** `CARALARM` family + classifier (GODZILLA), **(3)** fold skimmer scoring.
SubGHz/NFC modules and the Trident 5 GHz build are a larger, hardware-dependent phase to scope only
if Dale wants them.
