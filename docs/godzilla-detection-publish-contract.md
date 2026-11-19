# GODZILLA detection-publish — consumer contract (types 60/61)

**For:** the S3-side receive handler (owned by Claudius Maximus) that consumes GODZILLA node
detections. **From:** PSPE (producer/wire side). **Date:** 2026-11-18.

P0 (#43) landed the wire. This is the seam contract so the S3 receiver and the node producer agree
without re-reading each other's code. The authoritative types live in `c5_node/main/proto.h`, kept
**byte-identical** with the S3-side `src/c5_cmd.h` (pinned by literal-size `static_assert`s).

## Types
- `POSEI_TYPE_RESP_WIFI_DET` = **60** — GODZILLA→S3 Wi-Fi detection batch.
- `POSEI_TYPE_RESP_BLE_DET`  = **61** — GODZILLA→S3 BLE detection batch.

Both are **additive**: the C5 never emits them, so its on-air format is unchanged (approach B). The
v5 header and `POSEI_VERSION` are frozen.

## Payloads (S3 reads via the `c5_*` mirror in `src/c5_cmd.h`)
```
c5_wifi_det_t { u8 node_id; u8 count; c5_wifi_det_entry_t det[C5_DET_MAX]; }  // C5_DET_MAX = 64
c5_wifi_det_entry_t { u8 bssid[6]; u8 channel; i8 rssi; u8 family; u8 flags; }  // 10B
c5_ble_det_t  { u8 node_id; u8 count; c5_ble_det_entry_t  det[C5_DET_MAX]; }
c5_ble_det_entry_t  { u8 addr[6]; u8 addr_type; i8 rssi; u8 family; u8 flags; }  // 10B
```
`family` = `C5_DET_FAM_*` (UNKNOWN=0, WIFI_AP=1, WIFI_STA=2, BLE_GENERIC=3, FINDMY=4, AIRTAG=5,
IBEACON=6, REMOTEID=7, FLOCK=8, RAVEN=9, TILE=10, SMARTTAG=11). `flags` reserved, 0 in P0. `addr_type`: 0 public, 1 random.

## Receiver rules (the handler Claudius is writing)
1. Dispatch on type **60/61** in the existing `on_recv` path, after the standard magic + `version==5`
   checks.
2. **Validate length before trusting `count`:** require `payload_len == c5_wifi_det_bytes(count)`
   (resp. `c5_ble_det_bytes`). Reject/trim a batch whose `count > C5_DET_MAX` — the helpers clamp,
   but the handler should not index past `count`.
3. **Attribution:** `node_id` is carried in the payload (approach B — the header has no source id).
   Map `node_id` ↔ sender MAC using the HELLO peer table for display/aggregation.
4. **Passive — never TX-gated.** These are detections; nothing here touches TX-arm.
5. **Forward-compat:** a pre-P0 peer never sends 60/61; an unknown type is dropped, as today.

## Producer rules (the GODZILLA node, later phase)
- Fill `node_id`, `count`, then `count` entries; set `payload_len = posei_wifi_det_bytes(count)`
  (resp. BLE) so only the populated prefix rides the wire.
- More than `POSEI_DET_MAX` (64) detections → send several batches, same `node_id`.

## Out of scope here
Aggregation/UI, radar, and the node firmware itself — later phases. This doc is the wire/handler
seam only.
