# Phase 1 · Tier 1 — Implementation Handoff (POSEIDON C5 node)

> Handoff for VS Code Claude (Claude Code). Prepared 2026-11-05.
> Repo: `wilcox101-maker/poseidon_adv`. This is the build-ready slice of the
> larger phased plan (see `C5_NEXT-PHASE_PLAN.md` and the Poseidon ADV plan doc).

## 0. TL;DR

Phase 1 Tier 1 = implement the **passive, low-risk** C5 node commands whose S3
side is *already built*. For each one, the ESP-NOW wire protocol, the S3 command
sender, the S3 response parser, and the S3 result collector already exist.

**Your job is the C5 firmware handler plus a small S3 menu entry. Do not touch
the wire protocol.**

Five deliverables, in order:

1. **T1.0** — Thread frame labeling on the existing Zigbee sniffer (warm-up; no new command).
2. **T1.1** — `PROBE_SNIFF`, probe-request logger.
3. **T1.2** — `DEAUTH_DETECT`, passive deauth/disassoc detector.
4. **T1.3** — `CLIENTS_HUNT` / `CLIENTS_AP`, STA discovery.
5. **T1.4** — `SPECTRUM`, per-channel peak-RSSI sweep.

Then **Tier 2 prep** (refactors and stubs only, no features) in section 7.

## 1. Scope and non-goals

- In scope: C5 firmware (`c5_node/`) and minimal S3 menu wiring (`src/`).
- Out of scope — do **not** start: Tier 2 features (beacon spam, AP clone, karma,
  CIW), Tier 3 (BLE offload, 802.15.4 injection, full Thread decode), and the
  multi-board / UART / Pi work. **No protocol changes of any kind.**

## 2. Invariants — do not break these

- **Protocol v4 is frozen (FLAG DAY).** `POSEI_VERSION == 4` in
  `c5_node/main/proto.h` must equal `C5_VERSION == 4` in `src/c5_cmd.h`; both
  sides reject a mismatch. Do not add, renumber, or resize any type or struct.
  Every Tier 1 command type (18–26) and response type (46–49) already exists on
  both ends.
- **The S3 side is already complete** for every Tier 1 response: senders
  (`c5_cmd_*`), parsers (`handle_resp_*`), and collectors (`c5_stas` /
  `c5_probes` / `c5_deauth_hits` / `c5_spectrum_get`) are all in
  `src/c5_cmd.{h,cpp}`. The only S3 work is a menu entry that calls the sender
  and renders the collector output.
- **ISR safety (POS-AUDIT-005/006).** The promiscuous / 802.15.4 RX callback runs
  in ISR / high-priority context and must stay IRAM-safe. `esp_now_send()` is
  **not** IRAM-resident — never call it (nor `ESP_LOGx`, nor `led_fx_set`) from
  the callback. Parse and bounds-check in the ISR, `xQueueSendFromISR()` a small
  POD, and do `esp_now_send` from a normal-priority drain task.
  `c5_node/main/hs_capture.c` is the reference implementation.
- **Channel discipline.** Before any `proto_send_to()`, restore the radio to
  2.4 GHz channel 1 (`esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)`) — POSEIDON
  RXes ESP-NOW on ch 1. Set `g_pause_hello = true` for the duration of a capture
  so HELLOs don't fight the sweep, and clear it on exit.
- **One running job at a time.** Mirror the existing guard: pre-empt a second
  capture the way `hs_capture_start` does — unregister the RX callback *before*
  retiring the old context, then delay, then start the new one.

## 3. Where the code goes

- **C5 dispatch:** `c5_node/main/main.c`, the `on_recv()` switch. The nine
  Tier-1/2 command types currently fall through to
  `ESP_LOGW("... not implemented yet, dropping")`. Replace each Tier 1 case with
  a real `*_start()` call, add `extern` decls at the top (as the existing
  handlers do), and route `CMD_STOP` to your new `*_stop()`.
- **New C5 source files** (add each to `c5_node/main/CMakeLists.txt` SRCS):
  `probe_sniffer.c` (T1.1), `deauth_detect.c` (T1.2), `clients_hunt.c` (T1.3),
  `spectrum.c` (T1.4). T1.0 edits `zb_sniffer.c` in place.
