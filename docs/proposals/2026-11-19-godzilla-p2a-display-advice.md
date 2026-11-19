# GODZILLA — advice for the P2a display/status-screen PR

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19 00:10 ET, at Dale's request ("Advise
Godzilla"). Context: #65 (pin map) merged; your stack choice — `ESP32_Display_Panel` +
`esp_lcd_st77916` + LVGL 9 — is **accepted**. Items marked *verify* are my best understanding,
not checked on this board: confirm them and say so in the PR.

## 1. Verification from the #65 review
- LCD (CS 21, PCLK 40, D0–D3 46/45/42/41, RST 3, BL 5) and TP_INT 4 / RTC_INT 6 match Waveshare's
  1.85B page. **Not yet confirmed:** I2C SCL 10 / SDA 11 and TP_RST 1.
- **Add a boot-time I2C scan** (print found addresses to serial). Expect 0x15 (CST816S) and 0x51
  (PCF85063). That settles the three unconfirmed pins on Dale's first flash.

## 2. `env:godzilla` build config
- ESP32-S3R8 = 8 MB **octal** PSRAM + 16 MB flash. *Verify* for pioarduino 55.03.38:
  `board_build.arduino.memory_type = qio_opi`, `board_upload.flash_size = 16MB`,
  `board_build.partitions = default_16MB.csv`, `-DBOARD_HAS_PSRAM`. Boot log should report
  ~8 MB PSRAM; fail loudly (serial) if `ESP.getPsramSize()` is 0.
- Pin every new lib to an exact version (`ESP32_Display_Panel`, its `ESP32_IO_Expander` dep if
  pulled, `lvgl/lvgl @ 9.x.y`). No `^` ranges for display libs — API churn between minors is real.
- LVGL config: one `lv_conf.h` under `godzilla_node/` (`LV_CONF_INCLUDE_SIMPLE`), 16-bit colour,
  draw buffers in PSRAM, `LV_USE_LOG` off in release. Keep fonts to what the status screen needs.

## 3. Panel bring-up
- `esp_lcd_st77916` ships a default init table; many ST77916 panels need the **vendor's** table.
  *Verify* against Waveshare's 1.85B Arduino/ESP-IDF demo and use theirs if it differs; cite it.
- QSPI clock: start conservative (~40 MHz), raise only after a clean bench run.
- Backlight on GPIO5 via LEDC PWM; default ~70 %, off until first frame is drawn (no white flash).
- Round panel: 360×360 buffer, but keep content inside the circle (inscribed radius 180 px).

## 4. Tasks and the radio
- Run LVGL in its **own FreeRTOS task** (`lv_timer_handler` every ~5–10 ms) on core 1. The Arduino
  loop must keep calling `gz_ble_scan_tick()` / `gz_wifi_scan_tick()` on schedule — never block it
  on rendering or SPI flushes.
- NimBLE host runs on core 0; the promiscuous callback is ISR-like. Neither may touch LVGL. Pass
  counters to the UI via atomics or a small locked struct read by the UI task.
- PSRAM bandwidth: QSPI flush from PSRAM buffers is fine; keep ESP-NOW and staging batches in
  internal RAM (they already are — static).

## 5. Status screen (orders P2a)
Node name, uptime, link channel, BLE and Wi-Fi: frames published + entries in last batch, last
publish age, free heap/PSRAM, I2C scan result. Deepwater palette; text only for now (radar is P2b).

## 6. Touch and RTC (can be the next PR)
- CST816S via `ESP32_Display_Panel`'s touch driver if it supports it on this pin map; else a
  minimal register driver. Map taps to LVGL input.
- PCF85063: keep the BCD/time conversion **pure with native tests** (`test/test_godzilla_rtc`);
  only the I2C read/write is on-target. If the oscillator-stop flag is set, seed from build time.

## 7. Carry-over follow-up
- From #62: add an ESP-NOW **send-complete wait** (`esp_now_register_send_cb` + bounded wait,
  10–20 ms) before `gz_link_send()` restores the scan channel. Small, own PR; report
  published-vs-received on the bench (Cardputer C5 -> x `frames`).

## 8. PR hygiene (unchanged)
Passive only. No `src/*` edits, no C5/wire change. Original artwork only. CI green incl.
`godzilla`. List Dale's bench steps: `pio run -e godzilla -t upload`, serial at 115200 for the
PSRAM size + I2C scan, then what the screen should show. Claudius reviews and merges.
