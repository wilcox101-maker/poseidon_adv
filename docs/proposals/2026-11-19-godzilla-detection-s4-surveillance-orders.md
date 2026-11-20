# GODZILLA — detection §4: surveillance & privacy awareness (orders)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19. **Dale:** "Fold all features that are
missing into Godzilla" — the gaps found comparing against skizzophrenic/SquachWatch-CYD (GPL-3.0).
**Order:** after the watch-face phase (units 4–6) and the render fixes; one small PR per unit.

## Ground rules for this brief
- **Passive only.** Classify frames the node already hears. Nothing here transmits.
- **No GPL code or curated lists copied.** Take the *categories* only. Build every signature table
  from primary sources and cite them in the file: IEEE MA-L/MA-M OUI registry, Bluetooth SIG
  Assigned Numbers (company IDs, 16-bit service UUIDs), vendor documentation.
- **Confidence on every signature: HIGH or LOW.** LOW shows with a "?" and never raises the alert
  overlay on its own. Shared OUIs (e.g. a parent company that also ships non-camera products) are
  LOW by definition.
- **Honour the existing evidence.** `src/sigdb_surveillance.h` removed Axon SSID patterns because no
  public fingerprint exists and SSID matches produced false positives. Do not reintroduce
  name/SSID-only matching for LE gear.

## Wire (done by Claudius in the same PR)
New families, append-only: `SKIMMER=12`, `CAMERA=13`, `LE_EQUIP=14`, `ALPR=15`, `SMARTGLASS=16`
(`proto.h` / `c5_cmd.h` / `det_classify.h`), with Cardputer names, tags and colours, and parity
tests pinning all 17 families. Your node classifiers publish these values; add their labels and
severity to `gz_radar.h` (skimmer / LE / ALPR = surveillance-red, camera / glasses = amber).

## Units
1. **BT card skimmers → `SKIMMER` (HIGH when name + UUID agree).** BLE-advertising serial modules
   commonly used in skimmers: match the advertised local name (HM-10/HC-08/BT04-A-style names) plus
   the serial-service UUIDs those modules expose. *Hardware limit:* HC-05/HC-06/RN42 are Bluetooth
   **Classic-only**; seeing them needs an active inquiry (transmits) and the S3 has no Classic radio,
   so they are out of scope — say so in the file header. Native-test with captured advert bytes.
2. **Covert / IP cameras → `CAMERA`.** Wi-Fi OUI table for camera makers (e.g. Hikvision, Dahua,
   Wyze, Arlo, Reolink, Eufy, Ring) from the IEEE registry; dedicated camera-maker OUIs HIGH, shared
   parent-company OUIs LOW. Optional LOW-confidence setup-AP SSID patterns. Native-test the lookup.
3. **LE equipment → `LE_EQUIP` (LOW).** Only an OUI the IEEE registry assigns to the equipment maker
   (e.g. Axon Enterprise) — verify it exists; if none, ship the family empty and say why. Never
   SSID-only.
4. **Non-Flock ALPR → `ALPR` (LOW).** OUIs registered to ALPR makers (e.g. Motorola Solutions /
   Vigilant) from the IEEE registry; LOW, needs corroboration.
5. **Recording smart glasses → `SMARTGLASS`.** Match the glasses maker's Bluetooth SIG company ID
   and/or assigned service UUID as published in Assigned Numbers. HIGH only on an exact assigned
   value; native-test with captured bytes.
6. **Priority ALERT overlay (UI).** Full-screen alert for HIGH-confidence `SKIMMER`, surveillance
   families, and a tracker seen repeatedly over time: family, confidence, MAC, RSSI, channel and a
   mini radar; tap to dismiss, auto-clear after 60 s, rate-limited per device, dimmed in always-on.
   Per-family enable toggles in the on-device customiser. Lay out via `gz_geom`, shape-safe on all
   three boards. [render]

Each classifier unit: pure header in the node tree, captured-bytes native test, CI green, draft PR.
Recon's SD log picks the new family names up automatically — confirm in the PR.
