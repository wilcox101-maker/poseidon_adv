# POSEIDON GODZILLA node

Full-UI detection head for the Waveshare ESP32-S3-Touch-LCD-1.85B. It scans passively on its own
2.4 GHz radio and publishes what it sees to the Cardputer over ESP-NOW v2, as another node on the
POSEIDON fabric. Poseidon-dependent: no standalone release — it rides Poseidon's CI, versioning and
wire protocol (`c5_node/main/proto.h`).

Plan and scope: [`docs/proposals/2026-11-18-godzilla-node.md`](../docs/proposals/2026-11-18-godzilla-node.md).
Wire contract: [`docs/godzilla-detection-publish-contract.md`](../docs/godzilla-detection-publish-contract.md).

## Status

| Piece | State |
|---|---|
| `main/gz_batch.h` — detection batching core | done, native-tested (`test/test_godzilla_batch`) |
| `[env:godzilla]` + `main/main.cpp` skeleton | done, builds in CI (`build.yml` matrix) |
| ESP-NOW link + HELLO (`main/gz_link.*`, `main/gz_hello.h`) | task 3a — STA ch1, broadcast peer, HELLO every 5 s; HELLO builder native-tested (`test/test_godzilla_hello`) |
| Passive BLE scan → batch → publish (`main/gz_ble_scan.*`) | task 3b — continuous NimBLE passive scan → `det_classify_ble` → `gz_batch.h`, broadcast every ~1 s; publish-timing predicate native-tested (`test/test_godzilla_publish_due`) |
| Passive Wi-Fi scan → batch → publish (`main/gz_wifi_scan.*`) | task 3c — promiscuous mgmt capture hopping 1..13 → `flock_frame_classify` → `gz_batch.h`, broadcast every ~1 s (returns to ch1 to send); hop step native-tested (`test/test_godzilla_wifi_hop`) |
| Board pin map (`main/gz_board.h` → `main/gz_board_185b.h`) | task 2a — cited 1.85B pins (LCD/touch/RTC), collision-guarded, native-tested (`test/test_godzilla_board`) |
| Display + status screen (`main/gz_display.*`, `main/gz_ui.*`, `main/gz_stats.*`) | task 2a — ST77916 QSPI via vendored `esp_lcd_st77916` + Waveshare's two panel-revision init tables (picked from reg 0x04), LVGL 9.2.2 in its own task; status screen from publish counters. Rev pick + status formatters native-tested (`test/test_godzilla_display_rev`, `test/test_godzilla_status`). |
| RTC + touch (`main/gz_rtc.h`, `main/gz_touch.h`) | P2a — PCF85063 clock seeded from build time on oscillator stop; CST816S touch as an LVGL pointer, INT-gated reads. Pure logic native-tested (`test/test_godzilla_rtc`, `test/test_godzilla_touch`) |
| Radar (`main/gz_radar.h`, `main/gz_radar_store.*`, `main/gz_ui_radar.*`) | P2b — home screen: the node's own published detections, radius from RSSI, stable hashed bearing (a layout slot, not a direction), Cardputer family colours (red surveillance / Remote ID, amber trackers). Tap a dot for a detail card; swipe left/right for the status screen. Table + geometry native-tested (`test/test_godzilla_radar`) |
| Pocket watch (`main/gz_watch.h`, `main/gz_ui_watch.*`) | P2c — brass-and-ivory watch face from LVGL primitives: Roman numerals (IIII), ticking second hand, software clock resynced from the PCF85063 about once a minute. Long-press sets the time (tap upper half hour +1, lower half minute +1, long-press writes the RTC, 15 s idle cancels). Swipe ring radar → hunt → watch → status. Model native-tested (`test/test_godzilla_watch`) |
| Recon log (`main/gz_recon.h`, `main/gz_recon_task.*`, `main/gz_clock.*`) | P3 — microSD (SDMMC 4-bit) session log. Long-press the radar to start or stop; detections are written to `/recon/<year>/<MMDDHHMM>.CSV`, one row on first sight and then at most every 30 s per device. Never formats the card. Policy, queue, paths and CSV native-tested (`test/test_godzilla_recon`) |
| Hunt view (`main/gz_hunt.h`, `main/gz_ui_hunt.*`) | P3 — RSSI trace for one contact. Tap a radar dot, hold its card: the Hunt screen shows family, address, radio, latest RSSI, a stronger / weaker / steady trend and the last 60 readings (~1 min); "lost" after the 60 s TTL, hold to stop. Fed in the radar store's critical section from the published batches. A strength meter, not a direction finder. Ring, trend, lost state and trace geometry native-tested (`test/test_godzilla_hunt`) |
| Second board — AMOLED-2.06 (`main/gz_board.h`, `main/gz_board_amoled206.h`, `[env:godzilla-amoled206]`) | Port unit 1 — board chosen at build time (`-DGZ_BOARD_AMOLED206`); 1.85B map unchanged in `gz_board_185b.h`. The 2.06 builds headless in CI (radios, link, publish, RTC); CO5300 display, FT3168 touch and the SPI TF slot follow in units 2-5 per `docs/proposals/2026-11-19-godzilla-amoled206-port.md`. HELLO name `GODZILLA206`. Map native-tested (`test/test_godzilla_board_amoled206`) |

## Board

Pin map for the Waveshare ESP32-S3-Touch-LCD-1.85B lives in `main/gz_board.h`, cited from the
Waveshare wiki ([ESP32-S3-Touch-LCD-1.85B](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B)).
This is the **B** revision (LCD reset on GPIO3, not an I/O expander) — do not mix it with the plain
1.85 map. Peripherals: ST77916 display (QSPI, 360×360), CST816S touch and PCF85063 RTC (shared I2C
bus, SCL=10 / SDA=11).

Nothing here has run on hardware yet.

## Rules

- **Passive only.** The node transmits ESP-NOW publish/HELLO frames and nothing else. Any future
  transmitter stays behind Poseidon's master TX-arm gate ([`docs/tx-arm.md`](../docs/tx-arm.md)).
- The wire header stays v5; the C5 node's on-air format is not touched by anything in this directory.

## Credit

The node's feature set is a native reimplementation of
[Dragon Detector](https://github.com/valleytechsolutions/Dragon-Detector) (MIT) by Your Pal Kal /
Valleytech Custom Solutions. No Dragon Detector source, binaries or artwork are included.
