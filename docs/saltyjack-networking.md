# SaltyJack and networking

Authorized use only. Run these tools on networks and hardware you own, or where you have explicit written permission. DHCP starvation and rogue DHCP disrupt address assignment. Responder and WPAD collect authentication material. Unauthorized use is illegal in most places. If you are unsure, do not run the tool. See [Legal](legal.html) and the README safety section.

This page is the operator guide for **this** `master` tree of [wilcox101-maker/poseidon_adv](https://github.com/wilcox101-maker/poseidon_adv). The version string in the README, `CHANGELOG.md`, and `docs/m5burner.json` is **Deepwater v0.8.0**. There is no separate v0.8.1 firmware image in this repository. Lab notes that say “0.8.1 fork” mean this Deepwater tree: SaltyJack over Wi-Fi station mode, and the SoftAP rogue-DHCP path, are in the image you flash today. The W5500 driver is not.

The same guide is on the docs site: [saltyjack-networking.html](saltyjack-networking.html).

## Four paths, and which one SaltyJack actually uses

The Cardputer-Adv radio is an ESP32-S3. It has 2.4 GHz Wi-Fi. It does not speak 5 GHz. SaltyJack does not grow a second radio by plugging in a hat.

| Path | Who is the access point? | What the Cardputer does | SaltyJack in this tree |
|---|---|---|---|
| **Wi-Fi STA** | An authorized AP you already run | Joins that AP as a client. lwIP attaches to the STA netif `WIFI_STA_DEF`. | **Works.** DHCP Starve, Rogue DHCP (STA), Responder, and WPAD bind here. |
| **SoftAP** | The Cardputer | Brings up an open AP named `POSEIDON-SaltyJack` and serves DHCP itself. | **Works**, but only the Rogue DHCP (AP) tool. This is not the STA path. |
| **W5500 RJ45** | Whatever is on the Ethernet segment | Would be an SPI Ethernet netif, if a driver existed. | **Not implemented.** Hardware pinout only. [hardware.html](hardware.html) — “Not wired in this tree.” |
| **ESP32-C5 TRIDENT** | Unrelated. The C5 is its own board. | ESP-NOW satellite for 5 GHz, Zigbee, and 5 GHz PMKID. | **Never.** SaltyJack does not send or receive through the C5. |

“Station mode” (STA) means the Cardputer is a Wi-Fi **client**. It associates to an access point, receives a DHCP lease from that network’s real server, and then uses the address lwIP installed on the STA interface. Starve, rogue-STA, Responder, and WPAD all call `WiFi.status()` / `WiFi.localIP()` and, for starve, stop the DHCP **client** on `WIFI_STA_DEF` for the duration of the run. None of that code opens an Ethernet interface. None of it calls the C5 command channel (`c5_cmd_*` over ESP-NOW).

Rogue DHCP (AP) is the contrast. It does not join the target AP. `wifi_raw_ap_up("POSEIDON-SaltyJack")` starts an open SoftAP (channel 1, not hidden, up to 4 stations) via the raw-IDF helper, because Arduino `WiFi.softAP()` crashes on the Bruce-pinned libraries this tree builds with. The AP address is lwIP’s default **192.168.4.1**. The tool then stops the IDF DHCP **server** on `WIFI_AP_DEF` and binds UDP/67 itself. Clients that join `POSEIDON-SaltyJack` are leased by the Cardputer, with the Cardputer as gateway and DNS.

## 0.8.1 vs future W5500

Lab notes call this fork “0.8.1”. The version string actually tagged in the README, `CHANGELOG.md`, and `docs/m5burner.json` is **Deepwater v0.8.0**. There is no v0.8.1 binary to flash. The table is what that image does today, and what is still future — the W5500 driver in particular.

| Capability | This tree (Deepwater v0.8.0) | Later, not in this tree |
|---|---|---|
| Join an authorized AP (`WiFi` → Connect, `Network` → Connect, or System → Settings → Saved Wi-Fi) | Yes | — |
| SaltyJack DHCP Starve, Rogue DHCP (STA), Responder, WPAD on that STA netif | Yes | — |
| SaltyJack Rogue DHCP (AP), SSID `POSEIDON-SaltyJack` | Yes | — |
| On-device NTLMv2 wordlist (`NTLMv2 Crack`) from files on the microSD | Yes. Does not need a live link. | — |
| ESP32-C5 over ESP-NOW: dual-band scan, Zigbee sniff, 5 GHz deauth, 5 GHz PMKID | Yes, under **C5 nodes**. Separate from SaltyJack. | — |
| W5500 / RJ45 as the lwIP interface for SaltyJack or the Network menu | **No.** No `Ethernet` begin, no W5500 driver, no second netif. | Comments in `src/features/saltyjack/saltyjack.h` and `saltyjack_responder.cpp` describe this as future (“once the SPI Ethernet hat is wired up”, “when that hat lands”). |
| SaltyJack packets forwarded by the C5 | **No.** | Not a planned path. The C5 is a different radio. |

Older copy on the roadmap, codex, and arsenal pages described a wired RaspyJack path as if it already ran. That described an intended design. It is not what this tree does. Trust this page and the source comments above.

## Prerequisites

- A Cardputer-Adv, and this fork’s Deepwater image. Standalone units take `poseidon-factory.bin` at `0x0`. Launcher units take the app-only image in the existing POSEIDON slot. Do not factory-flash a Launcher or Meshtastic layout. Details are in the [README Quick Start](../README.md) and [Install](install.html).
- Written authorization for the network you will join or stand up.
- For How-to A: the SSID and passphrase of that network. The S3 must be able to associate and get a non-zero IPv4 address. WPA3-only, 802.1X, and PMF networks that refuse this client will not produce a STA lease, and SaltyJack will stop at “Not connected.”
- A microSD card if you want hash files. Responder and WPAD also print a capture line on the serial console when the SD file cannot be opened.
- Hats: seat **one hat at a time**, or none. STA SaltyJack uses the S3’s own Wi-Fi and does not need LoRa, Hydra, or a W5500. Hydra and the CAP-LoRa1262 share SPI with the microSD (SCK 40, MISO 39, MOSI 14). Do not seat Hydra and LoRa together, and do not seat either of them in the same session you are trying to use a W5500.
- The ESP32-C5 is not a hat on that header. Leave it unplugged from USB while you flash it (How-to C), so the browser attaches to the C5 and not the Cardputer.

## How-to A — SaltyJack over Wi-Fi STA

This is the path that works now.

### 1. Flash this fork

Use the Deepwater binaries from **this** repository, not an upstream POSEIDON build and not a guessed “0.8.1” filename. The tagged image in-tree is v0.8.0.

- Standalone / M5Burner: `poseidon-factory.bin` at offset `0x0`.
- M5Stack Launcher: `poseidon-launcher.bin` through Launcher’s installer.
- The known dual-app development layout: `poseidon-launcher-dual.bin` into the POSEIDON slot only.

Browser flashing for the Cardputer is the S3 target on [docs/flash/](flash/index.html) (desktop Chrome or Edge). Confirm the chip is the ESP32-S3 before you write. The C5 target on that page is a different board (How-to C).

### 2. Hats, one at a time

Power off. If a LoRa or Hydra hat is installed and you are not using it, remove it. SaltyJack STA does not need a hat. Only one hat may be seated. Do not stack Hydra and LoRa.

### 3. Join the authorized network

The Cardputer has to be a client of that AP **before** you open Starve, Rogue DHCP (STA), Responder, or WPAD.

Three menus call the same connect screen (`feat_wifi_connect`):

| From the root menu | Then | On-screen title |
|---|---|---|
| `w` Wi-Fi | `n` Connect | WIFI CONNECT |
| `n` Network | `c` Connect | WIFI CONNECT |
| `s` System | `s` Settings, then `w` Saved Wi-Fi | WIFI CONNECT |

The failure screen inside Starve and Responder says “via System > Connect.” There is no System item named Connect. Use one of the three paths above. System → Settings → Saved Wi-Fi is the closest match to that sentence.

On WIFI CONNECT:

- `n` types a new SSID and password and stores them in Preferences (`poseidon` / `ssid` + `pass`).
- `c` connects. If the password is empty, or if you still have an AP selected from a Wi-Fi scan, it asks for the password instead of replaying a stale one.
- `x` forgets the saved network.
- Backtick (`` ` ``) goes back.

Connect uses the raw STA driver (`wifi_lean_sta_init`, then `esp_wifi_set_config` / `esp_wifi_connect`). It does not call Arduino `WiFi.begin()`, which can double-create the STA netif and reboot. It waits up to 15 seconds for an association **and** a non-zero address on `WIFI_STA_DEF`. Success shows `CONNECTED` plus IP, gateway, and mask. Failure shows `FAILED` and clears the saved password so the next attempt cannot loop on a bad passphrase.

Stay associated. Opening a raw SoftAP feature (portals, evil twin, beacon spam, Rogue DHCP (AP)) tears the STA netif down. Rejoin before going back to a STA SaltyJack tool.

### 4. Open SaltyJack

From the root menu press `j`. SaltyJack is its own screen, not a child of Network. Keys inside it:

| Key | Action |
|---|---|
| `;` / `.` | Move |
| Enter | Run the highlighted tool |
| `i` | Long description of the highlighted tool |
| `v` | Cycle list, grid, and carousel |
| `` ` `` | Back |

Tools, in order: About, DHCP Starve, Rogue DHCP (STA), Rogue DHCP (AP), Responder, WPAD NTLM, NTLMv2 Crack.

### 5. Tool order on an authorized LAN

Use this order when the goal is the STA suite. Skip any step you are not authorized to run. Rogue DHCP (AP) is documented in the next section; it is not step 2 of the STA flow.

1. **DHCP Starve** (STA). Requires `WL_CONNECTED` and a non-zero `localIP`. The tool stops the STA DHCP client (it already holds a lease; the client would otherwise consume UDP/68), binds UDP/68, and sends Discover + Request from random locally-administered MACs. The screen counts discover, offer, request, ACK, and NAK. The gateway line is `WiFi.gatewayIP()` at start. **NAK ≥ 20** flips the status to `pool exhausted`. Backtick stops the flood, restarts the STA DHCP client, and shows the totals. This keeps new legitimate clients from getting an address. Run it only on a lab LAN where that outcome is the test.
2. **Rogue DHCP (STA)**. Requires `WL_CONNECTED`. Binds UDP/67 and answers Discover with Offer and Request with ACK. Offered addresses use host suffixes **100–150** on the subnet you joined (at most 16 clients). Options in the lease:
   - router (option 3) = the Cardputer’s STA address
   - DNS (option 6) = the Cardputer’s STA address
   - domain (option 15) = `poseidon.lan`
   - WPAD (option 252) = `http://poseidon.lan/wpad.dat`
   - lease time 24 h
   
   The screen shows offers, ACKs, client count, and the last lease (IP and MAC), or `racing real server...` until the first lease. This races the real DHCP server; it is not the SoftAP.
3. **Responder** and/or **WPAD NTLM**. Both require `WL_CONNECTED`. Run the one your test plan calls for. They are separate menu items; starting one leaves the previous tool. Pairing means: starve and rogue-STA first so clients receive option 252 and treat the Cardputer as DNS, then open Responder or WPAD and leave it running.
   - **Responder** listens for LLMNR (UDP/5355, multicast 224.0.0.252), NetBIOS name service (UDP/137), and SMB (TCP/445). It answers name lookups with the Cardputer’s STA address and writes NTLMv2 material to `/poseidon/saltyjack/ntlm_hashes.txt` (hashcat 5600 shape `user::domain:challenge:proof:blob`). NetBIOS name used by the module is `POSEIDON-SJ`. The same line is printed on serial.
   - **WPAD NTLM** serves HTTP on port 80, including a PAC, then HTTP 407 with `Proxy-Authenticate: NTLM`. Captures append to the same `ntlm_hashes.txt` (commented `# wpad from <client ip>`). The IP printed at the top of the screen is the STA address when `WiFi.status()` is connected.
4. **NTLMv2 Crack** (optional, offline). Reads `/poseidon/saltyjack/ntlm_hashes.txt` and `/poseidon/saltyjack/ntlm_wordlist.txt`. Hits append to `/poseidon/saltyjack/ntlm_found.txt`. A default wordlist is seeded on first run. Enter skips a user. Backtick aborts. This tool does not check Wi-Fi.

Stop a live tool with backtick. Do not pull power in the middle of Starve if you can help it: the clean exit restarts the STA DHCP client. If you do power-cycle, Connect again and confirm a non-zero IP before the next run.

### “Not connected” — what it means

The STA tools refuse to start when the S3 is not a client with a usable lease. The exact strings:

| Tool | Condition | What the screen says |
|---|---|---|
| DHCP Starve | `WiFi.status() != WL_CONNECTED` **or** `localIP` is `0.0.0.0` | `Not connected.` / `Join target WiFi first` / `via System > Connect.` |
| Rogue DHCP (STA) | `WiFi.status() != WL_CONNECTED` | `STA not connected` / `join target WiFi first` |
| Responder | `WiFi.status() != WL_CONNECTED` | `Not connected.` / `Join target WiFi first` / `via System > Connect.` |
| WPAD NTLM | `WiFi.status() != WL_CONNECTED` | `Not connected.` / `Join target WiFi first` |
| Rogue DHCP (AP) | SoftAP bring-up failed (heap or radio), **not** a missing STA lease | `AP setup failed` / `check heap / radio` |

`Not connected` does **not** mean the W5500 cable is unplugged, and it does **not** mean the C5 is offline. Those paths are not consulted. It means the ESP32-S3 STA interface is down or has no IPv4 address.

What to check, in order:

1. You joined **before** opening the tool, using Wi-Fi → Connect, Network → Connect, or System → Settings → Saved Wi-Fi. The on-screen “System > Connect” hint is not a menu label.
2. WIFI CONNECT ended on `CONNECTED` with a non-zero IP. `FAILED` means the password was dropped; enter it again.
3. The AP is 2.4 GHz and allows this client. The S3 cannot join 5 GHz. A C5 on the bench does not fix that for SaltyJack.
4. You did not run a SoftAP feature, including Rogue DHCP (AP), after connecting. Those replace the STA netif. Connect again.
5. `UDP bind failed`, `UDP :67 bind failed`, or `listener bind failed` is a different fault: the interface was up, but the port was busy. Leave the tool, avoid overlapping listeners, and try once more. It is not the “Not connected” screen.
6. Hashes missing on the card: the capture can still be on serial (`---- SaltyJack NTLMv2 capture ----` or the WPAD equivalent). Confirm the microSD is seated. The log directory is `/poseidon/saltyjack/`.

## How-to B — W5500 RJ45

The module can be wired. **This firmware does not drive it.** [docs/hardware.html](hardware.html) marks the hat **Not wired in this tree**. `src/features/saltyjack/saltyjack.h` says attacks follow “whichever L2 lwIP is attached to — WiFi STA for now, W5500 wired once the SPI Ethernet hat is wired up.” There is no `Ethernet.begin`, no W5500 register setup, and no Ethernet netif in this tree. Plugging the jack in will not move SaltyJack off Wi-Fi, and it will not clear “Not connected.”

Until a driver exists, use How-to A.

### Pin table (Bruce / Cardputer-Adv)

SPI matches the Cardputer bus already used by the microSD (`src/sd_helper.cpp`: SCK 40, MISO 39, MOSI 14, SD chip-select 12). Chip-select and interrupt below are the Bruce-style W5500 assignment for this header. Reset is not connected. The firmware does not claim these GPIOs for Ethernet.

| Signal | Cardputer-Adv GPIO | Notes |
|---|---|---|
| SCLK | **40** | Shared HSPI clock (microSD and hats, not the display FSPI) |
| MOSI | **14** | Shared HSPI MOSI |
| MISO | **39** | Shared HSPI MISO |
| SCS | **1** | W5500 chip-select. Not the SD CS (GPIO 12). |
| INT | **2** | W5500 interrupt. Optional for a later driver; unused now. |
| RST | **NC** | Do not tie reset to a GPIO. Leave it open (module pull-up). |
| 5V | 5V | Full-size modules that expect 5V, plus GND. |
| GND | GND | Common ground with the Cardputer. |

**W5500 Mini modules are 3.3V only.** Do not power a Mini from the 5V pin. That will destroy the PHY. Use 3.3V and GND for a Mini. Confirm the silkscreen on the board in your hand before you apply power.

### Do not seat Hydra or LoRa at the same time

One hat. The header is shared.

- CAP-LoRa1262 uses HSPI plus GPIO 3 (RST), 4 (DIO1), 5 (NSS), and 6 (BUSY). GNSS UART is GPIO 13 (MCU TX) and GPIO 15 (NMEA RX).
- Hydra (CC1101 + nRF24) sits on the same HSPI pads: nRF24 CE on 4, CC1101 GDO0 on 5, nRF24 CS on 6, CC1101 CS on 13. GPIO 13 is also the LoRa-GNSS UART TX, so those two hats cannot be seated together.
- The microSD already owns SCK 40, MISO 39, MOSI 14, and CS 12 on that HSPI bus (not the display’s FSPI).
- A W5500 on SCS 1 / INT 2 still shares SCK, MOSI, and MISO with the card and with those hats.

Seating Hydra or LoRa together with the W5500 (or seating Hydra and LoRa together) fights chip-select and the SPI lines. Remove them before you attach the Ethernet module. The Feather nRF52840 hat is also mutually exclusive with LoRa / Hydra; it is not part of SaltyJack.

### What you can and cannot expect

- You can build the cable to the table above and leave it unplugged from the network until firmware exists.
- You cannot run DHCP starve, rogue DHCP, Responder, or WPAD “over the jack” on this image.
- You cannot work around “Not connected” with the cable.
- A future image would have to create an lwIP netif on the W5500 and teach those tools to accept it. That code is not here. Do not assume the roadmap bullet from v0.4 shipped it; this tree still has only the STA and SoftAP paths.

## How-to C — ESP32-C5 TRIDENT

The C5 is a second board. It talks to the Cardputer over **ESP-NOW**, protocol magic `POSE`, on Wi-Fi channel 1. It is how this fork reaches **5 GHz** Wi-Fi, **802.15.4 / Zigbee**, and **5 GHz PMKID**. The S3 cannot transmit or capture on 5 GHz by itself. The C5 menu is root `5` (C5 nodes), not `j` (SaltyJack).

SaltyJack never routes through the C5. Connecting a C5 does not join a LAN, does not create a STA lease, and does not change the “Not connected” check.

Authorized use only. 5 GHz deauth and PMKID capture are radio transmissions and credential capture. Same rule as the rest of this page: your equipment, or written permission.

### Flash the C5 (not the Cardputer)

1. **Unplug the Cardputer** from USB. If both boards are plugged in, Web Serial will often open the S3 and you will write the wrong image, or write C5 offsets onto the Cardputer.
2. Plug in only the ESP32-C5.
3. Open [docs/flash/](flash/index.html) in **desktop Chrome or Edge**. Firefox, Safari, and phone browsers do not expose Web Serial. The page also accepts desktop Opera.
4. Set Target to **TRIDENT — C5 Satellite (ESP32-C5)**. Leave the Cardputer / ESP32-S3 target alone.
5. Connect, then flash. The image is three parts, under `docs/flash/bin/trident-c5/`:

| Offset | File |
|---|---|
| `0x2000` | `bootloader.bin` |
| `0x8000` | `partition-table.bin` |
| `0x10000` | `poseidon_c5_node.bin` |

The flasher pins the C5 to 4 MB flash, DIO, 80 MHz, because automatic flash-size detect on this chip falls back to about 1 MB and the write dies past `0x100000`.

CLI equivalent, from `docs/flash/`:

```text
esptool --chip esp32c5 -p <PORT> --baud 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x2000   bin/trident-c5/bootloader.bin \
  0x8000   bin/trident-c5/partition-table.bin \
  0x10000  bin/trident-c5/poseidon_c5_node.bin
```

Building from source is a different tree: `c5_node/` with ESP-IDF (this firmware targets 5.5.4). See [c5_node/README.md](../c5_node/README.md). The bins above are what the web flasher writes.

### Auto-pair, then C5 → Status

No PIN and no SaltyJack setting. After the C5 boots it broadcasts an ESP-NOW HELLO about every 5 seconds. The Cardputer locks ESP-NOW receive to channel 1 so those HELLOs are not missed.

1. Power the Cardputer (you can plug it back in now) and power the C5 nearby.
2. Root menu `5` **C5 nodes**.
3. `s` **Status**.

Status calls `c5_begin()` and lists peers. With no HELLO yet the screen says `NO C5 ONLINE`, `power on your C5 node.`, `it broadcasts HELLO every 5s.`, `will auto-connect.` When a HELLO lands it switches to `ONLINE` and the peer name. The corner dot is green when any C5 is online and red when none are. `P` pings every peer. `S` broadcasts STOP to halt a scan the node is running.

Other C5 entries (Scan 5G+2G, Zigbee sniff, Deauth 5G, PMKID 5G, Nuke 5G) send ESP-NOW commands to that peer. They require a peer online. They do not configure SaltyJack, and SaltyJack does not call them.

### What the C5 is for

| C5 menu item | Role | SaltyJack? |
|---|---|---|
| Status | Peer table, auto-connect via HELLO | No |
| Scan 5G+2G | Remote dual-band scan; results return over ESP-NOW | No |
| Zigbee sniff | 802.15.4 channel hop, frame summaries back over ESP-NOW | No |
| Deauth 5G / PMKID 5G | 5 GHz operations the S3 cannot do | No |
| — | DHCP, LLMNR, SMB, WPAD, NTLM files | **Not on the C5.** Those stay on S3 STA lwIP. |

## SoftAP rogue DHCP (the other SaltyJack path that works)

Rogue DHCP (AP) is not How-to A and not the C5. From SaltyJack (`j`) open **Rogue DHCP (AP)**.

- SSID: `POSEIDON-SaltyJack` (open, channel 1).
- Address: 192.168.4.1 unless lwIP’s AP default has been changed.
- The Cardputer is the DHCP server. Lease options match the STA rogue tool (gateway, DNS, `poseidon.lan`, option 252).
- Clients must **join that SSID**. They are not stations of your existing AP.
- Failure is `AP setup failed` / `check heap / radio`, typically after other Wi-Fi features have fragmented the heap. Reboot and try this tool first if you need the SoftAP.
- Starting it drops the STA session. Responder and WPAD still require `WL_CONNECTED`, so they do not automatically follow onto the SoftAP in this tree. The AP tool’s own DHCP options point clients at the Cardputer; the WPAD/Responder menu items as written still check the STA association and will show “Not connected” if you are only hosting `POSEIDON-SaltyJack`.

Use the SoftAP when the test is “clients that associate to an open lab AP we operate.” Use STA when the test is “the Cardputer is already a client on an authorized LAN.”

## Quick map

|  | STA | SoftAP | W5500 | C5 |
|---|---|---|---|---|
| Radio / link | S3 joins your AP | S3 is `POSEIDON-SaltyJack` | SPI RJ45, pins above | ESP-NOW to a second board |
| lwIP interface | `WIFI_STA_DEF` | `WIFI_AP_DEF` (AP tool only) | None in this tree | None for SaltyJack |
| DHCP Starve | Yes | No | No | No |
| Rogue DHCP | STA mode | AP mode | No | No |
| Responder / WPAD | Yes, if STA is associated | Menu still requires STA | No | No |
| 5 GHz / Zigbee / PMKID | No (S3 is 2.4 GHz) | No | No | Yes, C5 menu |
| Clears “Not connected” | Only a real STA lease | No | No | No |

## Keyboard and files (short)

- Root: `j` SaltyJack, `w` then `n` or `n` then `c` to join, `5` then `s` for C5 status, `` ` `` back.
- SaltyJack: `;` `.` Enter `i` `v` `` ` ``.
- Captures: `/poseidon/saltyjack/ntlm_hashes.txt`
- Wordlist / hits: `ntlm_wordlist.txt`, `ntlm_found.txt` in that directory.
- Serial shows the same hash line even when the SD file is not opened.

## Credit

SaltyJack is a port of [@7h30th3r0n3](https://github.com/7h30th3r0n3)’s Evil-M5Project / RaspyJack work (DHCP starvation, rogue DHCP, Responder, WPAD, on-device NTLMv2). The menu About page repeats that credit. Say which network you are allowed to test before you press Enter.
