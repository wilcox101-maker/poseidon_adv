# C5 GPIO Expander over ESP-NOW v2 — Design Spec

_Status: proposed · 2026-11-16 · target: C5 (TRIDENT) firmware + Cardputer (S3) transport API. UI is UIE's._

## 1. Goal

Let the Cardputer drive and read the TRIDENT C5's GPIOs remotely over the
existing ESP-NOW link, turning the C5 into a wireless **IO expander / pin
coprocessor**. The Cardputer issues "configure / write / read" pin operations;
the C5 executes them against hardware and replies. The v5 wire protocol
(ESP-NOW v2, 1024-byte payload, already on `master`) lets a **batch** of pin
operations ride a single frame in each direction, so N pins cost one round-trip,
not N.

This is the reason to build it on v2 rather than one-pin-per-frame: the payload
headroom is what makes a batched transaction (read all inputs + set all outputs
in one exchange) practical.

## 2. Non-goals

- **No UI.** The Cardputer-side menu/screen for this is UIE's work. This spec
  defines the wire protocol and a thin S3-side transport API (`c5_gpio_*`) that
  a UI — or any other feature — calls. Nothing here draws pixels.
- **Not a flag day.** This is **additive to v5** — new message types only. An
  older peer that doesn't know `CMD_GPIO` hits the `default` case in `on_recv`
  and drops it, exactly as it would any unknown type. `POSEI_VERSION` /
  `C5_VERSION` stay **5**. (Contrast the 4→5 payload-ceiling raise, which *was*
  a flag day because it changed every struct's wire layout.)
- **Not hard-real-time.** ESP-NOW is best-effort and the C5 time-shares its
  radio with scan/attack/sniff duty cycles. This is for human-scale IO (buttons,
  LEDs, relays, slow sensors), not microsecond-tight bit-banging. See §8.
- **Not tx-arm gated.** Driving a GPIO is not RF transmission. The control frame
  is ordinary ESP-NOW traffic (the same bus the scans already use), so the
  master transmit-arm switch (`tx_arm.*`) does **not** gate it. See §8.

## 3. Protocol — additive message types (v5, no version bump)

Three new IDs, mirrored byte-identical in `c5_node/main/proto.h` and
`src/c5_cmd.h`, each with a `static_assert(sizeof(...) <= POSEI_PAYLOAD_MAX)`,
pinned by `test/test_proto`.

```
/* Commands: S3 → C5 — uses the free slots in the 10–29 range */
POSEI_TYPE_CMD_GPIO        = 27   /* batched pin transaction (config/write/read/adc/pwm) */
POSEI_TYPE_CMD_GPIO_WATCH  = 28   /* subscribe/unsubscribe pin-change notifications (Phase 3) */
/* 29 reserved */

/* Responses: C5 → S3 — extends the range past 49 */
POSEI_TYPE_RESP_GPIO       = 50   /* batched results for a CMD_GPIO */
POSEI_TYPE_RESP_GPIO_EVENT = 51   /* async pin-change notification (Phase 3) */
```

A single multiplexed `CMD_GPIO` (rather than one command per operation) is a
deliberate choice: it conserves the scarce 10–29 command-ID space and puts the
batching in the payload, which is where ESP-NOW v2's headroom actually pays off.

### 3.1 Payload structs

```c
/* One pin operation. 4 bytes. */
typedef struct __attribute__((packed)) {
    uint8_t op;     /* GPIO_OP_* (see below) */
    uint8_t pin;    /* C5 GPIO number (validated against the allow-list, §4) */
    uint8_t a;      /* op-specific arg: config flags | level | PWM duty hi-byte  */
    uint8_t b;      /* op-specific arg: PWM duty lo-byte | PWM freq index         */
} posei_gpio_op_t;

enum {                    /* posei_gpio_op_t.op */
    GPIO_OP_CONFIG   = 0, /* a = mode flags: bit0 OUTPUT, bit1 PULLUP, bit2 PULLDOWN, bit3 OPEN_DRAIN */
    GPIO_OP_WRITE    = 1, /* a = level (0/1) */
    GPIO_OP_READ     = 2, /* digital read; result.value = 0/1 */
    GPIO_OP_ADC_READ = 3, /* oneshot ADC; result.value = raw 0..4095 (Phase 2) */
    GPIO_OP_PWM_SET  = 4, /* LEDC duty = (a<<8)|b (0..1023); freq fixed/table (Phase 2) */
};

#define POSEI_GPIO_MAX_OPS 32   /* req = 2 + 32*4 = 130 B; resp = 2 + 32*4 = 130 B — far under 1024 */

/* CMD_GPIO request: a batch of ops, executed in array order. */
typedef struct __attribute__((packed)) {
    uint8_t count;                        /* 1..POSEI_GPIO_MAX_OPS */
    uint8_t flags;                        /* reserved (0) */
    posei_gpio_op_t ops[POSEI_GPIO_MAX_OPS];
} posei_gpio_req_t;

/* One result. 4 bytes. */
typedef struct __attribute__((packed)) {
    uint8_t  pin;
    uint8_t  status;   /* GPIO_OK=0, GPIO_ERR_PIN=1 (not allow-listed), GPIO_ERR_OP=2, GPIO_ERR_MODE=3 */
    uint16_t value;    /* digital level, ADC raw, or 0 for write/config/pwm acks */
} posei_gpio_res_t;

/* RESP_GPIO: results in the same order as the request's ops. */
typedef struct __attribute__((packed)) {
    uint8_t count;
    uint8_t flags;
    posei_gpio_res_t res[POSEI_GPIO_MAX_OPS];
} posei_gpio_resp_t;
```

`POSEI_GPIO_MAX_OPS = 32` is generous against the C5's ~6–8 physical pins — it
leaves room for repeated ops on one pin in a transaction and for a future I²C
expander's virtual pins (§7, Phase 4). The structs are tiny; the point of
batching here is **one round-trip**, not filling the frame.

### 3.2 Request / response flow

- The S3 sends `CMD_GPIO` with a seq it owns, to the C5 peer's MAC.
- The C5 executes each op in order, inline in `on_recv` (ops are µs-fast — no
  task spawn, same as `CMD_PING`), builds `RESP_GPIO` with one result per op in
  the same order, and `proto_send_to`s it back with the request's seq echoed.
- The S3 matches the reply by seq, with a **50 ms timeout and up to 2 retries**
  (best-effort transport; see §8). Writes and configs are acked by their result
  entry, so a caller can confirm a write landed, not just that it was sent.

## 4. Safe-pin allow-list — the critical safety surface

The C5 has very few pins that are safe to expose. Strapping pins, the UART0
console, the native-USB pair, and the onboard WS2812 must never be driven by a
remote command, or a stray write bricks boot, kills the console, or disconnects
USB. The design makes this a **single compile-time allow-list** in one file; any
op on a pin not in the list returns `GPIO_ERR_PIN` and touches no hardware.

```c
/* gpio_io.c — the ONE authority on which C5 pins are remotely drivable.
 * VERIFY against the ESP32-C5-WROOM-1 datasheet + your board schematic before
 * trusting these numbers; seeded conservatively. Annotate each pin's caps. */
typedef struct { uint8_t pin; uint8_t caps; } c5_safe_pin_t;  /* caps: bit0 ADC, bit1 LEDC */

static const c5_safe_pin_t c5_safe_pins[] = {
    /* { pin, caps } — EXAMPLE seed, NOT yet schematic-verified: */
    { 1, 0 }, { 6, 0 }, { 8, 0 }, { 9, 0 }, { 10, 0 },
    /* add ADC-capable pins with caps|=1, LEDC with caps|=2 once confirmed */
};
```

**Hard excludes (never in the list), per Espressif C5 docs:**

| Pins | Why excluded |
| --- | --- |
| 2, 3, 7, 25, 26, 27, 28 | strapping / boot-mode pins (27 is also the onboard WS2812) |
| 11, 12 | UART0 console (boot log) |
| 13, 14 | native USB D±  |
| 4, 5 | JTAG (keep for debug) |
| flash / PSRAM pins | module-internal |

The seed set above is **a starting point, not schematic truth** — the one job
the implementer owes before hardware test is confirming each listed pin is a
free, broken-out GPIO on the actual WROOM-1 board and tagging its ADC/LEDC
capability. Everything else in the design is independent of the exact numbers.

## 5. C5 firmware touch points

- **New `c5_node/main/gpio_io.{h,c}`** — the allow-list, `c5_gpio_exec(const
  posei_gpio_op_t*, posei_gpio_res_t*)` for one op, and lazy init for the ADC
  oneshot unit (Phase 2) and LEDC channels (Phase 2). Digital config/read/write
  use the `driver/gpio.h` calls directly.
- **`main.c` `on_recv` switch** — one new `case POSEI_TYPE_CMD_GPIO:` that loops
  `req->count` ops through `c5_gpio_exec`, fills `posei_gpio_resp_t`, and
  `proto_send_to`s `RESP_GPIO`. Runs inline (no `xTaskCreate`) — ops are
  non-blocking; this keeps latency low and avoids a task per transaction.
- **No change to the HELLO/capability path for Phase 1.** (Optional later: a
  `has_gpio` capability bit so the Cardputer only offers the feature for nodes
  that advertise it — fits the capability-field work already queued as Phase 2
  item 4 of `esp-now-v2.md`.)

## 6. S3 (Cardputer) touch points — transport only

- **Mirror the structs + IDs** in `src/c5_cmd.h` (byte-identical, static_asserts).
- **New thin API** in `src/c5_cmd.cpp` (or a sibling), for UI/features to call:
  - `bool c5_gpio_config(uint8_t pin, uint8_t mode_flags);`
  - `bool c5_gpio_write(uint8_t pin, uint8_t level);`
  - `bool c5_gpio_read(uint8_t pin, uint8_t *level);`
  - `int  c5_gpio_batch(const posei_gpio_op_t *ops, uint8_t n, posei_gpio_res_t *out);`
  - each wraps send + seq-matched wait (50 ms, 2 retries) and surfaces per-pin status.
- **UI is explicitly out of scope** (UIE). This spec stops at the callable API.

## 7. Phasing

| Phase | Scope | Hardware-testable with |
| --- | --- | --- |
| **1 — core digital** | `CMD_GPIO`/`RESP_GPIO`, `GPIO_OP_CONFIG/WRITE/READ`, allow-list, inline dispatch, S3 API | an LED + a button on two safe C5 pins |
| **2 — analog / PWM** | add `GPIO_OP_ADC_READ` + `GPIO_OP_PWM_SET` (same message type, new op codes — additive) | a pot on an ADC pin, LED brightness via LEDC |
| **3 — event push** | `CMD_GPIO_WATCH` + `RESP_GPIO_EVENT`: C5 ISR on an input pin pushes a change notification, so the Cardputer stops polling | a button that reports edges unprompted |
| **4 — optional expander** | drive an I²C GPIO expander (e.g. PCF8574) off the C5 behind the same op interface as virtual pins — real pin-count expansion past the C5's ~6–8 | an 8-bit expander breakout |

Phase 1 is the whole usable product; 2–4 are additive and each independently
shippable without a version bump.

## 8. Risks / notes

- **Channel contention.** The C5 channel-hops during scans and locks to a
  channel during attacks; while it's off the ESP-NOW channel, a `CMD_GPIO` may
  not be heard until they're co-channel again. This is an existing property of
  *every* C5 command, not new here — but it means GPIO latency is unbounded
  while the C5 is mid-scan. The 50 ms/2-retry policy papers over momentary
  misses, not a 10-second channel lock. Document it; don't promise realtime.
- **Best-effort delivery.** No ACK layer below ESP-NOW. Reads retry; a caller
  that needs a guaranteed write should read-back to confirm (the result entry
  already supports this).
- **Not tx-arm gated, by design.** GPIO control is not offensive RF. If a future
  reviewer wants a *separate* opt-in guard for "remote pin control" (e.g. to
  avoid surprise relay actuation in a demo), that's a deliberate product choice,
  not a safety default — flag for Dale rather than assume.
- **Pin-list correctness is the whole safety story.** The allow-list in §4 is
  seeded, not verified. The one thing that must be right before flashing is that
  every listed pin is a genuinely free, broken-out GPIO on the board. A wrong
  entry that lands on a strapping pin is a boot-brick risk (recoverable via the
  GPIO28 BOOT download mode, but still a bad day).
- **Scope.** This is a new roadmap item, outside the locked v1.0 Band 1 + 6
  Band 2 set (`release-versioning-policy.md`). Like BLE recon / wardriving it can
  land before or after 1.0 at Dale's discretion; it doesn't gate 1.0 and isn't
  scope creep in either direction. App release follows the `0.9.9.9x` convention.

## 9. Acceptance (Phase 1)

- C5 and Cardputer build clean (PlatformIO + `idf.py`), `test/test_proto` green
  with the new structs pinned.
- From the Cardputer: `c5_gpio_config(pin, OUTPUT)` + `c5_gpio_write(pin, 1)`
  lights an LED on a safe C5 pin; `c5_gpio_read(buttonpin)` tracks a button.
- An op on an excluded pin (e.g. 27) returns `GPIO_ERR_PIN` and does **not**
  touch hardware — verified the WS2812/strapping pins are untouchable remotely.
- A batch of config+write+read in one `CMD_GPIO` returns one correctly-ordered
  result row per op.
- POS-AUDIT entry logged for the hardware pass.
