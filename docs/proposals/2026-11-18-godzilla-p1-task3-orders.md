# GODZILLA — orders: P1 task 3 (passive radio loop + publish)

**For:** GODZILLA (new chat). **From:** Claudius Maximus, 2026-11-18 22:15 ET, at Dale's direction.
Task 1 is merged (#56: `[env:godzilla]` + skeleton, in CI). Task 2 is merged (#50). **Start task 3 now.**

## Answers to the open questions (#45 / #57)
1. **Where detections surface:** the existing C5 results store. The S3 already merges 60/61 into
   `godzilla_det.h`'s table via `c5_cmd.cpp`, exposed as `c5_dets()` + `c5_dbg_det_frames()` /
   `c5_dbg_det_rejects()` (#44). Bench screen: **C5 → x "GODZILLA dets"** (#54). No new module.
2. **Ack:** **fire-and-forget** for P1. The S3 sends no ack for 60/61; the node publishes on a
   timer and the next batch supersedes the last. Revisit only if the bench shows real loss.

## Task 3 — split into three PRs, in order
**3a. ESP-NOW link + HELLO** (`godzilla_node/main/`)
- Wi-Fi STA mode, channel 1 (the C5 link channel), `esp_now_init`, broadcast peer.
- HELLO every 5 s exactly as `c5_node` does (`POSEI_TYPE_HELLO`, `role` ≠ 1, name `"GODZILLA"`,
  `has_5g = 0`, `has_ieee802154 = 0`, heap). Confirm the S3 lists it in C5 → Diagnostics.

**3b. Passive BLE → batch → publish**
- NimBLE **passive** scan (no scan requests), continuous. Callback: `det_classify_ble(payload)`,
  then `gz_ble_batch_add()` into a small lock-protected staging area. No allocation in callbacks.
- 1 s publish timer on the main loop: pack (`gz_ble_batch_pack`), `esp_now_send` broadcast,
  reset. On `GZ_ADD_FULL`: pack, send, reset, re-add (as `gz_batch.h` documents).

**3c. Passive Wi-Fi → batch → publish**
- Promiscuous, **management frames only**, hop 1–13 at ~250 ms, **returning to channel 1 for each
  publish** so ESP-NOW reaches the S3 (state this trade-off in the PR).
- Callback: `flock_frame_classify(payload, sig_len, &fr)`; on `>= 0`,
  `gz_wifi_batch_add(fr.addr, ch, rssi, flock_frame_family(&fr))`. Enqueue only; publish on the loop.

## Each PR must
- Stay passive: nothing transmits except ESP-NOW HELLO/publish. No deauth, no beacon, no probe
  requests. No C5 change; header v5; exact length per the contract.
- Keep `pio run -e godzilla` and all other envs green; add native tests for any new pure logic.
- Leave `src/*` alone (read-only for the node). Need a change? Say so in the PR.
- List **Dale's bench steps** (from 3b on): flash the 1.85B via USB, open C5 → x on the Cardputer,
  watch `frames` rise and `rej` stay 0.

## Notes from the old chat (#57)
- Its PC note: ESP-IDF v6.0.2 vs documented v5.5.4 — relevant to TRIDENT (`c5_node`), not to
  GODZILLA (Arduino/pioarduino). Not your concern.

Claudius reviews and merges each PR. One topic per PR.
