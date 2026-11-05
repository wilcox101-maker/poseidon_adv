# Features

Running inventory of what this fork ships. I recount from the files named here when a menu, pin map, or payload table changes. Numbers are from those sources. Where an on-screen blurb disagrees with the table, I say so.

Source revision is **0.9.0** (`POSEIDON_VERSION` in `platformio.ini` and `src/version.h`). The README badge and the `version` field in `docs/m5burner.json` match that. The files under `release_binaries/`, and the assets on the [v0.9.0 GitHub release](https://github.com/wilcox101-maker/poseidon_adv/releases/tag/v0.9.0) that `docs/m5burner.json` points at, are now **0.9.0** Deepwater images built from this source. The image you flash is the 0.9.0 binary. The W5500 driver is still absent — that remains future work.

Authorized use only: hardware and networks I own, or where I have written permission.

## Menu

`src/menu.cpp` holds **170 rows**: **153** launch an action, **17** open a submenu. **148** action functions are unique. Five are linked from two rows:

| Function | Rows |
| --- | --- |
| `feat_wifi_connect` | WiFi → Connect, Network → Connect |
| `feat_ble_tracker` | Bluetooth → Tracker, Hunt → Tracker scan |
| `feat_ble_finder` | Bluetooth → Finder, Hunt → BLE tracker |
| `feat_subghz_finder` | Sub-GHz → Finder, Hunt → Sub-GHz finder |
| `feat_nrf24_finder` | nRF24 → Finder, Hunt → 2.4 GHz finder |

The README badge says **170+**. That matches this tree: 170 rows in `src/menu.cpp`, plus SaltyJack’s own 7-row menu (the root `j` entry is already one of the 170).

Root (`MENU_ROOT_CHILDREN`, 17):

| Key | Name | What it opens |
| --- | --- | --- |
| `w` | WiFi | 21 actions |
| `b` | Bluetooth | 18 actions |
| `i` | IR | 6 rows (5 actions + Pranks) |
| `t` | Argus | `feat_triton` |
| `f` | Feather | 8 actions (nRF52840) |
| `u` | BadUSB | 8 actions |
| `n` | Network | 16 rows (15 actions + Attacks) |
| `j` | SaltyJack | its own menu, 7 rows |
| `r` | Radio | 5 rows (LoRa, Sub-GHz, nRF24, GPS fix, SATCOM) |
| `o` | Tools | 9 rows (8 actions + Hunt) |
| `m` | Mesh | 1 action (ESP-NOW status) |
| `5` | C5 nodes | 6 actions |
| `x` | MIMIR | USB-C drop-box control |
| `p` | PC Bridge | TRIDENT screen mirror (`feat_trident`) |
| `s` | System | 12 actions |
| `d` | Mass Storage | microSD as a USB drive |
| `k` | KERBEROS | USB security key |

### WiFi (21)

Scan, Clients, AP Clients, Deauth, Deauth all, Deauth det., AP Clone, Portal, Evil Twin, AP Signal Test, Karma, Beacon spam, Probe sniff, PMKID cap, Spectrum, Wardrive, Connect, DefMon, Cable Guard, Surveil (Flock / Raven), CIW Zeroclick.

Portal templates in `s_templates[]` (`src/features/wifi_portal.cpp`): **19** — Google, Facebook, Microsoft, Free WiFi, Apple ID, Office 365, LinkedIn, Amazon, Netflix, Instagram, Hotel WiFi, Starbucks, Airport WiFi, Router Admin, Router Firmware, Starlink, Tesla WiFi, Zoom, Company SSO. The WiFi menu blurb still says 16.

CIW categories in `wifi_ciw.cpp`: **14**. The `PAYLOADS[]` table has **108** entries (CMD 25, FMT 13, SERIAL 9, OVFL 8, ESC 8, CHAIN 8, XSS 8, JNDI 6, NoSQL 6, PROBE 5, PATH 4, ENC 3, CRLF 3, HEAP 2). The file header still says ~157 and the menu blurb still says ~120.

### Bluetooth (18)

Scan, Spam, Bad-KB, Tracker, Finder, Drone RID, Sniffer, iBeacon, Clone, GATT, Flood, Karma, SourApple, Find My, Salty Deep, WhisperPair, BlueDucky, HID Sniff.

### IR (6 + 10 pranks)

TV-B-Gone, Remote, Clone, Learn, Pranks, LED Test.

Pranks: Power Bomb, Ch Scramble, Volume Bomb, Source Roulette, Mute Torture, Permanent Off, Cable Flakes, Caption Chaos, Sleep 180, AC Chaos.

`IR_EXTRA_POWER_TABLE` in `src/features/ir_extras_data.h` has **27** brand power codes (Sharp through Bose-SB).

### Feather / nRF52840 (8)

Scan, Long Range, Sniffer, Zigbee, MITM, MITM Relay, Scout+Strike, WiFi+BLE Combo. UART hat, mutually exclusive with LoRa and Hydra.

### BadUSB (8 rows, 166 scripts)

Live Keyboard, Built-in, Pranks, Windows, macOS, Linux, Android, ChromeOS.

Script counts from the arrays:

| Set | File | Count |
| --- | --- | --- |
| Built-in | `s_payloads[]` in `badusb.cpp` | 5 (Hello, Notepad, Rickroll, Lock, Terminal) |
| Windows | `BADUSB_WIN_PAYLOADS` | 47 |
| macOS | `BADUSB_MAC_PAYLOADS` | 24 |
| Linux | `BADUSB_LINUX_PAYLOADS` | 19 |
| Android | `BADUSB_ANDROID_PAYLOADS` | 11 |
| ChromeOS | `BADUSB_CHROMEOS_PAYLOADS` | 8 |
| Pranks | 7 categories in `badusb_pranks_data.h` | 52 (8, 8, 7, 7, 8, 7, 7) |

Per-OS total is **109**. With built-in and pranks, **166**. The BadUSB blurb still says 165+, the Windows blurb still says 44, and the prank blurb still says 59. Prank categories: GASLIGHT, VISIBLE CHAOS, LOCKERS, BROWSER DOOM, FAKE HACK, LOUD, CLASSIC.

### Network (16 + 6 attacks)

Port scan, Ping Sweep, Ping, DNS, Connect, Responder, LAN Recon, UPnP scan, CCTV Toolkit, Attacks, DHCP Starve, Rogue DHCP STA, Rogue DHCP AP, Net Hijack, WPAD Abuse, Autodiscover.

Attacks submenu: UART Shell, TCP Tunnel, Honeypot, Dead Drop, Printer, SSDP Poison.

These rows are separate from SaltyJack. They use the joined Wi-Fi STA (`WIFI_STA_DEF`) the same way. There is no Ethernet netif behind them.

### Radio

LoRa (8): Scan, Beacon, Mesh LF, Analyzer, Mesh Chat, Mesh Nodes, Mesh Page, Mesh Pos. SX1262 on the CAP-LoRa1262 hat. Bands called out in the menu: 433 / 868 / 915 and Meshtastic LongFast.

Sub-GHz (9), CC1101 on the Hydra cap: Broadcast, Scan/Copy, Record RAW, Play .sub, Analyzer, Brute force, Jammer, Finder, Jam Detect.

Brute-force protocols in `subghz_bruteforce.cpp`: **6** — Came 12-bit, Nice 12-bit, Chamberlain 9-bit, Linear 10-bit, Holtek 12-bit, Ansonic 12-bit.

nRF24 (6): Sniffer, MouseJack, BLE Spam, Scanner, Jammer, Finder.

Also on the Radio menu: GPS fix, SATCOM (TLE + SGP4).

### Tools and Hunt

Tools: Hunt, Flashlight, Stopwatch, Dice/8ball, Morse (send and listen), MAC rand, Calc, Screen test, SD format.

Hunt: BLE tracker, Tracker scan, Sub-GHz finder, 2.4 GHz finder. Each one calls a finder that also has its own row under Bluetooth, Sub-GHz, or nRF24.

### C5 nodes (6)

Status, Scan 5G+2G, Zigbee sniff, Deauth 5G, PMKID 5G, Nuke 5G + HS.

Wire protocol on both sides is version **4** (`C5_VERSION` in `src/c5_cmd.h`, `POSEI_VERSION` in `c5_node/main/proto.h`), magic `POSE` (`0x504F5345`). `c5_node/README.md` still says v3. `c5_cmd.cpp` also has senders for 2.4 GHz scan, client hunt, beacon spam, probe sniff, deauth detect, karma, AP clone, spectrum, and CIW. Nothing in `MENU_C5` calls those senders.

### System (12)

Files, Clock, Settings, Heap, Theme, Display, Ambient, Layout, Screensaver, Saver Pick, Sound, About.

Layout is two styles: Terminal and Carousel. Display toggles reduced motion, large text, and ambient decoration.

### Argus

One root entry. Rank titles in `triton_rank_title()`: **10** — Novice, Scout, Stalker, Hunter, Predator, Phantom, Specter, Wraith, Kraken, Leviathan.

### KERBEROS

One root entry. `feat_kerberos` registers a U2F handler and a CTAP2 CBOR handler. CTAP2 commands in `lib/kerberos_core/ctap2.cpp`: getInfo, makeCredential, getAssertion, clientPIN, reset. Resident credentials and the PIN hash go to NVS. The menu blurb and the top of `kerberos.cpp` still describe Phase 1 U2F only.

### MIMIR and PC Bridge

MIMIR is the USB-C drop-box control row. PC Bridge is TRIDENT: framebuffer mirror and remote keyboard over USB-C. That PC Bridge row is separate from the ESP32-C5 TRIDENT node under root `5`.

## Radio paths

Four paths. SaltyJack uses the first two.

| Path | State in this tree | What uses it |
| --- | --- | --- |
| Wi-Fi STA | Works. lwIP on `WIFI_STA_DEF` after a join. | SaltyJack DHCP Starve, Rogue DHCP (STA), Responder, WPAD. Network-menu tools that need an address. |
| SoftAP | Works. Open AP `POSEIDON-SaltyJack` from `wifi_raw_ap_up` in `saltyjack_dhcp_rogue.cpp`. | SaltyJack Rogue DHCP (AP). Other SoftAP tools (Portal, Evil Twin, Karma, AP Clone) bring up their own AP. |
| W5500 RJ45 | Pinout and docs only. No `Ethernet.begin`, no W5500 driver, no Ethernet netif in `src/`. | Nothing. `saltyjack.h` still describes it as future. |
| ESP32-C5 / TRIDENT | Separate board, ESP-NOW. | Root `5`. 5 GHz scan, 5 GHz deauth, 5 GHz PMKID, 5 GHz handshake nuke, 802.15.4 / Zigbee. SaltyJack does not route through it. |

W5500 pinout, from `docs/saltyjack-networking.md` and `docs/hardware.html` (the firmware does not claim these pins): SCLK 40, MOSI 14, MISO 39, SCS 1, INT 2, RST open. Shared SPI with the microSD. A W5500 Mini is 3.3 V only.

## Hydra RF Cap

Hardware-confirmed PINGEQUA map. Definitions: `src/cc1101_hw.h`, `src/nrf24_hw.h`, `src/sd_helper.h`, `src/gps.h`. Poseidon does not read `brucePins.conf`.

| Signal | GPIO |
| --- | --- |
| SPI SCK / MISO / MOSI | 40 / 39 / 14 |
| CC1101 CS / GDO0 | 13 / 5 |
| nRF24 CS / CE | 6 / 4 |
| microSD CS | 12 |
| GPS UART RX / TX | 15 / 13 |

GPIO 13 is CC1101 CS and the LoRa-GNSS GPS UART TX. Those two cannot be live together. GDO0 (GPIO 5) is an input and is not a GPS pin. GPS RX (GPIO 15) is unused by Hydra.

`cc1101_park_others()` and `nrf24_park_others()` hold the other chip-selects high before the radio takes the bus, and call `gps_end()` only while the GPS poller is up. After the bus is released, `gps_resume_if_enabled()` restarts the UART and the NMEA poller when GPS is enabled.

Seat one hat at a time. Hydra, the CAP-LoRa1262, and a W5500 module share SCK / MISO / MOSI with the microSD.

## SaltyJack

Own renderer in `src/features/saltyjack/saltyjack_menu.cpp`. Root `j`. **7** rows:

| Row | Path |
| --- | --- |
| About | credits |
| DHCP Starve | STA |
| Rogue DHCP (STA) | STA |
| Rogue DHCP (AP) | SoftAP `POSEIDON-SaltyJack` |
| Responder | STA (LLMNR, NBT-NS, SMB) |
| WPAD NTLM | STA HTTP |
| NTLMv2 Crack | offline HMAC-MD5 against a wordlist on the microSD |

Operator notes: [docs/saltyjack-networking.md](docs/saltyjack-networking.md).

## Themes and screensavers

**11** themes in `THEMES[]` (`src/theme.cpp`), ids in `src/theme.h`: POSEIDON, MATRIX, E-INK, SYNTHWAVE, PHANTOM, BLOOD, AMBER CRT, NIGHT CITY, SOLARIS, NORDIC FROST, GHOST PURPLE. POSEIDON (Deepwater navy / ice / cyan) is id 0.

**13** screensavers in `s_pool[]` (`src/screensaver.cpp`): WARDRIVE, MATRIX RAIN, BREATHING, DEEP SCAN, PORT SCAN, HEX CASCADE, TERMINAL CRACK, NEURAL ARC, GLITCH BSOD, TIDE WAVES, SAT SKYPLOT, RF WATERFALL, STARFIELD WARP. The System help string still names 10. The picker reads `screensaver_pool_count()`, so the list on device follows the pool.

## Build environments

From `platformio.ini`:

| Env | Role |
| --- | --- |
| `cardputer` | Standalone. `default_8MB.csv`. Factory image at `0x0`. |
| `cardputer-launcher` | App-only for bmorcelli Launcher. `support_files/launcher_8Mb.csv`. App slot `0x170000`, size `0x4F0000`. |
| `cardputer-launcher-dual` | App-only for the dual-app unit. `support_files/launcher_dual_app_8Mb.csv`. POSEIDON `0x1E0000` size `0x2D0000`, Meshtastic `0x4B0000`. |
| `cardputer-ota` | Same image as `cardputer`, upload via espota. |
| `native-test` | Host Unity tests. No board. |

I do not factory-flash a Launcher or Meshtastic layout.

## Maintenance

Clearing `cardputer-launcher` compiler warnings is maintenance. I keep that work as its own commit when I track upstream, and I do not count it as a feature. Two diagnostics remain in the pinned SmartRC-CC1101 driver.

## How I keep the fork readable

Upstream stays [Evil0ctopus/poseidon_adv](https://github.com/Evil0ctopus/poseidon_adv). Fork work lands as one clear commit in one of three units: Hydra pin, shared-SPI, and GPS fixes; warning cleanup; docs. Published `master` history stays as it is. The same note is in the README under “This fork”.
