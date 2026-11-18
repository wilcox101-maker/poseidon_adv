# Rules for AI collaborators — Poseidon ADV

Dale owns the project. This file sets the collaboration rules and the project's place in the org;
project detail lives in [`README.md`](README.md), [`CHANGELOG.md`](CHANGELOG.md) and
[`docs/fork-status.md`](docs/fork-status.md).

## Place in the org
- **Dale** — owner.
- **Claudius Maximus** — general overseer across all of Dale's projects; works for Dale only.
- A **standalone project** (not under the Redux Manager). Shared support from **Custom Hardware Design**.
- Roles: **PSPE** — Poseidon Senior Programming Engineer (main code base) · **PUIE** — Poseidon User
  Interface Engineer (main UI code).

## Project facts
- A fork of [Evil0ctopus/poseidon_adv](https://github.com/Evil0ctopus/poseidon_adv); fork-exclusive
  work is kept as focused commits that replay across upstream syncs — see
  [`docs/fork-status.md`](docs/fork-status.md).
- Target: M5Stack Cardputer-Advance (ESP32-S3), PlatformIO/Arduino, plus the TRIDENT ESP32-C5 node.

## Hard rules
- **Authorized use only** — hardware and networks Dale owns or has written permission to test.
- **TX-arm:** every offensive transmitter stays behind the master SAFE/ARMED gate; passive scan and
  sniff are never gated (see README).
- **No secrets, ever.** No tokens, keys or credentials in the tree.
- **Dale decides** anything irreversible, and the rule files (`AGENTS.md`, `CLAUDE.md`).
- **Brevity:** result first, at most 5 lines unless the work needs more.
