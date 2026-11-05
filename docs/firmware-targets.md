# Firmware Targets — Which Build for Which Use Case

Quick reference for picking the right firmware build. Two boards: the
**Cardputer** (ESP32-S3, PlatformIO/Arduino) and the **C5 node** (ESP32-C5,
ESP-IDF, in `c5_node/`).

## Cardputer (PlatformIO envs in `platformio.ini`)

| Use case | Env | Link offset | Install method |
|---|---|---|---|
| Standalone dev unit, no Launcher — own the whole flash | `cardputer` | factory (0x0) | `pio run -e cardputer -t upload` over USB (full factory image) |
| **Standard bmorcelli Launcher (single app slot)** — the dev unit | `cardputer-launcher` | 0x170000 | Launcher → *Install from SD* / WebUI, using `firmware.bin` only |
| Dual-app Launcher (POSEIDON + Meshtastic) | `cardputer-launcher-dual` | 0x1E0000 (`poseid` slot) | App-only update via Launcher; **never** a factory image or full `pio upload` |
| Already-installed unit, update over WiFi | `cardputer-ota` | same as `cardputer` | `pio run -e cardputer-ota -t upload --upload-port <device-ip>` (auth baked in: `poseidon`) |
| Host-side unit tests, no board | `native-test` | n/a | `pio test -e native-test` |

**Current dev unit:** standard single-app Launcher → use **`cardputer-launcher`**.
The `*-dual` env/CSV describe a layout once read off this unit but it is now on
the plain Launcher; see `support_files/launcher_dual_app_8Mb.csv` header.

**Launcher install artifacts** (in `.pio/build/<env>/`):
- `firmware.bin` — the app only. This is what Launcher installs.
- `firmware.factory.bin` — bootloader + partitions + app. Full flash; **clobbers the Launcher**. Do not hand this to Launcher.

## C5 node (ESP-IDF, `c5_node/`)

| Use case | Command | Notes |
|---|---|---|
| Flash the ESP32-C5 companion node | `idf.py -C c5_node -p <UART-COM> flash` | Use the **CH343 UART** port (VID 1A86:55D3), not the native USB-JTAG (303A:1001) — UART avoids the bootloader-handshake hang |
| Build + flash + monitor in one shot | `scripts\c5_flash_and_monitor.ps1 -Port <COM>` | Sources the ESP-IDF PowerShell env first |
| Watch boot log only | `idf.py -C c5_node -p <UART-COM> monitor` | Ctrl+] to exit |

If esptool hangs on "waiting for device to connect": confirm no other process
holds the port, prefer the UART port over USB-JTAG, and if still stuck do a
manual bootloader entry (hold BOOT, tap RESET, release BOOT).
