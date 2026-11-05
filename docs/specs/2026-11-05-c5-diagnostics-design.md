# C5 Diagnostics Screen — Design Spec

_Status: proposed · 2026-11-05 · target: Cardputer (S3) feature + optional C5 firmware addition_

## 1. Goal

A Cardputer-side **C5 Diagnostics** screen that shows, at a glance, whether each
connected C5/TRIDENT node is healthy and how the ESP-NOW link is performing —
without plugging the C5 into USB. It complements the existing **Status** screen
(peer list) with live link + health telemetry.

## 2. Non-goals

- Not a Web UI. (Rejected: the C5's radio channel-hops for scans and sits on
  ch1 for ESP-NOW, which contends with a stable SoftAP; a Cardputer-hosted web
  server previously failed on heap — `AsyncTCP`/`ESPAsyncWebServer` disabled in
  `platformio.ini`.)
- Not a control panel — read-only diagnostics plus a manual re-ping. Commands
  stay in their own feature screens.
- No new wire protocol in Phase A (keep `POSEI_VERSION == C5_VERSION == 4`).

## 3. Data inventory — what's already available vs. what's missing

| Field | Source today | Phase A plan |
| --- | --- | --- |
| Node name | `c5_peer_t.name` (from HELLO) | show |
| Online / last-seen age | `c5_peer_t.last_seen`, `c5_any_online()` | show (age = `millis() - last_seen`) |
| Capabilities 5G / 15.4 | `c5_peer_t.has_5g / has_ieee802154` | show as badges |
| Peer count | `c5_peer_count()` | show |
| Free heap (KB) | **on the wire in HELLO (`heap_kb`) but NOT persisted** per peer | add `heap_kb` to `c5_peer_t`, set in `handle_hello` — zero protocol change |
| Link RSSI | **available via `recv_info->rx_ctrl->rssi` in `on_recv`, not captured** | capture per-peer in `on_recv` — zero protocol change |
| Ping RTT | measurable: `c5_cmd_ping()` → `RESP_PONG` | timestamp the ping, measure RTT on PONG |
| Channel / frames | `RESP_STATUS` (`c5_status_channel()/c5_status_frames()`) — only populated during an attack/scan | show when available, else "—" |
| C5 firmware / proto version | **not on the wire** (HELLO lacks it) | Phase B only |
| Uptime / reset reason / task high-water | not on the wire | Phase B only |

## 4. UI design (Cardputer, 240×135)

New feature `feat_c5_diag()` in `src/features/c5_scan.cpp` (sibling of
`feat_c5_status`), menu entry under **C5 nodes** (suggest key `g` "Diagnostics").
Dashboard-loop pattern like the other C5 screens (`ui_dashboard_chrome("C5 DIAG",
changed)` + 300 ms redraw throttle; radar is now upper-right app-wide).

Per-node row (one block per peer, up to the peer cap):
```
C5-5980         [5G][15.4]   ●online
  rssi -58dBm   heap 212KB   seen 2s
  ping 7ms      ch 1         frames 1.2k
```
- `●` green/red from `c5_any_online`-style freshness (<30 s).
- RSSI colour-coded (green > -60, amber -60..-80, red < -80).
- heap trend arrow (↑/↓/–) by comparing to the previous sample (optional).
- Footer: `` `=back   p=ping all ``. Pressing `p` calls `c5_cmd_ping()` and
  updates the RTT column (reuse the Status screen's ping handling).

Empty state: "no C5 online — press p to ping" (mirror `feat_c5_status`).

## 5. S3 implementation touch points (Phase A, zero protocol change)

1. `src/c5_cmd.cpp`
   - `c5_peer_t`: add `uint16_t heap_kb;` and `int8_t rssi;`.
   - `handle_hello`: store `h->heap_kb` into the peer.
   - `on_recv`: capture `recv_info->rx_ctrl->rssi` into the peer on any frame
     (same spot that refreshes `last_seen`). Guard for the IDF recv-info variant
     already handled at `c5_cmd.cpp:292`.
   - Ping RTT: record `millis()` when `c5_cmd_ping()` sends; on `RESP_PONG`
     compute and store `rtt_ms` per peer (match PONG to the pinged node by MAC).
   - New accessors: `int c5_diag(c5_diag_t *out, int max)` returning a packed
     per-peer snapshot (`name, online, last_seen_age, rssi, heap_kb, has_5g,
     has_ieee802154, rtt_ms, channel, frames`), plus `c5_diag_t` in `c5_cmd.h`.
2. `src/features/c5_scan.cpp`: add `feat_c5_diag()`; reuse `draw_status_header`,
   the dashboard chrome, and the `c5_present()` recovery guard.
3. `src/menu.cpp`: `extern void feat_c5_diag(void);` + a `MENU_C5` entry (`g`).

No `c5_node/` firmware change in Phase A. No new ESP-NOW types.

## 6. Phase B (optional, later — versioned protocol bump)

Add a `CMD_DIAG` / `RESP_DIAG` pair for data the C5 can only self-report:
firmware + proto version, uptime, last reset reason (`esp_reset_reason()`),
min free heap, per-task stack high-water, and per-radio health (Wi-Fi + 802.15.4
init state). This is a **flag-day protocol change** → bump `POSEI_VERSION ==
C5_VERSION` together, add the structs to both `proto.h` and `c5_cmd.h`, and a
C5 handler that fills the struct from `esp_*` introspection APIs. Gate the S3
screen to show the richer fields only when the peer answers `RESP_DIAG` (older
C5s just show the Phase A subset).

## 7. Acceptance

- Phase A: with a C5 online, the screen shows live RSSI, heap, last-seen, and a
  ping RTT that updates on `p`; a C5 powered off flips to offline within ~30 s;
  no protocol/version change (`== 4`), no C5 reflash required.
- Phase B (if built): firmware/proto version and uptime appear for a C5 running
  the new firmware; an old C5 still shows the Phase A fields without error.

## 8. Risks / notes

- `rx_ctrl` availability differs across the IDF recv-info variants the code
  already branches on (`c5_cmd.cpp:292`); capture RSSI only on the variant that
  carries it, default to 0/"—" otherwise.
- Channel/frames come from `RESP_STATUS`, which the C5 only emits during an
  active attack/scan — show "—" at idle rather than a stale value.
- Keep the screen read-only except for ping, so diagnostics never perturb a
  running capture on the node.
- Effort: Phase A is ~1 feature screen + ~4 small `c5_cmd` additions; fits as a
  Tier 2 item after PR #7. Phase B is a protocol flag-day — schedule
  deliberately.
