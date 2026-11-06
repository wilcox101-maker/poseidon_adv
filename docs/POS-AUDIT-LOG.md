# POS-AUDIT Log

Tracked hardware-validation log for poseidon_adv. This is the formal
record referenced by the Poseidon ADV claude.ai project's
`release-versioning-policy.md` (validation gate) and
`test-matrix-framework.md` (framework this log implements).

Append new entries at the bottom, oldest first. Don't renumber or
reorder past entries — if a result needs correcting, add a new row
that supersedes it and say so in Notes, rather than editing history.

## Schema

| Column | Meaning |
| --- | --- |
| ID | `POS-AUDIT-###`, assigned sequentially, never reused |
| Date | `YYYY-MM-DD` |
| Trigger | `flag-day` (post individual risky change), `band-end` (full band done), or `release` (pre-tag full sweep) |
| Area | One row of the current test matrix (see `test-matrix-framework.md`) |
| Hardware | Which physical unit(s) ran the test |
| Result | `pass`, `fail`, or `intermittent` |
| Notes | Free text — repro conditions for intermittent/fail, anything else worth keeping |

## Hardware inventory (for the Hardware column)

- **Cardputer** — 1x M5Stack Cardputer-Advance (ESP32-S3)
- **C5** — 1x ESP32-C5 TRIDENT companion node
- **S3-dev-A / S3-dev-B** — 2x bare ESP32-S3 N8R16 boards, available as a
  mock/dev environment if a second node is needed before more Cardputer/C5
  hardware exists (e.g. early multi-node bring-up away from the real
  Cardputer+C5 pair)

## Log

| ID | Date | Trigger | Area | Hardware | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| POS-AUDIT-119 | 2026-11-04 | release (v0.9.1) | not preserved | Cardputer + C5 | pass | Backfilled 2026-11-05 — thin entry, pre-dates this log. Part of the v0.9.1 pre-release hardware pass; original per-area detail wasn't captured in a tracked file at the time. |
| POS-AUDIT-120 | 2026-11-04 | release (v0.9.1) | not preserved | Cardputer + C5 | pass | Backfilled 2026-11-05 — thin entry, pre-dates this log. |
| POS-AUDIT-121 | 2026-11-04 | release (v0.9.1) | not preserved | Cardputer + C5 | pass | Backfilled 2026-11-05 — thin entry, pre-dates this log. |
| POS-AUDIT-125 | 2026-11-05 | release (v0.9.1) | Scan 5G+2G (incl. AP detail) | Cardputer + C5 | pass | Backfilled 2026-11-05 — thin entry. Flagged qualitatively as "a little slower" during this pass; this is the observation that motivated the ESP-NOW v2 payload-raise investigation (see `esp-now-v2.md`). No numeric timing was recorded. |
| POS-AUDIT-126 | 2026-11-05 | release (v0.9.1) | not preserved | Cardputer + C5 | pass | Backfilled 2026-11-05 — thin entry, pre-dates this log. |

<!-- Next entry starts at POS-AUDIT-127. Append below this line, don't renumber the rows above. -->
