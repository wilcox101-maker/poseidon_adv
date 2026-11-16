# Hardware Validation Runbook — ESP-NOW v2 / wire protocol v5

_Turnkey procedure for the one open release gate: confirming the v5 flag day
actually works on hardware. Target audience: whoever has the Cardputer + C5 on
the bench. Logs as **POS-AUDIT-127** in `docs/POS-AUDIT-LOG.md`._

## Why this exists

The v5 bump (ESP-NOW v2, variable-length framing, 1024-byte payload) is proven
in CI — both firmwares build, `test/test_proto` pins the wire layout — but has
**never run on real silicon**. The C5's Wi-Fi 6 stack is newer than the S3's, so
v2 negotiation parity between the two chips is *assumed, not measured*. This is
the dominant open technical risk; everything below is the minimum to clear it.

Nothing here needs a second pair of hands or external hardware beyond the two
boards. Flashing is at the operator's discretion (devices connect to Dale's PC).

## Prerequisites

- 1× M5Stack Cardputer-Adv (ESP32-S3), 1× ESP32-C5 board, USB-C cables.
- Cardputer firmware: `pio run -e cardputer` (Arduino 3.3.8 / IDF 5.5.4).
- C5 firmware: ESP-IDF **5.5.x** (`C:\esp\v5.5.5` locally; CI uses release-v5.5),
  `idf.py -C c5_node build`. **Do not** use an IDF older than 5.4 — v2 won't exist.
- Recovery escape hatch if a flash goes bad: hold **BOOT (GPIO28)** on the C5 /
  **BOOT (GPIO0)** on the Cardputer, tap RESET, release — re-flash from ROM
  download mode. See `docs/flash/index.html`. You cannot brick either board from
  software.

## Flash both chips (safe paths)

- **Cardputer:** web-flasher factory image (`docs/manifest.json` → offset 0 +
  erase) is the safe path. Avoid raw-offset `write_flash` into the dual-OTA
  launcher — that's the documented brick near-miss.
- **C5:** `idf.py -C c5_node flash` writes all three parts, or use the
  web-flasher (`docs/flash/`). The committed partition table is now the 2 MB
  factory layout (CI guards it against `partitions.csv`).

## The checks

Run in order. **A is the gate** — if A fails, stop and diagnose before anything else.

### A — v2 negotiated on BOTH chips (the gate)

Both firmwares log `esp_now_get_version()` at boot (added with the v5 bump).

1. Open the C5 UART0 console; reset; confirm the boot log shows **`ESP-NOW version=2`** (and/or `ESPNOW: espnow [version: 2.0] init`).
2. Open the Cardputer serial console; reset; confirm the same **`version=2`** line.

**Pass:** both say 2. **Fail (either says 1):** that chip fell back to v1 — v5
frames over 250 B will truncate/drop on it. Check: IDF ≥ 5.4 on that build, Wi-Fi
inited before `esp_now_init()`, no custom `ESP_NOW_MAX_DATA_LEN` override. Do not
proceed to a release with a v1 fallback on either side.

### B — v5 link is live

3. With both powered, confirm the C5 appears in the Cardputer's **C5 → Status**
   (HELLO handshake completed under v5 — a v4 peer would be rejected on the
   version check, so a visible peer *is* a v5 handshake).
4. Trigger a **PING**; confirm a PONG / RTT comes back (small-frame path, ~10 B
   on air under variable-length framing).

### C — headroom path works (large frame)

5. Run **Scan 5G+2G**; confirm AP results arrive and the screen populates. Under
   v5 the RESP_AP batch carries ~23 records/frame (was ~5) — watch for the
   previously-flagged "Scan 5G+2G a little slower" (POS-AUDIT-125/126) being
   gone. This exercises a genuinely large v2 payload end-to-end.

### D — SoftAP ↔ ESP-NOW coexistence

6. Start **AP Clone** or **Karma** on the C5 (arm TX first). Confirm the SoftAP
   comes up *and* the ESP-NOW link to the Cardputer stays alive (Status still
   shows the peer, PING still answers). This is the coexistence path that a
   Cardputer-hosted web UI previously failed on heap — confirm the C5 side holds.

### E — 2 MB partition boots clean

7. In the C5 boot log, confirm no partition-table errors and that NVS data
   survives a reflash (pairing/state persists). Confirms the 1 M→2 M factory
   change boots on real silicon, not just in `gen_esp32part`.

## Pass / fail + recording

- **All of A–E pass →** the v5 gate is cleared; a release candidate may carry v5.
- Log the result as **POS-AUDIT-127** in `docs/POS-AUDIT-LOG.md` (next free
  number): date, both boot `version=` values, which checks passed, and any
  anomaly. A fail is still logged — it's the record of what the bench showed.
- Per `release-versioning-policy.md` (relaxed 2026-11-15) this is a confidence
  gate for the v5 release, not a blocker on continued development.

## Coverage limits (known, not fixable here)

- **Single unit of each board.** Multi-node behavior — capability-based peer
  admission (PR #15) with >1 satellite, the multi-node wire fields — can't be
  exercised with one C5. The 2× bare ESP32-S3 N8R16 boards can mock a second
  *peer* on the ESP-NOW bus (HELLO/admission) but not C5-specific radio work.
- **Cross-IDF pairing.** Cardputer (IDF 5.5.4 via Arduino) ↔ C5 (IDF 5.5.5). A
  passing check A *is* the cross-version v2 interop evidence — there's no other
  way to confirm it than this bench run.
