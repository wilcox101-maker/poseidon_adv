<div align="center">

  <h1>POSEIDON: ADVANCED</h1>
  <p><b>Commander of the Deep</b></p>
  <p><i>Educational RF / embedded security lab for the M5Stack Cardputer-Advance — authorized use on owned hardware only</i></p>

  [![target](https://img.shields.io/badge/target-Cardputer--Adv-blue)](https://m5stack.com)
  [![framework](https://img.shields.io/badge/framework-Arduino%2FPlatformIO-green)](https://platformio.org/)
  [![license](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)
  [![modules](https://img.shields.io/badge/research%20modules-170%2B-orange)]()
  [![release](https://img.shields.io/badge/release-v0.8.1-blueviolet)](https://github.com/wilcox101-maker/poseidon_adv)
  [![version](https://img.shields.io/badge/version-Deepwater-success)]()
  [![use](https://img.shields.io/badge/use-authorized%20%2F%20owned%20hardware-important)]()

</div>

---

## This fork (0.8.1)

[wilcox101-maker/poseidon_adv](https://github.com/wilcox101-maker/poseidon_adv) tracks [Evil0ctopus/poseidon_adv](https://github.com/Evil0ctopus/poseidon_adv). Upstream changes are pulled and rebased over time. Fixes for the PINGEQUA Hydra RF Cap on Cardputer ADV stay on top.

This fork exists to correct Hydra bring-up (wrong pins, shared-SPI chip-selects, GPS conflict on GPIO 13). It is not a separate product.

Official Hydra map in this tree: SPI SCK 40, MISO 39, MOSI 14; CC1101 CS 13, GDO0 5; nRF24 CS 6, CE 4; microSD CS 12. Source revision **0.8.1**. Binaries under `release_binaries/` are still the 0.8.0 Deepwater images until the next package build.

This fork’s Hydra GPIO map matches Bruce’s Hydra RF Cap pinout for Cardputer ADV: shared SPI SCK=40, MISO=39, MOSI=14; CC1101 CS=13, GDO0=5; nRF24 CS=6, CE=4. Bruce and Poseidon are separate firmwares. Poseidon does not use `brucePins.conf`. They share the same Hydra wiring map.

---

## Safety / Authorized Use

**POSEIDON is an educational wireless auditing lab for security research on hardware and networks you own or have explicit permission to test.**

- Use only on **your own equipment** or systems where you have **written authorization**.
- Do **not** use this firmware for unauthorized access, interference, jamming, or disruption of third-party networks or devices.
- You are responsible for complying with local radio and computer-misuse laws (e.g. FCC / Ofcom / ETSI and equivalents).
- Captured data (handshakes, probes, logs) may include personal information — handle it lawfully.

If you are unsure whether a use is allowed, treat it as **not allowed**. See the upstream [Legal & Responsible Use](https://generaldussduss.github.io/poseidon/legal.html) page for fuller guidance.

---

## About POSEIDON (Community Edition)

**POSEIDON** is a pocket-sized, keyboard-first **wireless auditing and security-research lab** built for the **M5Stack Cardputer-Advance** (ESP32-S3, QWERTY keyboard, 240×135 color display). Research modules cover Wi-Fi, Bluetooth Low Energy (BLE), Sub-GHz, 2.4 GHz, LoRa, Infrared, and LAN — intended for learning RF/embedded security concepts and testing **owned** gear.

Upstream POSEIDON builds on the GeneralDussDuss release with stability work, performance tweaks, and features aimed at lab / field evaluation on authorized hardware. This wilcox101-maker checkout tracks that tree and adds the Hydra pin and SPI fixes described above.

* **170+ research modules:** Broad coverage across Wi-Fi, BLE, Sub-GHz, 2.4 GHz, LoRa, Infrared, and LAN for recon and defensive assessment in authorized environments.
* **QWERTY-driven interface:** Letter-mnemonic menus and direct parameter typing (no thumb-stick scrolling).
* **Argus autonomous Gotchi:** A reactive 96×96 mood sprite that can hunt handshakes on owned networks using an SD-persisted reinforcement-learning brain.
* **Deepwater interface:** Eleven themes, crisp domain icons, compact list/card layouts, optional ambient decoration and reduced motion.

---

## What's New & Custom Changes in `poseidon_adv`

* **Deepwater redesign:** Shared bounded text/chrome, a short skippable boot reveal, scrollable help, paged shortcuts and display accessibility settings.
* **Real-time sensor awareness:** Dedicated monitoring helpers for Flock ALPR and ShotSpotter Raven fingerprints (lab / awareness tooling — not for unauthorized targeting).
* **Triton RPG Gotchi leveling:** Persistent XP tracking and interactive leveling tied to Argus.
* **Expanded UI palettes & screensavers:** Eleven themes and thirteen ambient screensavers; animation is optional.
* **On-device data management:** Integrated hex and log viewers on the display.
* **Advanced connectivity:** GATT notification streaming, richer device/OS fingerprinting, and expanded support for hardware hat expansion modules.

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
|---|---|
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

## Quick Start & Installation

Use the **Deepwater binaries from this fork**, not upstream's older release.
The [release downloads](https://github.com/Evil0ctopus/poseidon_adv/releases/latest)
and [tracked binaries](release_binaries/) contain the same design:

| Installation | Binary | Use |
|---|---|---|
| Standalone / M5Burner | `poseidon-factory.bin` | Full factory image at `0x0`; replaces the flash layout |
| M5Stack Launcher | `poseidon-launcher.bin` | App-only image; install through Launcher's SD/WebUI flow |
| Existing custom dual-app unit | `poseidon-launcher-dual.bin` | App-only image for the verified layout below; never a factory flash |

**Do not flash the factory image over Launcher or another multi-app installation.**
Back up your device and verify its partition table before a manual update.

### Web Flasher

Open [the personal website's Flash hub](https://evil0ctopus.github.io/flash.html#poseidon_adv)
in Chrome or Edge, connect a Cardputer-Advance and install the standalone image.
The standalone package is hosted in the separate
[website repository](https://github.com/Evil0ctopus/Evil0ctopus.github.io/tree/main/firmware/poseidon_adv).
Do not use a factory installer over an existing Launcher/Meshtastic layout.

### M5Burner

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
table; do not assume the standard profile's `0x170000` offset.

The connected development unit uses [launcher_dual_app_8Mb.csv](support_files/launcher_dual_app_8Mb.csv):
POSEIDON at `0x1E0000`, capacity `0x2D0000`; Meshtastic at `0x4B0000`.
`cardputer-launcher-dual` describes that existing layout. Update only its
POSEIDON application slot. Do not flash its table, a factory image, or use
PlatformIO's full upload target over that installation.

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
.\scripts\prepare_deepwater_release.ps1 -Version 0.8.1
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

## Support My Work

If you find this firmware or related open-source security tools helpful, consider supporting future development:

[![PayPal](https://img.shields.io/badge/PayPal-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://paypal.me/Evil0ctopus)

## Acknowledgments & Shoutouts

* **[@7h30th3r0n3](https://github.com/7h30th3r0n3)** — Evil-M5Project and RaspyJack (inspiration for the SaltyJack LAN toolkit).
* **[@justcallmekoko](https://github.com/justcallmekoko)** — ESP32Marauder wireless research / promiscuous-mode frameworks.
* **[@bmorcelli](https://github.com/bmorcelli)** / **[@pr3y](https://github.com/pr3y)** — Bruce hardware reference implementations.
* **[@UberGuidoZ](https://github.com/UberGuidoZ)** — Flipper Sub-GHz signal libraries.
* **[@M5Stack](https://github.com/M5Stack)** — Hardware design and SDKs.
