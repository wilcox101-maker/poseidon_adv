# Cardputer → node time-sync (design)

**Decisions (Dale, 2026-11-19):** build the Cardputer→node time-sync (adds one additive ESP-NOW
message type). Watch-face backgrounds stay **black + carbon-texture only** — no custom images.
ES3C28P bench: **not yet**.

## Why
The ES3C28P has no RTC, so its Classic face reads "set via Cardputer". A node can only publish
passively, but the Cardputer already **sends** commands to nodes; a time-set is just one more
downlink. With it, any node (RTC-less, or an RTC that drifted/was never set) shows real
clock/date/moon.

## Wire (additive — header stays v5, passive)
New **downlink command** type, next free number in the command range, mirrored in both headers:
`POSEI_TYPE_CMD_TIMESET` (c5_node/main/proto.h) == `C5_TYPE_CMD_TIMESET` (src/c5_cmd.h).
Payload, fixed and little-endian:
- `uint32_t epoch` — UTC seconds (2026+ fits u32 fine)
- `int16_t  tz_offset_min` — local offset from UTC, for display only
This is additive: existing types and struct sizes are untouched, so no flag day. The node only
**receives** it — it transmits nothing new, so the passive rule holds.

## Lane split
- **Claudius (main-firmware / wire):** add the type + payload to `proto.h` and `c5_cmd.h`; extend
  the parity test (test_c5_parity) with the new pair; add the Cardputer **sender** — push TIMESET
  to a node when it HELLOs (fresh pairing) and whenever the Cardputer's own clock is set/changed.
- **GODZILLA (node):** consume TIMESET — on a board with an RTC, set it; on the RTC-less ES3C28P,
  keep a soft clock seeded by TIMESET and ticked locally. The Classic face then shows real time;
  drop the "set via Cardputer" placeholder once a sync has arrived. Pure time math native-tested.

## Sequence
1. Claudius PR: wire type + payload + parity-test pair (no behaviour yet).
2. Claudius PR: Cardputer sender (on HELLO + on clock-set).
3. GODZILLA order: node consumer + Classic-face wiring (after its current UI units).

Passive throughout; nothing here arms a transmitter. The sender reuses the Cardputer's existing
ESP-NOW peer/send path — no new radio behaviour beyond a unicast it already does for commands.
