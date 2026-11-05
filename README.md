<div align="center">

  <h1>POSEIDON: ADVANCED</h1>
  <p><b>Commander of the Deep</b></p>
  <p><i>Educational RF / embedded security lab for the M5Stack Cardputer-Advance — authorized use on owned hardware only</i></p>

  [![build](https://github.com/wilcox101-maker/poseidon_adv/actions/workflows/build.yml/badge.svg)](https://github.com/wilcox101-maker/poseidon_adv/actions/workflows/build.yml)
  [![target](https://img.shields.io/badge/target-Cardputer--Adv-blue)](https://m5stack.com)
  [![framework](https://img.shields.io/badge/framework-Arduino%2FPlatformIO-green)](https://platformio.org/)
  [![license](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)
  [![modules](https://img.shields.io/badge/research%20modules-170%2B-orange)](FEATURES.md)
  [![release](https://img.shields.io/badge/release-v0.9.0-blueviolet)](https://github.com/wilcox101-maker/poseidon_adv)
  [![version](https://img.shields.io/badge/version-Deepwater-success)](CHANGELOG.md)
  [![use](https://img.shields.io/badge/use-authorized%20%2F%20owned%20hardware-important)](#safety--authorized-use)

</div>

---

## This fork (0.9.0)

[wilcox101-maker/poseidon_adv](https://github.com/wilcox101-maker/poseidon_adv) is my checkout of [Evil0ctopus/poseidon_adv](https://github.com/Evil0ctopus/poseidon_adv). Upstream remains the base. Fork-specific work stays in a short stack of focused commits and is kept when upstream moves. Source revision **0.9.0** (the TRIDENT C5-node release: deauth stop path, scan/zigbee hardening, reproducible libnet80211 patch). Binaries under `release_binaries/` are still the 0.8.0 Deepwater images until the next package build.

I keep a counted inventory of this tree in [FEATURES.md](FEATURES.md).

### What this fork keeps

**Hydra RF Cap.** The official PINGEQUA map for Cardputer ADV, the same wiring Bruce uses for that hat. Poseidon and Bruce are separate firmwares. Poseidon does not read `brucePins.conf`.

| Signal | GPIO | Role |
| --- | --- | --- |
| SPI SCK / MISO / MOSI | 40 / 39 / 14 | Shared with the microSD |
| CC1101 CS / GDO0 | 13 / 5 | CS is also LoRa-GNSS GPS UART TX |
| nRF24 CS / CE | 6 / 4 | CE is high-Z when the radio is released |
| microSD CS | 12 | Held high while a Hydra radio owns the bus |

An earlier combo-hat map (CC1101 CS 15 / GDO0 13, nRF24 CS 4 / CE 3) drove the wrong pads. `cc1101_park_others()` holds microSD CS and nRF24 CS high before the CC1101 takes the bus, and calls `gps_end()` only while the GPS poller is running. GPIO 13 cannot be CC1101 CS and GPS UART TX at the same time. GDO0 (GPIO 5) stays an input and is not a GPS pin. GPS RX is GPIO 15, which Hydra does not use. After the bus is released, `gps_resume_if_enabled()` restarts the UART and the NMEA poller when the user had GPS on. Definitions live in `src/cc1101_hw.h`, `src/nrf24_hw.h`, and `src/gps.h`. The wiring table is also in [docs/proposals/cc1101-nrf24-hat.md](docs/proposals/cc1101-nrf24-hat.md).

**SaltyJack networking notes.** STA, SoftAP, W5500, and C5 operator notes are already in [docs/saltyjack-networking.md](docs/saltyjack-networking.md) (and the HTML twin). The [SaltyJack and networking](#saltyjack-and-networking) section below is the short form of that page.

**Warning cleanup.** The `cardputer-launcher` project sources were cleared of volatile-increment, deprecated NimBLE and Network, format, misleading-indentation, comment, and unused-symbol diagnostics. The edits are mechanical and leave runtime behavior as it was, including the Hydra map. Two diagnostics remain in the pinned SmartRC-CC1101 driver (`s2` unused, `a` maybe uninitialized). That driver is a git pin, so it stays unpatched here. The same warning classes can show up again when this fork takes a newer upstream tree or toolchain. Fold the next cleanup into its own commit.

### How later upstream syncs should land

Keep new fork work as one clear commit in one of these units:

1. Hydra pin, shared-SPI, and GPS fixes
2. Warning cleanup
3. Docs (this note, the SaltyJack how-to, changelog)

When pulling Evil0ctopus/poseidon_adv, replay those units whole so the Hydra fixes, the warning cleanup, and the docs each stay identifiable. History already on `master` stays as published. Rewrite it only when a sync cannot proceed any other way.

The live divergence ledger — last-synced upstream ref, ahead/behind, and per-unit status — is in [docs/fork-status.md](docs/fork-status.md).

---

## Safety / Authorized Use

**POSEIDON is an educational wireless auditing lab for security research on hardware and networks you own or have explicit permission to test.**

- Use only on **your own equipment** or systems where you have **written authorization**.
- Do **not** use this firmware for unauthorized access, interference, jamming, or disruption of third-party networks or devices.
- You are responsible for complying with local radio and computer-misuse laws (e.g. FCC / Ofcom / ETSI and equivalents).
- Captured data (handshakes, probes, logs) may include personal information — handle it lawfully.

If you are unsure whether a use is allowed, treat it as **not allowed**. See the upstream [Legal & Responsible Use](https://github.com/Evil0ctopus/poseidon_adv/blob/master/docs/legal.html) page for fuller guidance.

---

## About POSEIDON (Community Edition)

**POSEIDON** is a pocket-sized, keyboard-first **wireless auditing and security-research lab** built for the **M5Stack Cardputer-Advance** (ESP32-S3, QWERTY keyboard, 240×135 color display). Research modules cover Wi-Fi, Bluetooth Low Energy (BLE), Sub-GHz, 2.4 GHz, LoRa, Infrared, and LAN — intended for learning RF/embedded security concepts and testing **owned** gear.

Upstream POSEIDON is [Evil0ctopus/poseidon_adv](https://github.com/Evil0ctopus/poseidon_adv). I track that tree. The stability work, performance tweaks, and lab / field features in this checkout sit on top of it, and I keep the Hydra, warning-cleanup, and docs commits described above.

- **170+ research modules:** Broad coverage across Wi-Fi, BLE, Sub-GHz, 2.4 GHz, LoRa, Infrared, and LAN for recon and defensive assessment in authorized environments.
- **QWERTY-driven interface:** Letter-mnemonic menus and direct parameter typing (no thumb-stick scrolling).
- **Argus autonomous Gotchi:** A reactive 96×96 mood sprite that can hunt handshakes on owned networks using an SD-persisted reinforcement-learning brain.
- **Deepwater interface:** Eleven themes, crisp domain icons, compact list/card layouts, optional ambient decoration and reduced motion.

---

## What's New & Custom Changes in `poseidon_adv`

- **Deepwater redesign:** Shared bounded text/chrome, a short skippable boot reveal, scrollable help, paged shortcuts and display accessibility settings.
- **Real-time sensor awareness:** Dedicated monitoring helpers for Flock ALPR and ShotSpotter Raven fingerprints (lab / awareness tooling — not for unauthorized targeting).
- **Triton RPG Gotchi leveling:** Persistent XP tracking and interactive leveling tied to Argus.
- **Expanded UI palettes & screensavers:** Eleven themes and thirteen ambient screensavers; animation is optional.
- **On-device data management:** Integrated hex and log viewers on the display.
- **Advanced connectivity:** GATT notification streaming, richer device/OS fingerprinting, and expanded support for hardware hat expansion modules.

## Deepwater interface

- **Boot:** a pixel-aligned trident reveal automatically opens the menu after about 900 ms. Any key skips; security-key boot mode still bypasses the splash.
- **Navigation:** Terminal shows six rows (four with large text) and the selected item's description. Carousel shows one domain card with consistent icons. Letter shortcuts, `;`/`.` navigation, ENTER, Back and `=`/`?` help remain available. Changing layouts returns immediately to the root.
- **Text:** names fit columns with visible `...` overflow. Help scrolls with `;`/`.`; ENTER or Back closes it.
- **Shortcut hints:** `Ctrl+/` advances long footers (`^/ more`). TAB remains available to features that use it.
- **Display settings:** `System → Display` controls motion (`M`), large text (`B`) and ambient decoration (`A`). Large text enlarges menu labels, WiFi/BLE results, help and notices; status and secondary metadata stay compact.
- **Feedback:** notices wrap and can be dismissed early. Destructive device settings and file deletion require confirmation; Back cancels.
- **SaltyJack:** theme-aware 16px icons, a paged grid with every tool reachable, shared footer paging and scrollable help. No second entry splash.
- **Memory:** no full-screen animation buffers for menu transitions or event panels. No PSRAM requirement. Heap diagnostics remain in `System → Heap`.

The default POSEIDON theme now uses navy, ice-white and cyan with restrained lavender. Existing saved theme IDs and explicit ambient preferences are preserved. New ambient preferences default off; reduced motion pauses decoration and automatic animated screensavers, not live measurements.

| Theme | Aesthetic |
| --- | --- |
| POSEIDON | Deepwater navy / ice-white / cyan — default |
| MATRIX | Phosphor green |
| E-INK | Paper white and dark text |
| SYNTHWAVE | Magenta and cyan on midnight grape |
| PHANTOM | Violet and lavender |
| BLOOD | Red on black |
| AMBER CRT | Warm amber terminal |
| NIGHT CITY | Electric yellow and cyan |
| SOLARIS | Solar gold on charcoal |
| NORDIC FROST | Glacial cyan and silver |
| GHOST PURPLE | Electric violet on obsidian |

## SaltyJack and networking

Authorized use only: networks and hardware you own, or explicit written permission. DHCP starvation and rogue DHCP disrupt address assignment. Responder and WPAD collect authentication material.

**Comprehensive how-to** (STA meaning, SoftAP contrast, How-to A/B/C, pins, hat conflicts, C5 offsets, “Not connected” troubleshooting, 0.9.0 vs future W5500): **[docs/saltyjack-networking.md](docs/saltyjack-networking.md)** and **[docs/saltyjack-networking.html](docs/saltyjack-networking.html)**.

The tagged image in this tree is Deepwater **v0.8.0**. There is no separate v0.9.0 binary file. Lab notes that say “0.9.0 fork” mean this tree. W5500 firmware is future work, not this image.

### Wi-Fi STA

Station mode means the Cardputer joins an access point as a **client**. SaltyJack DHCP Starve, Rogue DHCP (STA), Responder, and WPAD use lwIP on that STA netif (`WIFI_STA_DEF`) today. They do not use the ESP32-C5. They do not use the W5500.

Rogue DHCP (AP) is the other path, and it is not STA. The Cardputer brings up an open SoftAP named `POSEIDON-SaltyJack` (lwIP default 192.168.4.1) and serves DHCP itself.

### How-to A — SaltyJack over Wi-Fi STA (works now)

1. Flash this fork (Deepwater image). Standalone: `poseidon-factory.bin` at `0x0`. Do not factory-flash a Launcher layout. See Quick Start below.
2. Hats one at a time, or none. STA SaltyJack does not need a hat. Do not seat Hydra and LoRa together.
3. Join the authorized target Wi-Fi first and wait for `CONNECTED` with a non-zero IP. Root `w` → `n` Connect, or `n` → `c` Connect, or System → Settings → `w` Saved Wi-Fi.
4. Root `j` opens SaltyJack. Tool flow on an authorized LAN: **Starve → Rogue STA → Responder / WPAD** (NTLMv2 Crack is the offline follow-up from the microSD). Backtick stops a tool.
5. **`Not connected`** means the S3 STA is down or has no IPv4 address. It does not mean the Ethernet cable or the C5. The on-screen line “System > Connect” is not a menu name; use the Connect entries above.

### How-to B — W5500 RJ45 (hardware ready, firmware not in this tree)

[docs/hardware.html](docs/hardware.html) says the driver is **Not wired in this tree**. No Ethernet stack is started. Until it is integrated, use How-to A. Do not seat Hydra or LoRa at the same time; they share SPI with the microSD. A **W5500 Mini is 3.3 V only** — do not power a Mini from 5 V.

| Signal | GPIO | Notes |
| --- | --- | --- |
| SCLK | 40 | Shared SPI clock |
| MOSI | 14 | Shared SPI MOSI |
| MISO | 39 | Shared SPI MISO |
| SCS | 1 | W5500 chip-select (SD CS is GPIO 12) |
| INT | 2 | W5500 interrupt, unused by this firmware |
| RST | NC | Leave reset open |
| 5V / GND | 5V and GND | Full-size modules that expect 5 V. Mini: 3.3 V and GND only |

### How-to C — ESP32-C5 TRIDENT (not SaltyJack)

Separate board, ESP-NOW, for 5 GHz, Zigbee, and 5 GHz PMKID. SaltyJack never routes through the C5.

1. Unplug the Cardputer so Web Serial attaches to the C5.
2. Desktop Chrome or Edge: [docs/flash/](docs/flash/index.html), target **TRIDENT — C5**.
3. Offsets, bins under `docs/flash/bin/trident-c5/`: `0x2000` bootloader, `0x8000` partition table, `0x10000` `poseidon_c5_node.bin`.
4. Power both. The C5 HELLO-broadcasts and auto-pairs. On the Cardputer: root `5` → `s` Status.

### Quick map

| Path | What it is | SaltyJack |
| --- | --- | --- |
| STA | Cardputer joins an authorized AP | Starve, rogue STA, Responder, WPAD — works now |
| SoftAP | Open AP `POSEIDON-SaltyJack` | Rogue DHCP (AP) only — works now |
| W5500 | RJ45, pin table above | Not in this tree. Use STA until a driver lands |
| C5 | ESP-NOW satellite | 5 GHz / Zigbee / PMKID only. Never SaltyJack |

## Quick Start & Installation

Use [M5Stack WebBurner](https://burner.m5stack.com/) with the Deepwater factory image for a
standalone installation. The versioned [catalog metadata](docs/m5burner.json)
describes this fork's release. An upstream POSEIDON listing is maintained
separately: changing this repository does not automatically update an
M5Burner account's public catalog entry.
Launcher catalog availability is separate too; the app-only GitHub download
can be installed directly through Launcher without waiting for a catalog update.

### Launcher and custom layouts

Install `poseidon-launcher.bin` through M5Stack Launcher's application installer.
For raw flashing, determine the destination slot from the actual partition
table; do not assume a fixed offset.

The connected development unit runs the **standard single-app bmorcelli
Launcher** — app slot at `0x170000`, built with `cardputer-launcher`. Install
its `firmware.bin` (not `firmware.factory.bin`) via Launcher's *Install from
SD* or WebUI.

The `cardputer-launcher-dual` env and [launcher_dual_app_8Mb.csv](support_files/launcher_dual_app_8Mb.csv)
(POSEIDON at `0x1E0000`, Meshtastic at `0x4B0000`) describe an alternative
dual-app layout — not what this unit currently runs. For a unit on that layout,
update only its POSEIDON slot; never flash its table, a factory image, or use
PlatformIO's full upload target.

See [docs/firmware-targets.md](docs/firmware-targets.md) for the full
build-target → use-case → install-method reference, including the C5 node.

## Building and validation

```text
pio run -e cardputer -e cardputer-launcher -e cardputer-launcher-dual
pio test -e native-test -f test_ui_layout -f test_wifi_logic -f test_ble_db -f test_heap_budget
```

On Windows, with development firmware running on a connected Cardputer:

```powershell
.\scripts\test_deepwater_ui.ps1 -Port COM5
```

For release packaging after building all three profiles:

```powershell
.\scripts\prepare_deepwater_release.ps1 -Version 0.9.0
```

This refreshes the [distribution binaries](release_binaries/README.md), legacy
aliases and SHA256 sums together. Update the personal website's separate
firmware package from those checksum-verified release artifacts.

The smoke script exercises UI navigation, all eleven theme previews, display
settings and **cancel-only** confirmation dialogs, including SaltyJack's list,
paged grid, card and help views without running its tools. It restores the
starting theme/layout/display preferences, checks paint timing against 100 ms
and checks retained heap. Artifacts go under `.pio\ui-smoke`.

Serial `D` reads back the LCD in one-row chunks with a checksum. Unsupported
readback is reported, not counted as successful screenshots. `-SkipCapture`
runs navigation checks alone. Readback pauses input while streaming; use it on
idle menus, not time-critical operations. Paint timing is not end-to-end
keyboard latency or a guarantee for every feature.

Verified on the Cardputer-Adv during the redesign: **33/33 native tests**,
all three firmware profiles and the final hardware sweep passed. Menu paint
maximum was **41 ms**, boot reveal **925 ms**, and the final warmed sweep
retained no additional heap. Application-only flashing passed hash verification;
partition-table readback matched the backup byte-for-byte. This was shared UI
and safe navigation validation, not a transmit/destructive-operation audit.

## Acknowledgments & Shoutouts

- **[@Evil0ctopus](https://github.com/Evil0ctopus)** — Fork of origin
- **[@7h30th3r0n3](https://github.com/7h30th3r0n3)** — Evil-M5Project and RaspyJack (inspiration for the SaltyJack LAN toolkit).
- **[@justcallmekoko](https://github.com/justcallmekoko)** — ESP32Marauder wireless research / promiscuous-mode frameworks.
- **[@bmorcelli](https://github.com/bmorcelli)** — Custom M5Burner firmware browser and webflasher.
- **[@pr3y](https://github.com/pr3y)** — Bruce hardware reference implementations.
- **[@UberGuidoZ](https://github.com/UberGuidoZ)** — Flipper Sub-GHz signal libraries.
- **[@M5Stack](https://github.com/M5Stack)** — Hardware design and SDKs.
