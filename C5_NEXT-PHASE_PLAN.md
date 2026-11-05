# Next phase: C5 node and Poseidon ESP-NOW

The next step for me(wilcox101-maker) is work on the ESP32-C5 node and the ESP-NOW link back to the Cardputer. The command IDs are already on the wire. I will finish those before I add anything new.

## Where I am

The C5 already answers HELLO and PING, scans 5 GHz and 2.4 GHz, runs 5 GHz deauth, captures PMKID, captures a handshake when the EAPOL fits, and sniffs Zigbee/802.15.4. The Cardputer menu for that is the six C5 rows: Status, Scan 5G+2G, Zigbee sniff, Deauth 5G, PMKID 5G, Nuke 5G + HS.

`POSEI_VERSION` in `c5_node/main/proto.h` and `C5_VERSION` in `src/c5_cmd.h` are both **4**. The v3 cut added nine commands and four responses, and moved the old responses from 20–25 to 40–45. Version 4 then slimmed `posei_hs_t` / `c5_hs_t` to 174 bytes so a handshake fits the 230-byte payload (`eapol_m2` is capped at 128). The banners in `proto.h` and `c5_cmd.h`, and the type table in `c5_node/README.md`, still say v3. The constants are what the firmware checks.

Those nine commands are defined on both sides and unimplemented on the C5. `c5_node/main/main.c` logs them and drops them. The Cardputer already has the senders in `src/c5_cmd.cpp` and the getters (`c5_stas`, `c5_probes`, `c5_deauth_hits`, `c5_spectrum_get`). Nothing in `MENU_C5` calls those senders. The 2.4 GHz tools on the S3 stay the local path. This phase is the remote path, using the structs already in the headers.

Wardrive stays out of the protocol. `proto.h` says so on purpose. The Cardputer wardrive already merges C5 scan results into the WiGLE CSV during a channel-1 hold.

## Done this cycle — deauth actually transmits, and stops

The 5 GHz deauth path now actually goes on air and can be stopped. Two things were in the way.

