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

**c. Reserved for Dale / explicitly NOT folded (out of the passive, authorized-use scope):**
- Anything that transmits, pairs, injects, clones or emulates: WhisperPair-style pairing, BLE
  Predator loot, Chameleon NFC cloning, any jamming **transmit**.
- Active network intrusion: IoT-Recon-style joining a Wi-Fi network and scanning the subnet,
  credential capture.
- GPS wardriving (no GPS; already Dale-reserved).

## 4. Licensing / sourcing caveat
HaleHound's license was **not visible on the page** — confirm it before any closer reference, and
regardless build every signature table from **primary sources** (IEEE OUI registry, Bluetooth SIG
Assigned Numbers, ASTM F3411 for Remote ID, vendor docs), never copied lists or code.

## 5. Recommended first step (if Dale agrees)
Smallest safe increment, no new hardware: **(1)** the node-capability HELLO + hub sensor map
(Claudius, wire), **(2)** `CARALARM` family + classifier (GODZILLA), **(3)** fold skimmer scoring.
SubGHz/NFC modules and the Trident 5 GHz build are a larger, hardware-dependent phase to scope only
if Dale wants them.
