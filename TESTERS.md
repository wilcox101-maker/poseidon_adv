# For Testers

If you're helping stress-test POSEIDON, this doc tells you **what just changed**,
**what to hit hardest**, and **how to report what you find**.

**Currently testing:** v0.5.0 is tagged, master is ~20 commits past it +
in-flight 0.6 work. The TRIDENT ESP32-C5 web flasher just went live at
[docs/flash/](docs/flash/). Big stuff to verify
on real hardware:

1. End-to-end browser flash of a TRIDENT C5 (P1)
2. **LoRa + Meshtastic full sweep** (P2) — biggest regression surface
3. Samsung Smart Remote rebuild (P3)
4. WiFi Beacon Spam new raw-IDF path (P4)
5. TRIDENT 5 GHz attack operations (P5)
6. Regression sweep across all 100+ features (P6)

---

## Priority 1 — C5 web flasher (just deployed)

The flasher at [docs/flash/](docs/flash/) is the
first time we've had a one-click install path for the ESP32-C5
satellite. esp-web-tools 10.2.1 on jsdelivr (the unpkg edge cache was
serving an old 10.0-era bundle with no ESP32-C5 support — that's now
fixed). We need real-board confirmation.

1. Open the flasher URL in Chrome / Edge / Opera
2. Plug the C5 module **directly** via USB-C (not through the Cardputer)
3. Click **Install TRIDENT** → pick the C5's COM port (NOT the
   Cardputer-Adv's port — they both enumerate as VID 0x303A PID 0x1001
   so look for the *second* port that appears when you plug the C5 in)
4. Confirm: erase → bootloader (0x2000) → partition-table (0x8000)
   → app (0x10000) all green
5. Boot it, pair it to a POSEIDON Cardputer, `Radio → C5 → Status`
   should show `C5 x1` within a few seconds

**Known gotcha:** if "Failed to initialize" appears, the C5 didn't enter
download mode automatically. Hold the C5's **BOOT** button (GPIO 9),
tap **RESET**, release BOOT — then click Install. Some C5 dev boards
don't have the auto-reset DTR/RTS wiring.

## Priority 2 — LoRa + Meshtastic full sweep ★

LoRa is the band with the most surface area and the most regression
risk — nine features touching the SX1262 driver, antenna-switch GPIO
sequencing, HSPI shared with SD, GNSS UART, and Meshtastic protobuf
encoding. Hit each leaf and confirm none reboot the device.

**Setup:** attach the CAP-LoRa1262 hat with sky view (GPS needs it).
Verify `System → About` reports "Adv" board.

### Per-feature checklist

- **LoRa → Scan** — switch bands with TAB (433 / 868 / 915 / US
  LongFast). Each retune should be instant, no crash.
- **LoRa → Beacon** — press TX. A nearby SDR or Meshtastic device
  should see the packet.
- **LoRa → Analyzer** — cycle Bar / Waterfall / Oscilloscope renders.
  The `RX N` overlay should tick when real LoRa traffic is in range.
  Backtick should back out of any view immediately.
- **LoRa → Mesh Nodes** — leave running ~2 min near Meshtastic
  activity (check meshmap.net for your area). Roster should populate
  with **real names**, not garbage. If garbage → protobuf decode bug.
- **LoRa → Mesh Chat** — press `T` to start typing, type a short
  message, ENTER to broadcast. Check another Meshtastic device — the
  message should appear from POSEIDON's `!xxxxxxxx` node ID.
- **LoRa → Mesh Page** — from Mesh Nodes, highlight one of your own
  devices and press ENTER. Type a short DM, send. The destination
  should receive it as a direct, not a broadcast.
- **LoRa → Mesh Pos** — press `T` to toggle reporting on. Wait for GPS
  fix. Press `B` to force a position send. You should appear as a pin
  on other Meshtastic apps.
- **LoRa → GPS Fix** — live NMEA, baud auto-detect, sat count + HDOP.

### Range test

From your house outward in increments, note the last RX confirmed
(RSSI/SNR shown in Analyzer) by band. Report in feet or meters. Useful
for the next CHANGELOG.

