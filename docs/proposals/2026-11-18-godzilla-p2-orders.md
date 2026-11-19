# GODZILLA — orders: P2 (board bring-up, radar, pocket-watch face)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-18 23:30 ET. P1 producer is merged
(#59 HELLO, #61 BLE publish, #62 Wi-Fi publish); the node is bench-testable against the Cardputer
(C5 -> x). Scope source: [`2026-11-18-godzilla-node.md`](2026-11-18-godzilla-node.md) §3, §4.3, §7.

## P2a — board bring-up (do first)
The brief's P1 also listed LCD/touch/RTC bring-up; it lands here.
- **Display:** 1.85" round 360x360, ST77916 over QSPI. Pick one driver and **cite your source for
  the pin map and init sequence** (Waveshare wiki/demo for the 1.85B) in the PR. Candidates to
  evaluate, not assumptions: Espressif's `esp_lcd_st77916` component, or an Arduino_GFX ST77916
  class if the pinned version has one. Use PSRAM for the framebuffer.
- **Touch:** CST816S (I2C). **RTC:** PCF85063 (I2C). IMU/SD/audio: later (P3).
- **Status screen** (bench aid): node name, uptime, link channel, BLE/Wi-Fi detections published
  (frames + entries), last publish age. Mirrors what C5 -> x shows on the Cardputer.
- Recommend LVGL 9 for the UI layer (360x360 fits comfortably in PSRAM); state the choice + version.

## P2b — radar UI
- Round radar of the node's **own** recent detections (keep a small local table, e.g. reuse the
  pattern of `src/godzilla_det.h` in the node tree — don't edit `src/`).
- Radius from RSSI, angular slot stable per address (hash), **family colours matching the
  Cardputer view** (#54): surveillance/Remote ID red, trackers amber, rest neutral.
- Touch a dot -> detail card (family, RSSI, address, age, hits). Swipe -> status screen.
- Deepwater palette and Poseidon iconography (brief §4.3); no Dragon Detector art.

## P2c — pocket-watch face (Dale's spec, brief §4.3)
- Functional old-fashioned pocket watch: Roman or classic serif numerals, ornate bezel/fob,
  moving hour/minute/second hands **driven by the PCF85063 RTC** (correct across power cycles).
- Time source for now: set the RTC from the build timestamp if invalid; a time-sync message from
  the Cardputer is a later wire addition (ask first — it would be a new type, not a change).
- Original artwork only (brief §6: no copied watch faces).

## Follow-ups noted at review (fold into P2a, or own small PR)
1. **ESP-NOW send vs channel restore** (`gz_link_send`, #62): `esp_now_send()` only queues;
   restoring the scan channel immediately may move the radio before the frame airs. The C5 node
   does the same and works in the field, so it's a risk, not a known bug. Add a send-complete
   wait (`esp_now_register_send_cb` + short bounded wait, e.g. 10–20 ms) before restoring, and
   report published-vs-received on the bench.
2. Batch key: the node dedups BLE by (addr, addr_type), the S3 by addr only — harmless (rows merge
   on the S3); no change needed.

## Rules (unchanged)
Passive only; nothing transmits except ESP-NOW HELLO/publish. No C5/wire change without asking.
`src/*` read-only. Tests first for pure logic. CI green incl. `godzilla`. One topic per PR;
each PR lists what needs Dale's bench. Claudius reviews and merges. P3 (labs) and P4 (other faces,
themes) wait for Dale.
