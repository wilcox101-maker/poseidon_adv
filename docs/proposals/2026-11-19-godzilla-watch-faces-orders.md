# GODZILLA — watch faces & on-device UI customiser (orders)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19, on Dale's go. Dale greenlit watch faces
(previously parked as P4/Dale-reserved) and asked that an **on-device UI customiser** be included.
This is now the **priority phase** — ahead of the remaining detection-breadth units (Eddystone §1.2
is merged; AltBeacon / vendor-ID / tracker-surfacing / Wi-Fi class / fuzz resume after this).

**Visual spec:** the interactive mockup Claudius built —
<https://claude.ai/artifact/CTaHugJu3fsYKkSqtvpbss>. Build to match its layout and behaviour; the
numbers there are demo stand-ins, on hardware each reads the matching node state.

Standing rules unchanged: **passive only**; **no `src/*`, C5 or wire change**; capability flags,
never stubs; one geometry source (`gz_geom.h`); pure logic native-tested; CI green on every env
incl. `pio test (native-test)` and all godzilla envs; **one small PR per unit**, merge commits for
stacked work; Claudius reviews/merges — **GODZILLA never merges**. When a face's display/geometry
lands, say so and Claudius re-renders the three boards.

## Architecture (model after muki01/Smartwatch-ESP32, MIT — approach only, original code)
- Faces live in a registry under `godzilla_node/main/ui/faces/` (own folder, beside the existing
  screens — do not disturb radar/hunt/BLE-lab/status). Each face is a self-contained unit with a
  common interface: `build(parent)`, `update()` (or subject-driven), `destroy()`, plus a
  `caps_required` mask and a `name`.
- Drivers/state publish through **`lv_subject`** (LVGL 9.2.2 supports it); faces subscribe and
  self-update. Reuse the node state the status screen already shows (heap, PSRAM, uptime, BLE/Wi-Fi
  sent + last, channel, last-publish age, IMU rot, battery, recon, last decode) — wrap each as a
  subject once; faces read, never poll.
- Current face + theme + per-slot complication choices persist in **NVS** (namespace `gzui`), read
  at boot, written on change. A face/complication a board can’t source is **hidden**, never faked.

## Units — build in this order, one PR each, each with its native test
1. **Registry + subjects + persistence scaffold** (no visible change): the `gz_face_t` interface,
   the registry list, the `lv_subject` wrappers over existing state, face-switch plumbing, and the
   NVS codec for `{face, accent, background, always_on, slots[4]}`. *Native-test:* registry
   selection, capability filter (a face/complication gated off on a board without the cap), and the
   settings codec round-trips (serialize → deserialize == identity; unknown/old blobs fall back to
   defaults).
2. **Classic face** (extend the existing watch screen): the antique dial — Breguet Arabic numerals,
   dotted minute track with quarter lozenges, blued hands with open-moon tips, and four printed
   sub-scales: **month ring, day-of-week ring, date ring, twin-aperture moon-phase**. All four read
   the RTC → gate behind `GZ_BOARD_HAS_RTC`; on ES3C28P show "set via Cardputer time-sync" (that
   wire message stays Dale’s call). *Native-test:* the pure moon-phase age calc (known dates →
   expected phase bucket) and the sub-scale index math; layout via `gz_geom`. [render]
3. **Command face**: central detection radar (reuse the radar renderer) with analog hands + sweep,
   four **complication pods** bound to subjects, **feature-shortcut tap zones** (quadrant tap →
   open radar / hunt / BLE-lab / status), and a bottom status ticker. The "navigation" pod is
   **IMU tilt** on IMU boards and **channel activity** otherwise — no bearing/DOA, no GPS.
   *Native-test:* pod→field binding table and the round-safe pod placement geometry. [render]
4. **On-device UI customiser**: the settings screen reached by **long-press on the face** (match the
   mockup’s `Customiser` view) — face picker, **accent ×9**, background **black / texture**,
   **always-on** toggle, and **per-slot complication assignment** (list filtered to the board’s
   caps). Lay it out inside a **shape safe-area** (inscribed box for round, inset for rounded-rect)
   so nothing clips; route touches to the registry + NVS writer from unit 1. *Native-test:* the
   safe-area box per board shape, and that the slot picker only offers available complications.
   [render]
5. **Modular + Minimal faces**: muki-style 2×2 slot grid over a digital time, and a big-time
   minimal face with one status line. Reuse the slot renderer from the Command pods. [render]
6. **Polish**: always-on dimmed variants per face, accent applied consistently, and a light
   face-change transition. [render]

## Honest data bindings (complication → real node field)
| Complication | Real source | Boards |
|---|---|---|
| Node status | heap · PSRAM · uptime | all |
| Contact radar | BLE/Wi-Fi contact counts + strongest RSSI | all |
| Publish / mission | BLE sent · Wi-Fi sent · last-publish age · node id | all |
| Channel activity | Wi-Fi hop channel 1–13 | all |
| IMU orientation | QMI8658 tilt / rotation | 1.85B · AMOLED |
| Battery | AXP2101 %+V (AMOLED) · ADC volts (ES3C28P) | AMOLED · ES3C28P |
| Recon | recording on/off → SD file | all (SD) |
| Decode feed | last iBeacon / Eddystone / vendor tag | all |
| Clock / month / day / date / moon | PCF85063 RTC | 1.85B · AMOLED |

**Dropped from the sci-fi inspiration (the node has neither):** GPS coordinates, and a
direction/bearing compass (RSSI only, no direction-finding).

## Reserved for Dale / not in this phase
- Anything that transmits; BLE connect/pair/emulate/clone; the Cardputer **time-sync wire message**
  (so ES3C28P’s clock stays unset — expected); **custom uploaded images** as a background (needs a
  store + decode decision — ask first); Badge Studio / locksport (rest of P4).