### What to report

- Any LoRa feature that **reboots** the device — grab the serial log.
- Any **band that doesn't retune** when switched via TAB.
- **RX works but TX doesn't**, or vice versa (encryption / header bug).
- Mesh Nodes showing **garbage names / node IDs** (protobuf decode).
- Position showing up at **wrong lat/lon** (sfixed32 sign bug).
- Frames not detected on a band where you know traffic exists.

### Known caveats (don't file these as bugs)

- **Leaf-only** — POSEIDON does NOT forward other nodes' packets. If
  there are nodes behind another node's relay, we won't reach them
  directly.
- **No CAD before TX** — we transmit immediately without checking if
  the channel is busy. Collides with in-flight packets slightly more
  than a compliant node, especially in dense mesh areas. On the roadmap.
- **NodeInfo broadcasts every 30 min regardless of the Mesh Pos
  toggle** — that toggle only gates Position, not NodeInfo. If you
  need to be fully silent, don't enter mesh features at all.

## Priority 3 — IR Samsung Smart Remote rebuild

Every IR feature was silently no-op'ing. Two stacked bugs:

1. **GPIO 44 is UART0 RX by default** and `pinMode(OUTPUT)` doesn't
   fully detach the matrix in Arduino-ESP32 v3 (fix: `gpio_reset_pin`).
2. **Cardputer-Adv's IR LED is active-LOW**, which LEDC's
   `output_invert` flag can't handle at `duty=0` because the LEDC
   idle level isn't affected by inversion (fix: bit-bang the 38 kHz
   carrier in CPU instead).

Plus the Samsung protocol byte order was NEC, not Samsung32. Plus
TV-B-Gone was stealing the display backlight's LEDC channel (fix:
moved to channel 3 / timer 3).

### Per-feature checklist

- **IR → LED Test** — point a phone camera at the LED. Step 2 (GPIO 44
  inverted) should light it purple/white. Useful for verifying any
  Cardputer board variant.
- **IR → Remote** against a Samsung TV — press `p` for power, then
  `u/d` for volume, `m` for mute, `c/v` for channels. **No modifier
  keys should be required for any binding.** D-pad: `;` (up), `.`
  (down), `,` (left), `/` (right). ENTER for OK.
- If power toggle (`p`) doesn't respond, try `o` (discrete on) or `q`
  (discrete off). Modern Smart TVs sometimes ignore IR power.
- **IR → TV-B-Gone** at a TV wall — should cycle Sony / Samsung / LG /
  Panasonic / Philips / Vizio. Display backlight should **stay on**
  (was blanking before the channel-3 fix).
- **IR → Clone** — Samsung / LG / Sony preset profiles. Basic vol +
  mute should work for each.

### What to report

- Any button that doesn't fire IR (use the LED Test diag to confirm
  the hardware first).
- Any TV that responds to volume but not power (might be a
  model-specific power code we don't have).
- Any non-Samsung TVs that the Samsung sender unexpectedly controls.

## Priority 4 — WiFi Beacon Spam new path

The Arduino `WiFi.softAP()` path crashed in `ieee80211_hostap_attach`
(LoadProhibited at +0x2c) on Bruce's pinned `esp32-arduino-libs`.
New recipe: `esp_bt_controller_mem_release(BTDM)` → manual
`esp_netif_init` + `esp_netif_create_default_wifi_sta` →
`esp_wifi_init` with shrunk dynamic bufs (16/16) → STA mode +
promiscuous + raw 802.11 TX on `WIFI_IF_STA`. Sustained ~1200
frames/sec.

1. **WiFi → Beacon spam → Meme.** Open WiFi scan on a phone — meme
   SSIDs (FREE WIFI - CLICK HERE, NSA Surveillance Van, etc.) should
   appear within a few seconds.
2. The scanner UI should show a KITT-style sweep header, live big
   SSID in accent color, fading 5-line recent feed, fps counter, and
   a hex stream at the bottom. Channel rotates 1→11 once per full
   SSID-list pass.
3. Leave running 30+ seconds — uptime should keep ticking, no reboot.
4. Press ESC — should exit cleanly back to the menu with heap
   recovering to >50 KB.
5. **Side effect to verify:** after exiting Beacon Spam, BLE features
   won't work until reboot (BT memory was released to make room for
   WiFi). Confirm a fresh boot brings BLE back.

