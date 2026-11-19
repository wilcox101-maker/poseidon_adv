# Fork status

`wilcox101-maker/poseidon_adv` vs upstream `Evil0ctopus/poseidon_adv`.

**Synced to upstream:** `cc0b058` (2026-10-03; upstream unchanged since) · **HEAD:** `+235 / -0` (ahead / behind) · **updated:** 2026-11-19

Ledger only — the narrative lives in the README's "This fork" section, releases in `CHANGELOG.md`. Refresh the counts with `git fetch upstream && git rev-list --left-right --count upstream/master...HEAD`.

## Divergence units

Fork work is kept as self-contained units, replayed whole on each upstream pull.

| Unit | Scope | Status |
|------|-------|--------|
| Hydra pins / shared-SPI / GPS | `src/cc1101_hw.h`, `src/nrf24_hw.h`, `src/gps.h` | kept |
| Warning cleanup | `cardputer-launcher` sources | kept |
| CI | `.github/workflows` (PlatformIO build + native-test, Node 24) | kept |
| C5 node + ESP-NOW | `c5_node/`, `src/c5_cmd.*`, `src/features/c5_scan.cpp` | kept — Tier 2 surface + wire v5 shipped in 0.9.2 (on-device validation still owed); detection-publish types 60/61 added additively for GODZILLA |
| GODZILLA node | `godzilla_node/`, `src/godzilla_det.h`, `src/det_classify.h`, `src/flock_frame.h`, `src/odid_decode.h`, `src/features/c5_godzilla.cpp`, `[env:godzilla*]` ×3, `test/test_godzilla_*` | new — code complete on three boards (1.85B, AMOLED-2.06, ES3C28P); builds in CI; hardware pass owed |
| Docs / upstream-credit retarget | README, SaltyJack how-to, `FEATURES.md`, fork notes, host scrub | kept |

## Not upstreamed — re-verify on every sync

- **Platform + lib pin** (`platformio.ini`): pioarduino `55.03.38` + Bruce's `libnet80211` libs. Required for deauth TX; diverges from stock Espressif.
- **C5 `libnet80211` binary patch** (`tools/patch_libnet80211.sh`): applied to the ESP-IDF toolchain, not in-tree. Re-apply after any IDF reinstall/upgrade.
