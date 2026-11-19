# GODZILLA P1 — node producer (task brief)

**For:** GODZILLA (PSPE on the node). **From:** Claudius Maximus (overseer), 2026-11-18, at Dale's
direction ("keep Godzilla busy"). **Status:** in progress — task 2 done (#50).

## Ground truth on master
- Wire: `RESP_WIFI_DET` 60 / `RESP_BLE_DET` 61, header v5 frozen (#43); families 0–11 incl.
  RAVEN 9, TILE 10, SMARTTAG 11 (#46).
- Consumer contract: [`docs/godzilla-detection-publish-contract.md`](../godzilla-detection-publish-contract.md)
  (#45) — **exact length** `payload_len == 2 + 10*count`, `count <= 64`.
- S3 receiver merged (#44): `src/godzilla_det.h` + `c5_dets()`; rejects anything off-contract.
- Shared classifier (#47): `src/det_classify.h` → `det_classify_ble(adv, len)` returns `DET_FAM_*`
  (== wire family) from a raw advertising payload.

## Task (one topic per PR, tests first)
1. **Env + skeleton.** `[env:godzilla]` in `platformio.ini` for the Waveshare
   ESP32-S3-Touch-LCD-1.85B; sources in `godzilla_node/` (in-tree). Must not change any
   `cardputer*` env or `c5_node`.
2. **Batching core (pure, native-tested).** Accumulate detections, dedup by address within a
   batch, emit `posei_ble_det_t` / `posei_wifi_det_t` with `count <= POSEI_DET_MAX` and
   `payload_len = posei_*_det_bytes(count)`; >64 → several batches, same `node_id`.
   Native tests in `test/test_godzilla_batch/`.
3. **Radio loop.** Passive NimBLE scan → `det_classify_ble(getPayload())`; passive Wi-Fi scan →
   AP (`WIFI_AP`) / STA (`WIFI_STA`). Publish over ESP-NOW v2 on a timer; send HELLO like the C5
   so the S3 registers the peer.

## Hard limits
- **Passive only.** Nothing here transmits other than ESP-NOW publish/HELLO; nothing goes behind
  or around TX-arm. No C5 change; header stays v5.
- **Don't edit** `src/det_classify.h`, `src/godzilla_det.h`, `src/odid_decode.h`,
  `src/features/*` — Claudius is extracting the Remote ID decoder and the Flock frame check in
  parallel. Need a change there? Say so in your PR description.
- CI green (`build.yml` + native). Claudius reviews and merges.

## Done when
`pio run -e godzilla` builds in CI, native batching tests pass, and the PR notes what still needs
Dale's bench (first ESP-NOW frames landing in `c5_dets()` on the Cardputer).

## Update 2026-11-18 21:55 ET — shared headers ready (Claudius)
Task 2 (batching core) merged as #50. The reuse extraction is complete, so the node can use all
three shared classifiers. They are **read-only for GODZILLA** (ask in a PR if one needs a change):

| Header | Use on the node |
|---|---|
| `src/det_classify.h` (#47, #48) | `det_classify_ble(getPayload())` → BLE family (Remote ID, Raven, Find My, Tile, SmartTag, iBeacon, generic). |
| `src/odid_decode.h` (#51) | Optional for P1. `odid_decode_ble_svc()` for 0xFFFA service data if the node later shows UAS ID / position. Fixes the 2-byte framing bug the Cardputer had. |
| `src/flock_frame.h` (#52) | Wi-Fi loop: promiscuous mgmt callback → `flock_frame_classify(payload, sig_len, &fr)` → `gz_wifi_batch_add(fr.addr, ch, rssi, flock_frame_family(&fr))`. Tier-2 prefix alone stays AP/STA by design. |

**Next for GODZILLA, in order:**
1. **Task 1** — `[env:godzilla]` + `godzilla_node/` skeleton; CI must build it (add the env to
   `build.yml`'s matrix in the same PR).
2. **Task 3** — passive radio loop: NimBLE passive scan + Wi-Fi promiscuous (mgmt only, channel
   hop 1–13), both feeding `gz_batch.h`; publish timer (~1 s) packs, sends over ESP-NOW v2, resets;
   HELLO like the C5. Keep the ISR/callback path allocation-free; enqueue, don't publish, from the
   promiscuous callback.
3. In the task-3 PR, list Dale's bench steps: flash the 1.85B, then watch `c5_dbg_det_frames()` /
   `c5_dets()` rise on the Cardputer.

Still: passive only, no C5 change, header v5. One topic per PR; Claudius reviews and merges.