**Report:** phones seeing fewer than 5 SSIDs (RF environment), any
feature that's opened AFTER Beacon Spam and fails (state leak), and
whether Rickroll + Custom lists also work.

## Priority 5 — TRIDENT 5 GHz operations

Once a C5 is flashed (P1), exercise the actual attacks. TRIDENT's job
is the RF territory the S3 can't physically touch.

- **C5 → Scan** — full dual-band, APs from both 2.4 + 5 GHz with auth
  column. WPA3 rows red.
- **C5 → Deauth → broadcast** on a 5 GHz channel you own — frames
  counter ticks, target reconnects.
- **C5 → PMKID** — M1 capture against a 5 GHz AP. `.22000`
  hashcat-format file on SD.
- **C5 → Zigbee sniff** — channels 11-26, pcap dump compatible with
  Wireshark.
- After every C5 attack: `C5 → Status` should still show the
  satellite present and pinned to channel 1 (so POSEIDON keeps
  hearing HELLOs).

## Priority 6 — Regression sweep (broad surface)

100+ features. Hit each one once, even if just to enter and back out.
Anything that crashes or reboots since v0.5.0 is a regression we want
to know about now.

- **WiFi (17):** Scan · Clients (both) · Deauth · Deauth All ·
  Detector · AP Clone · Evil Portal · Karma · Beacon Spam · Probe
  Sniff · PMKID · Spectrum · Wardrive · CIW · Connect
- **BLE (16):** Scan · Spam · Bad-KB · Tracker Detect · Tracker
  Finder · Sniffer · iBeacon · Clone · GATT · Flood · Karma · Sour
  Apple · Find My · Toys · Drone RID · WhisperPair
- **Defensive (3):** Surveillance Hunter · Defensive Monitor · SATCOM
- **Sub-GHz (9):** Scan · Spectrum · Brute · Jammer · Broadcast
  Library · Record · Replay · Finder · Jam Detect
- **nRF24 (6):** Sniffer · MouseJack · BLE spam · Spectrum · Jammer ·
  Finder
- **Network Attacks (18):** Port Scan · Ping · DNS · Connect ·
  Responder · SSDP · LAN Recon · UART Shell · Reverse TCP · Telnet
  Honeypot · Dead Drop · Printer · SSDP Poison · DHCP Starve · Rogue
  DHCP · Hijack · WPAD · Autodiscover
- **IR (4):** TV-B-Gone · Remote · Clone · LED Test (covered in P3)
- **Tools + System:** Flashlight · Stopwatch · Dice · Morse · MAC
  Random · Calc · Screen Test · SD Format · Theme Picker · Ambient
  Preview · Carousel toggle · Screensaver settings

## How to report

### Ideal bug report

- **Build**: v0.5.0 (tagged) or master + commit SHA (boot splash
  shows version)
- **Hardware**: Cardputer-Adv + which hat attached (CAP-LoRa1262 /
  Hydra RF / W5500 / none / C5 satellite paired)
- **Feature path**: e.g. `LoRa → Mesh Chat → T → type → ENTER`
- **What happened** vs **what you expected**
- **Reproducibility**: always / sometimes / once
- **Serial log** if possible (`pio device monitor --baud 115200` on
  the Cardputer; `idf.py monitor` on the C5)

### Where to file

GitHub Issues: <https://github.com/Evil0ctopus/poseidon_adv/issues>
Label `v0.5-tester` if you flashed the tag, or `v0.6-tester` if you
flashed master.

## Thanks

If you're testing, you're the reason this firmware ships correctly.
Range numbers from the LoRa sweep, TV-model lists where IR works /
doesn't, and any "I clicked this and it rebooted" with a serial log
are the most valuable thing you can send. Keep breaking things.
