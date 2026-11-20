# Cardputer → node time-sync (design)

**Decisions (Dale, 2026-11-19):** build the Cardputer→node time-sync (adds one additive ESP-NOW
message type). Watch-face backgrounds stay **black + carbon-texture only** — no custom images.
ES3C28P bench: **not yet**.

> **Revised (Dale, 2026-11-20): "No RTC no clock faces."** A board without an RTC offers no clock
> faces at all (see `2026-11-20-godzilla-rtc-face-gating-orders.md`), so the RTC-less soft-clock
> path below is dropped. Time-sync is now **RTC-boards only**: the Cardputer sets a node's hardware
> RTC so its clock faces read true. The wire type and sender are unchanged and already shipped.
>
> **Status:** steps 1 (#120) and 2 (#132) merged. Step 3 is GODZILLA order E, RTC-boards only
> (`2026-11-20-godzilla-timeset-consumer-orders.md`).

## Why
A node's RTC can be unset or drifted. The Cardputer already **sends** commands to nodes, so a
time-set is just one more downlink: it sets the node's RTC and its clock faces read true. (Original
driver — giving the RTC-less ES3C28P a soft clock — is withdrawn; that board now shows no clock
faces.)

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
- **GODZILLA (node):** consume TIMESET on RTC boards — set the hardware RTC and publish it so the
  clock faces read true; drop the "set via Cardputer" placeholder once a sync has arrived. No
  soft-clock / RTC-less path (an RTC-less board has no clock faces). Pure time math native-tested.

## Sequence
1. ✅ Claudius PR #120: wire type + payload + parity-test pair (no behaviour yet).
2. ✅ Claudius PR #132: Cardputer sender — HELLO-driven unicast to nodes (incl. detection nodes the
   broadcast path skips), gated on a valid GPS-seeded clock, resync-throttled.
3. GODZILLA order E: node consumer (RTC boards only) — `2026-11-20-godzilla-timeset-consumer-orders.md`.

Passive throughout; nothing here arms a transmitter. The sender reuses the Cardputer's existing
ESP-NOW peer/send path — no new radio behaviour beyond a unicast it already does for commands.