- **The blob filter.** `esp_wifi_80211_tx()` returned `ESP_OK` while `ieee80211_raw_frame_sanity_check()` inside `libnet80211.a` silently dropped management subtypes 0xC (deauth) / 0xA (disassoc): the status counter climbed and nothing left the radio. The Cardputer-side link override (`src/features/wifi_sanity_override.cpp`) cannot reach it here — on the C5 the caller and the check live in the **same object** inside the archive, so the call is resolved internally before our object is considered, and both `--wrap` and `--allow-multiple-definition` miss. The fix is a direct in-place binary patch of the function body to `li a0,0 ; ret` (return 0 = "not filtered"), now reproducible as `tools/patch_libnet80211.sh`. It parses the `ar` archive and the member's ELF section/symbol tables in pure Python, overwrites the function at its absolute offset length-preservingly, backs up to `.bak`, and verifies — no `ar` extract/re-insert (that round trip was not byte-stable under the Windows riscv toolchain). It is idempotent and must be re-applied after any ESP-IDF reinstall/update. Confirmed in the linked `poseidon_c5_node.elf` on ESP-IDF 5.5.5 / esp32c5.
- **No stop path (POS-AUDIT-119).** Deauth had no stop at all — a targeted or broadcast burst (up to 60 s from the Cardputer's live dashboard) ran to completion regardless of `CMD_STOP`. Now `wifi_attacker_deauth_stop()` is wired into `CMD_STOP`, and the TX loops poll a per-attack `stop` flag every frame. There is one active attack at a time (`s_ctx`); a restart signals the current attack and the **new worker** waits (bounded, off the ESP-NOW receive callback so the Wi-Fi task never blocks) for the outgoing one to fully exit before it touches the radio; a failed `xTaskCreate`/`malloc` unwinds instead of wedging every later call; and the enumerated broadcast AP list is task-local so an overlapped restart can't corrupt it.

Two adjacent hardening fixes landed with it:

- **Scan (POS-AUDIT-121).** A `malloc` failure in `wifi_scanner_run` now sends the zero-payload `RESP_AP` end-of-stream terminator, so the Cardputer shows "done/failed" immediately instead of waiting out its own UI deadline.
- **Zigbee (POS-AUDIT-120).** `esp_ieee802154_enable()` is guarded to the real stopped→running edge to match the refcounted `disable()` in `zb_sniffer_stop()`; a duplicate `CMD_SCAN_ZB` (ESP-NOW redelivery, or the user re-picking the feature) no longer double-enables and underflows the modem-clock refcount.

The six existing C5 rows are unchanged. This cycle was correctness on the paths that already shipped, not new commands — the command build-out below is still the next phase.

## Priority

### 1. Finish the commands already in the headers

Highest value, lowest protocol risk. Same IDs, same structs, `POSEI_VERSION` stays 4. One command at a time. Each one gets a C5 handler, a `STOP` path, and one `MENU_C5` row that calls the sender I already have.

Order inside this band follows radio risk. Passive first, then transmit that stays in station mode, then anything that brings up a SoftAP.

| Order | Command | ID | Why this slot |
| --- | --- | --- | --- |
| 1 | `DEAUTH_DETECT` | 22 | Passive. Response is `RESP_DEAUTH_HIT` (48). Smallest new handler. |
| 2 | `PROBE_SNIFF` | 21 | Passive. `RESP_PROBE` (47). Karma needs these SSIDs later, so this lands first. |
| 3 | `CLIENTS_AP` | 19 | Channel-locked STA list. `RESP_STA` (46). This is the 5 GHz client list. |
| 4 | `CLIENTS_HUNT` | 18 | Same response, hopping. Hello pauses for the hop, same as a scan. |
| 5 | `SPECTRUM` | 25 | Passive RSSI sweep. `RESP_SPECTRUM` (49) already batches 24 channels (UNII-1, 2A, and 3). |
| 6 | `BEACON_SPAM` | 20 | Transmit. The packed request is 170 bytes. `STOP` has to kill it before I add a second TX mode. |
| 7 | `CIW` | 26 | Same transmit scheduler as beacon spam. The request struct is already the format. |
| 8 | `APCLONE` | 24 | SoftAP. The node boots in `WIFI_MODE_STA`. This waits until `STOP` returns the radio to station mode and Hello resumes. |
| 9 | `KARMA` | 23 | SoftAP plus probe sniff. Last, because it depends on both. |

Per command I touch:

- `c5_node/main/main.c` — replace the drop case, and make `CMD_STOP` call the new stop function. Stop already tears down Zigbee, PMKID, handshake capture, and (this cycle) deauth; each new transmit/SoftAP mode adds its own stop in the same `CMD_STOP` case.
- A small `.c` next to `wifi_scanner.c` when the handler does not fit in `main.c`.
- `src/menu.cpp` — one `MENU_C5` row.
- `src/features/c5_scan.cpp` — the screen, same shape as the existing C5 pages.
- `c5_node/README.md` — with the first of these, correct the type table so it matches version 4 and response IDs 40–49. That is a doc fix, not a protocol rev.

`posei_sta_t`, `posei_probe_t`, `posei_deauth_hit_t`, and the request structs stay as they are. A screen that needs more fields waits for band 2.

**Done when:** the C5 log for that ID is no longer "v3 CMD not implemented"; `STOP` releases promiscuous mode or SoftAP, unpauses Hello, and the next scan still returns APs; the new Cardputer row shows the records the existing getter already stores; `POSEI_VERSION` and `C5_VERSION` are still 4. Passive commands (detect, probe sniff, client list, spectrum) are how I confirm the stream. Beacon spam, CIW, AP clone, and karma run only on a network I am allowed to test.

### 2. Later

These need a new opcode, a changed packed struct, or a radio mode the current scheduler does not own. Band 2 starts after band 1's `STOP` path is boring.

- **Dedicated 5 GHz client tracking past the hunt.** `CLIENTS_HUNT` and `CLIENTS_AP` are the tracking already defined. `posei_sta_t` is sta, bssid, channel, rssi. A follow-one-STA view or a GPS-tagged trail adds fields, so it is a struct change and waits.
- **Location-tagged 5 GHz wardriving.** GPS stays on the Cardputer (the LoRa-GNSS hat). Wardrive already calls `c5_cmd_scan_5g` and merges in `merge_c5_5g()` during a channel-1 hold: ESP-NOW results come back on channel 1, and a hopping Cardputer misses the batch. The work is that hold window. There is still no WARDRIVE opcode.
- **Thread / Matter observation.** The Zigbee sniffer already receives 802.15.4 and emits `posei_zb_t`: channel, rssi, frame type, PAN, short addresses, seq. Matter needs a new response. Extending `posei_zb_t` in place would break every v4 peer.
- **BLE recon on the C5.** The Cardputer already does BLE. A C5 BLE command is for a node that is not sitting on the Cardputer's radio. BLE, Wi-Fi, and 802.15.4 do not run together on the C5. New command, version bump, and an in-progress Wi-Fi or Zigbee job wins the radio.
- **Multi-node.** Hello already discovers more than one peer, and the senders broadcast. Two nodes answering the same scan is fine. Assigning one node to 5 GHz and another to 802.15.4 is a new field or a new command, so that part is a bump.
- **Handshake frames that do not fit.** v4 already brought `posei_hs_t` down to 174 bytes with a 128-byte EAPOL cap. Splitting a larger EAPOL across frames is new framing and a version bump. I leave the cap alone until a real capture needs it.

**Done when:** the payload layout is written down and `sizeof` is at most 230 before I flash; `POSEI_VERSION` and `C5_VERSION` go up together in one commit; a peer still on 4 is ignored; the changelog names the old and new wire number in the same sentence.

## Constraints

**230-byte payload.** `POSEI_PAYLOAD_MAX` is 230. `posei_msg_t` is magic, version, type, seq, `payload[230]`, `payload_len`. The length byte has to match what I put in `payload`. The handshake struct is the reminder: extra fields pushed it past the cap, and v4 removed them. Any new or edited struct gets a `_Static_assert(sizeof(...) <= POSEI_PAYLOAD_MAX)` beside the handshake assert. One frame, no second format for overflow.

**Radio scheduling.** One owner at a time. Hello pauses (`g_pause_hello`) while a scan or promiscuous capture hops, because an ESP-NOW transmit locks the channel and starves the sweep. Scans do not stack (`s_scan_running`, duplicate seq dropped). `CMD_STOP` has to stop every mode I add, including the ones above Zigbee, PMKID, and handshake. Wi-Fi station and the 802.15.4 sniffer do not overlap. SoftAP returns to station mode or the node drops out of the peer table. On the Cardputer side, wardrive already shows the matching constraint: park on channel 1 for the result window or the batch is lost.

**Authorized use only.** This stays on hardware and networks I own or have permission to test. Passive commands are what I use to confirm a build. Beacon spam, CIW, AP clone, karma, and deauth stay on those networks and off anything else.

## Versioning

Three numbers. They move for different reasons.

**Wire protocol.** `POSEI_VERSION` and `C5_VERSION`, both 4 today. Either side drops a frame whose version byte does not match (`proto_validate`, `on_recv` in `c5_node/main/main.c`, the receive path in `src/c5_cmd.cpp`). A mixed pair goes silent. I still flash the C5 and the Cardputer together on a bump. The v3 renumber is why: responses left 20–25 for 40–45, and a peer that skipped the version check would misroute.

I add 1 to both constants, in the same commit, when the wire breaks:

- a `POSEI_TYPE_*` / `C5_TYPE_*` id is added or renumbered, or
- the layout of a packed struct a peer already parses changes.

Implementing `CLIENTS_HUNT` through `CIW` stays on version 4. IDs 18–26 and responses 46–49 are already in the headers. An old C5 keeps dropping them. A new C5 still accepts scan, deauth, PMKID, and handshake.

`RELEASE_CHECKLIST.md` says to bump `POSEI_VERSION` when CMD/RESP IDs change, and that most C5 commits do not need it. The handshake shrink is the exception already in the tree: the IDs stayed put, the struct changed, and the constant went 3 → 4. Struct layout counts the same as a new ID. On the next real bump I also rewrite the "wire protocol v3" comments in `proto.h` and `c5_cmd.h` so they match the constant.

**Cardputer version string.** SemVer, separate from the wire integer. The build flag is `-DPOSEIDON_VERSION=\"0.9.0\"` in `platformio.ini`. `src/version.h` falls back to `"0.9.0"` when the flag is missing. Splash, About, and the serial line print `poseidon_version()`. Checklist rule: patch for a fix, minor for a backward-compatible feature, major when the Cardputer-facing contract breaks (saved files, menu keys). The TRIDENT C5-node release took the minor bump, **0.8.1 → 0.9.0**, once for the release, not once per command. Protocol v3 and v4 already shipped inside the 0.x line. A wire flag day stays a `POSEI_VERSION` bump plus a changelog sentence.

**C5 image label.** The C5 tree has no second version macro. `c5_node/CMakeLists.txt` only names the project `poseidon_c5_node`. The published string is `version` in `docs/flash/manifest-trident-c5.json`: `proto-v4-2026-07-17`, pattern `proto-vN-YYYY-MM-DD`. The date changes when I cut a new C5 binary, even if N stays 4. N changes when `POSEI_VERSION` changes. Checklist §1 is the touch list: `platformio.ini` when the Cardputer string changes, `proto.h` when the wire changes, the manifest when a new C5 image is cut, `c5_node/README.md` when the protocol table is wrong.

**Changelog.** `CHANGELOG.md` follows Keep a Changelog and SemVer. Entries accumulate under `## [Unreleased]` until a release, then that block becomes `## [X.Y.Z] - YYYY-MM-DD` and a fresh `[Unreleased]` opens above it. Sections in use: `### Added`, `### Changed`, `### Fixed`. Shape: a bold lead-in, then what changed and why. If the wire moved, the same sentence says flag day and both numbers. The v3 entry is the one I copy:

> **C5 protocol v3 + 5G scan terminator.** Wire protocol bumped v2→v3 (flag day — TRIDENT C5 firmware must be rebuilt to match).

A band-1 command that leaves the wire at 4 reads like: **C5 deauth detect.** The node handles `CMD_DEAUTH_DETECT` and streams `RESP_DEAUTH_HIT`. `POSEI_VERSION` stays 4.