- **Template to clone:** `c5_node/main/hs_capture.c`. It already has the
  context/lifecycle, the ISR→queue→drain split, the channel lock, `g_pause_hello`
  handling, the restart-race guard, and restore-to-ch1. Strip the M1/M2 pairing
  and swap in the per-feature parse and emit.

## 4. Wire contract (exact — the S3 parsers enforce this)

Response structs live in `c5_node/main/proto.h` (C5) and are mirrored in
`src/c5_cmd.h` (S3). The per-frame rules the S3 `handle_resp_*` already assume:

| Response | Struct | Records per ESP-NOW frame | S3-side behavior |
| --- | --- | --- | --- |
| `RESP_PROBE` (47) | `posei_probe_t` | exactly one | dedup by (src, ssid); ssid_len clamped to 32; ring of 16 |
| `RESP_DEAUTH_HIT` (48) | `posei_deauth_hit_t` | exactly one | no dedup; ring of 16 |
| `RESP_STA` (46) | `posei_sta_t` | exactly one | dedup by (sta, bssid); ring of 16 |
| `RESP_SPECTRUM` (49) | `posei_spectrum_t` | one batch (<=24 ch) | latest-snapshot only, not accumulated |
| `RESP_ZB` (42) | `posei_zb_t` | exactly one | ring; T1.0 adds a frame-type label |

Consequences that matter:

- STA / PROBE / DEAUTH stream **one record per frame** (unlike `RESP_AP`, which
  batches multiple per frame). Let the drain task throttle your emits so you
  don't flood ESP-NOW; the S3 dedups anyway.
- SPECTRUM keeps only the **latest** batch on the S3. Send **one batch per full
  sweep**, and that batch must hold every channel you want visible at once
  (<=24). Pick a fixed working set (see T1.4).
- Always set `msg.seq` to the command's seq (echo `m->seq`) and
  `msg.payload_len = sizeof(struct)`.

## 5. Tier 1 work items

### T1.0 — Thread frame labeling (warm-up, edits `zb_sniffer.c`)

- **Goal:** the 802.15.4 radio already captures raw frames (Zigbee *and* Thread —
  both ride 15.4 on channels 11–26). Label each so the S3 can tell them apart.
  No new command, no protocol change.
- **Do:** in the `zb_sniffer.c` parse, set `posei_zb_t.frame_type` to carry two
  things — keep the raw 15.4 Frame Control Field type (bits 0–2: 0=beacon,
  1=data, 2=ack, 3=MAC command) in the low nibble, and a best-effort
  Zigbee-vs-Thread class in the high nibble.
- **Heuristic (verify against real captures, do not trust blindly):** on a 15.4
  *data* frame, look at the first payload byte after the MAC header. 6LoWPAN
  dispatch patterns (IPHC `0x60`–`0x7F`, mesh `0x80`–`0xBF`, frag `0xC0`–`0xEF`,
  `0x41` uncompressed IPv6) → Thread-likely. A Zigbee NWK header (frame-control
  low bits with protocol version nibble) → Zigbee-likely. If ambiguous, mark
  unknown. Treat this as a classifier, not a decoder — any UI label should read
  "Thread?" / "Zigbee?". Full Thread decode is Tier 3.
- **Keep compatible:** the low nibble (raw FCF type) must stay valid so the
  existing Zigbee view is unaffected. Document the nibble encoding in a comment.
- **Accept:** run `CMD_SCAN_ZB`; `RESP_ZB` frames arrive with the label populated
  and the current Zigbee view still works.

### T1.1 — `PROBE_SNIFF` (CMD 21 → `RESP_PROBE` 47)

- **Request:** `posei_probe_sniff_req_t { uint8_t channel; uint16_t duration_ms; }`
  — channel 0 = hop 1–13, else lock.
- **C5:** clone the skeleton (section 6). Filter `WIFI_PROMIS_FILTER_MASK_MGMT`.
  In the callback: management frame is `(p[0] & 0x0C) == 0x00`; probe-request
  subtype is `(p[0] & 0xF0) == 0x40`. src (addr2) = `&p[10]`. SSID element sits
  after the 24-byte MGMT header: element id at `p[24]` (0 = SSID), length at
  `p[25]`, bytes at `&p[26]`; clamp length to 32. Empty SSID (len 0) is a
  wildcard/broadcast probe — still emit it. rssi = `pkt->rx_ctrl.rssi`.
