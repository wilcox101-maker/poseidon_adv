# POSEIDON GODZILLA node

Full-UI detection head for the Waveshare ESP32-S3-Touch-LCD-1.85B. It scans passively on its own
2.4 GHz radio and publishes what it sees to the Cardputer over ESP-NOW v2, as another node on the
POSEIDON fabric. Poseidon-dependent: no standalone release — it rides Poseidon's CI, versioning and
wire protocol (`c5_node/main/proto.h`).

Plan and scope: [`docs/proposals/2026-11-18-godzilla-node.md`](../docs/proposals/2026-11-18-godzilla-node.md).
Wire contract: [`docs/godzilla-detection-publish-contract.md`](../docs/godzilla-detection-publish-contract.md).

## Status

| Piece | State |
|---|---|
| `main/gz_batch.h` — detection batching core | done, native-tested (`test/test_godzilla_batch`) |
| `[env:godzilla]` + `main/main.cpp` skeleton | done, builds in CI (`build.yml` matrix) |
| Passive BLE / Wi-Fi scan loop + ESP-NOW publish | not started |

Nothing here has run on hardware yet.

## Rules

- **Passive only.** The node transmits ESP-NOW publish/HELLO frames and nothing else. Any future
  transmitter stays behind Poseidon's master TX-arm gate ([`docs/tx-arm.md`](../docs/tx-arm.md)).
- The wire header stays v5; the C5 node's on-air format is not touched by anything in this directory.

## Credit

The node's feature set is a native reimplementation of
[Dragon Detector](https://github.com/valleytechsolutions/Dragon-Detector) (MIT) by Your Pal Kal /
Valleytech Custom Solutions. No Dragon Detector source, binaries or artwork are included.
