# GODZILLA — detection breadth & receive-path robustness (next phase)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19, on Dale's "keep GODZILLA on tasks."
The three-board port and its whole post-port queue are landed and merged (#99–#104, #106); the
only post-port item left open is audio (§prev-5), which stays **bench-blocked** on the ES8311
address. This phase keeps you busy on work that needs **no hardware in hand** and is pure-logic
native-testable end to end.

Standing rules unchanged: **passive only** (no TX beyond the existing ESP-NOW HELLO/publish);
**no `src/*`, C5 or wire-format change**; capability flags, never stubs; one geometry source;
every new decoder is a **pure, header-only part copied into the node tree** and native-tested
(the #98 §2.4a pattern — never edit `src/`); CI green on every env; one topic per PR; merge
commits for any stacked work. Claudius reviews/merges, renders, and owns the main-firmware/C5
side. **Everything here is receive-only classification of frames the node already hears — if a
task ever seems to need the node to transmit or connect, stop and flag it (§4).**

## 1. BLE Lab — decode breadth (passive advert parsing)
The node already captures raw adverts (#102). Add pure parsers that *label* the common families
in what's already captured — no new scanning mode, nothing that connects:
1. **Standard beacon formats:** iBeacon (Apple company-ID 0x004C, type 0x02/0x15 → UUID/major/
   minor/tx-power), Eddystone (service 0xFEAA → UID/URL/TLM), AltBeacon. Decode-only.
2. **Manufacturer / company-ID naming:** map the BLE SIG company ID in manufacturer-specific data
   to a vendor name for the top IDs (Apple, Microsoft, Google, Samsung, Tile, etc.); show
   "vendor — <type>" in the BLE Lab detail. Table lives in the node tree.
3. **Public tracker surfacing (protective, decode-only):** flag adverts that match the *public*
   Find My / AirTag and Tile/SmartTag advertisement signatures as "possible tracker", so an
   unknown tracker nearby is visible. This is anti-stalking surfacing from the advert alone —
   **no pairing, no connect, no identity resolution, no owner lookup** (that's §4).
Each parser takes a captured advert byte vector and is native-tested with real captured bytes
(pass a few through `test/`). Keep the BLE Lab row/detail layout routed through `gz_geom.h` so it
still fits 240 px.

## 2. Wi-Fi detection — passive device-family classification
From the management frames already sniffed (no new sniff mode, no TX):
4. **MAC privacy flag:** surface the locally-administered bit so a randomized MAC reads
   "randomized" vs a global/OUI MAC reads "<vendor>". Pure bit test + OUI lookup.
5. **Family tag from IEs:** walk the vendor-specific / capability IEs in probe-requests and
   beacons to tag a contact as phone / laptop / IoT / AP where the IEs make it unambiguous, and
   leave it "unknown" otherwise (no guessing). Pure IE-walk, native-tested with captured frame
   bytes. Surface the tag on the radar card and in Recon rows.

## 3. Receive-path robustness — fuzz the passive decoders
6. **Fuzz/boundary native tests** over every pure parser the node owns — the new BLE and Wi-Fi
   decoders above, the existing advert store, and the node-side detection **encode** path plus
   the shared pure `godzilla_det.h` merge helpers. Feed truncated, oversized, zero-length and
   garbage buffers; assert no over-read and a clean reject (no crash, counters increment). This
   hardens the node against malformed air traffic. *Scope note:* the POSEIDON-side receive path
   (`c5_dets` in `src/`) is Claudius's lane — I'll fuzz that side; you cover the node tree and
   the shared pure header.

## 4. Reserved for Dale / blocked — don't start without a yes
- **Audio tones (§prev-5):** still blocked on the bench ES8311 address (runbook E7). Resume once
  Dale confirms it from the 2.8".
- **CSI motion monitor** if it needs the node to transmit; **BLE connect / pair / emulate / clone**
  or any tracker **identity/owner resolution**; **wardrive** (no GPS); a **time-sync wire message**;
  **PMIC register writes**; **P4** (themes, watch faces, Badge Studio, locksport).
- Audit **F7** (C5↔POSEIDON enum parity test) is **Claudius's** — PR #107, don't duplicate it.

Suggested order: 1 → 2 → 3 (breadth first, then harden what now parses more). Keep each family a
separate small PR with its captured-bytes test. List any Dale-bench item a PR wants confirmed.