- **Emit:** one `posei_probe_t { src, channel, rssi, ssid_len, ssid[33] }` per
  probe. A short per-(src,ssid) dedup in the drain cuts spam; optional, S3 dedups
  too.
- **Hop:** if channel == 0, cycle 1–13 (~300 ms dwell) in the task loop and track
  the current channel in a `static volatile uint8_t` the callback reads.
- **S3:** menu item → `c5_cmd_probe_sniff(channel, duration_ms)`; live view polls
  `c5_probes()` and lists src + SSID + RSSI (clone the `c5_scan.cpp` list UI).
- **Accept:** trigger from the menu, walk a phone with Wi-Fi on nearby → its probe
  SSIDs appear.

### T1.2 — `DEAUTH_DETECT` (CMD 22 → `RESP_DEAUTH_HIT` 48)

- **Request:** `posei_deauth_detect_req_t { uint8_t channel; uint16_t duration_ms; }`.
- **C5:** MGMT filter. Deauth subtype `(p[0] & 0xF0) == 0xC0`, disassoc `== 0xA0`
  (both have `(p[0] & 0x0C) == 0x00`). dst (addr1) = `&p[4]`, src (addr2) =
  `&p[10]`. Reason code = first byte of the frame body after the 24-byte header:
  `reason = p[24]` (enough for the uint8 field). Emit one `posei_deauth_hit_t
  { src, dst, channel, reason }` per frame, no dedup. Hop or lock per the channel
  field.
- **S3:** `c5_cmd_deauth_detect(...)`; view polls `c5_deauth_hits()`. Pairs
  naturally with your own deauth attack UI (spot others', confirm your own).
- **Accept:** fire a deauth from another tool (or the Cardputer's own 2.4 GHz
  deauth on the locked channel) → hits appear with reason codes.

### T1.3 — `CLIENTS_HUNT` / `CLIENTS_AP` (CMD 18 / 19 → `RESP_STA` 46)

- **Request:** `posei_clients_req_t { uint8_t target_bssid[6]; uint8_t channel;
  uint16_t duration_ms; uint8_t hop_all; }`. target_bssid all-zero = every AP;
  hop_all = 1 → cycle 5 GHz 36–165; = 0 → lock `channel`.
- **C5:** DATA filter (`WIFI_PROMIS_FILTER_MASK_DATA`). Reuse `hs_capture.c`'s
  tods/fromds address logic verbatim to pull (bssid, sta) from the frame. If
  target_bssid is non-zero, require `memcmp(bssid, target, 6) == 0`. Emit one
  `posei_sta_t { sta, bssid, channel, rssi }` per frame. A small local
  (sta,bssid) dedup ring in the drain cuts repeats; S3 also dedups.
- **Shared handler:** `CLIENTS_HUNT` and `CLIENTS_AP` are the same routine with
  different (hop_all, target) defaults — route both from `main.c` to it.
- **Channels:** for hop_all, 5 GHz UNII (36,40,44,48,52,56,60,64,100…144,
  149,153,157,161,165), ~300 ms dwell; for 2.4 GHz clients, lock a 2.4 channel.
- **S3:** `c5_cmd_clients_hunt(duration)` / `c5_cmd_clients_ap(bssid, channel,
  duration)`; view polls `c5_stas()`, lists STA ↔ BSSID.
- **Accept:** with a known AP busy, its STAs show up tied to that BSSID.

### T1.4 — `SPECTRUM` (CMD 25 → `RESP_SPECTRUM` 49)

- **Request:** `posei_spectrum_req_t { uint16_t duration_ms; }` (0 = until STOP).
- **Reality check:** the ESP32 Wi-Fi driver exposes no true energy-detect API
  here. Implement **peak RSSI of received frames per channel** — a frame-activity
  heatmap, not an RF spectrum analyzer. Label it that way in the UI.
- **C5:** promiscuous `MASK_ALL`. Keep `int8_t peak[CHS]`. Sweep the working set,
  dwell ~80–150 ms per channel with promiscuous on, track the max rssi seen per
  channel (reset each sweep). After a full sweep, pack one `posei_spectrum_t
  { ch_count; {ch, peak_rssi}[24] }`, restore ch1, and emit. Repeat until
  duration / STOP.
- **Working set (<=24, because the S3 keeps only the latest batch):** recommend
  2.4 GHz 1–13 plus 5 GHz 36/40/44/48/149/153/157/161/165 = 22 channels. Full
  5 GHz won't fit one batch — pick one representative set and document it.
- **S3:** `c5_cmd_spectrum(duration)`; `c5_spectrum_get(&batch)` returns the
  latest sweep → render one bar per channel.
- **Accept:** a busy channel shows a stronger peak than a quiet one.

## 6. Shared sniffer skeleton and S3 wiring

### 6.1 C5 skeleton (clone of `hs_capture.c`, pairing removed)

This is the shape every Tier 1 sniffer takes. `PROBE_SNIFF` is shown; swap the
response type, the filter mask, and the parse block for the other features. Take
the full lifecycle (start / stop / restart-race guard) from `hs_capture.c` — only
the trimmed hot path is shown here.

```c
static const char *TAG = "probe";
extern volatile bool g_pause_hello;

struct ctx_t { uint8_t requester[6]; uint16_t seq;
               uint8_t channel; uint16_t duration_ms; volatile bool stop; };
static struct ctx_t *s_ctx = NULL;
static volatile uint8_t s_cur_ch = 1;      /* task sets on every hop; cb reads */

struct hit_t { posei_probe_t rec; };       /* size to the response struct */
static QueueHandle_t s_q;
static TaskHandle_t  s_drain;
#define Q_DEPTH 16

static void drain_task(void *_) {
    struct hit_t h;
    while (1) {
        if (xQueueReceive(s_q, &h, portMAX_DELAY) != pdTRUE) continue;
        if (!s_ctx || s_ctx->stop) continue;          /* drop late frames */
        posei_msg_t msg;
        proto_init_msg(&msg, POSEI_TYPE_RESP_PROBE);
        msg.seq = s_ctx->seq;
        memcpy(msg.payload, &h.rec, sizeof(h.rec));
        msg.payload_len = sizeof(h.rec);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  /* RX is on ch1 */
        proto_send_to(s_ctx->requester, &msg);
        led_fx_set(LED_MODE_ZB_RX);
    }
}

static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (!s_ctx || s_ctx->stop) return;
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    const uint8_t *p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 26) return;
    if ((p[0] & 0xF0) != 0x40) return;                 /* probe request */
    struct hit_t h; memset(&h, 0, sizeof(h));
    memcpy(h.rec.src, &p[10], 6);
    h.rec.channel = s_cur_ch;
    h.rec.rssi    = pkt->rx_ctrl.rssi;
    uint8_t slen = (p[24] == 0) ? p[25] : 0;
    if (slen > 32) slen = 32;
    h.rec.ssid_len = slen;
    if (slen) memcpy(h.rec.ssid, &p[26], slen);
    BaseType_t woken = pdFALSE;
    if (s_q) xQueueSendFromISR(s_q, &h, &woken);
    if (woken) portYIELD_FROM_ISR();
}

/* task: lazy-init queue+drain, g_pause_hello=true, set filter+cb+promiscuous,
   loop hopping (set s_cur_ch + esp_wifi_set_channel) or locked until stop/
   deadline, then tear down (promiscuous off, cb NULL, ch1, s_ctx=NULL,
   g_pause_hello=false, free ctx). Copy this verbatim from hs_capture.c's
   hs_task + hs_capture_start + hs_capture_stop. */
```

Parse blocks for the other features (same skeleton, different callback body):

- **DEAUTH_DETECT:** MGMT filter; accept `(p[0]&0xF0)==0xC0 || ==0xA0`;
  `dst=&p[4]`, `src=&p[10]`, `reason=p[24]`.
- **CLIENTS:** `WIFI_PKT_DATA`; lift (bssid, sta) with the tods/fromds block from
  `hs_capture.c`; filter on target_bssid when non-zero.
- **SPECTRUM:** `MASK_ALL`; no per-frame emit — the callback just updates
  `peak[s_cur_ch] = max(peak, rssi)`; the task emits one batch per sweep.

### 6.2 S3 menu wiring

The senders and collectors already exist, so each feature needs only a feature
function plus a menu entry:

- Add a `feat_*` function (model it on `src/features/c5_scan.cpp` and
  `c5_deauth_dashboard`): call the sender once, then loop a simple scrolling list
  UI that polls the collector (`c5_probes()`, `c5_deauth_hits()`, `c5_stas()`,
  `c5_spectrum_get()`), ESC to stop (send `c5_cmd_stop()`), `clear` on exit.
- Register the entry in `src/menu.cpp` near the existing C5 items (the "Scan
  5G+2G" / remote-deauth block), gated on `c5_any_online()`.
- Keep the UI minimal — a list of records is enough. SPECTRUM can reuse a simple
  horizontal-bar draw per channel.

## 7. Tier 2 prep (refactors and stubs only — do NOT build Tier 2 features)

Land these *after* T1.1–T1.4 work, as mechanical changes that make Tier 2 cheaper
without adding behavior:

1. **Extract a shared job scaffold.** Factor the common context / queue / drain /
   lifecycle out of the four Tier 1 sniffers into a tiny `c5_job.{c,h}` (or a
   header of static-inline helpers): `job_begin(filter, channel, hop, on_frame)`,
   `job_end()`, and the drain plumbing. Tier 2's KARMA and APCLONE reuse the
   lifecycle; BEACON_SPAM reuses the task/stop pattern. No behavior change to
   Tier 1 — this is a pure refactor, verified by re-running the Tier 1 tests.
2. **Document the raw-TX entry point.** `c5_node/main/wifi_attacker.c` already
   injects deauth frames on 5 GHz. Add a comment block (or a thin
   `wifi_tx_raw(const uint8_t *frame, size_t len, uint8_t channel)` wrapper if one
   doesn't exist) describing exactly how a crafted frame is put on air.
   BEACON_SPAM / CIW will build on this. **Do not implement beacon spam.**
3. **Safety auto-stop helper.** Add a reusable max-run watchdog
   (e.g. `job_deadline(ms)`) and wire it into the Tier 1 durations now. KARMA and
   APCLONE (Tier 2) require the ~5-minute safety cap their request structs already
   imply; proving the helper in Tier 1 de-risks that.
4. **Capability-routing marker (S3, stub only).** In `src/c5_cmd.cpp`,
   `s_peers[].has_5g` / `has_ieee802154` are stored per peer but not yet consulted
   before sending band-specific commands. Leave a
   `// TODO(phase1-infra): gate 5 GHz / 15.4 cmds on peer capability` marker where
   a command is dispatched. **Do not** refactor routing or touch the
   `if (h->role != 1)` gate — that role→capability work is separate Phase 1
   infrastructure, not Tier 2 prep.

## 8. Build, flash, test

- **C5 node (ESP-IDF).** PowerShell, with the C5 connected and the Cardputer
  unplugged so USB serial binds to the C5:

  ```powershell
  . C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
  idf.py -C c5_node -p COM<x> flash monitor   # exit monitor with Ctrl+]
  ```

- **Cardputer (PlatformIO).**

  ```powershell
  pio run -e cardputer -t upload --upload-port COM<y>
  pio device monitor -b 115200                # exit with Ctrl+C
  ```

  Use the app-only `upload` target (not a factory flash) if the unit is on a
  Launcher / dual-app layout.

- **Find COM ports:** `pio device list` from either shell.
- Per-feature acceptance tests are listed under each item in section 5.

## 9. Definition of done (Tier 1)

- All five items implemented; `main.c` no longer logs "not implemented" for
  `PROBE_SNIFF`, `DEAUTH_DETECT`, `CLIENTS_HUNT`, `CLIENTS_AP`, or `SPECTRUM`.
- Each has a working S3 menu entry rendering live results.
- No protocol, struct, or version change; `POSEI_VERSION == C5_VERSION == 4`
  untouched.
- ISR-safety discipline preserved — no `esp_now_send` / log / LED calls from any
  RX callback.
- Thread label present on `RESP_ZB` without breaking the Zigbee view.
- Section 7 prep items 1–3 landed as mechanical refactors/stubs; item 4 is a
  one-line marker only.

Suggested workflow: one branch/PR per item (`feat/c5-probe-sniff`,
`feat/c5-deauth-detect`, `feat/c5-clients-hunt`, `feat/c5-spectrum`,
`feat/c5-thread-label`), each small and independently flashable. Commit style and
the existing `POS-AUDIT-00x` tagging convention per the repo's history.
