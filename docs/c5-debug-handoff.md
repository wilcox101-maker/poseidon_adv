# C5 Debug Handoff

_Written from the Claude chat session (claude.ai), for continuation in Claude Code / VS Code._

## Repo state
- Branch `master`, up to date with `origin/master` at `3c231e2` ("Update acknowledgment for contributor username")
- Large uncommitted working-tree changes across C5 node source: `c5_node/main/main.c`, `wifi_attacker.c`, `wifi_scanner.c`, `zb_sniffer.c`, `CMakeLists.txt`, plus `platformio.ini`, `src/version.h`, and numerous docs (`C5_NEXT-PHASE_PLAN.md`, `KnownDeauthBugFixSoon.md`, `RELEASE_CHECKLIST.md`, `CHANGELOG.md`, `FEATURES.md`, `README.md`, `docs/plans/*`, `docs/specs/*`, `docs/review-action-items.md`, `docs/deauth-injection-patch.md`, `docs/poseidon-sdk-compat-fix.md`, `docs/saltyjack-networking.md`, `docs/proposals/cc1101-nrf24-hat.md`)
- New untracked files: `docs/fork-status.md`, `tools/patch_libnet80211.sh` (rebuilt deauth patch script — resolves ELF section offset via `readelf` instead of scanning raw `.text`, closes a silent-corruption bug), `.gitattributes`, `.markdownlint.json`, and newly added `scripts/c5_flash_and_monitor.ps1`
- Nothing staged or committed yet — all of the above is sitting in the working tree

## Recent fixes (from prior audit session, already in working tree, not yet committed)
1. Deauth task re-entrancy: both deauth tasks called `esp_wifi_set_promiscuous(true)` + `esp_wifi_set_channel(...)` unconditionally before their first stop-check. Since `c5_deauth_dashboard()` re-arms every ~56s, a slow-to-exit old task's setup call could land after the new task's and silently retarget the burst onto the wrong channel. Fixed by gating every radio-state touch on ownership, and replacing a fixed 50ms delay with a binary semaphore so restarts wait for the old task to actually finish.
2. Neither deauth task spawn checked `xTaskCreate`'s return value. Since `s_ctx` is set before the spawn call, a failed spawn (heap exhaustion) would leave it permanently stuck — every later deauth attempt eats a timeout and never runs, until reboot. Fixed to unwind the same way a failed malloc already did.
3. `tools/patch_libnet80211.sh` rewritten: the original offset logic matched a structurally empty (0-byte) `.text` section by coincidence under `-ffunction-sections` (how ESP-IDF actually compiles) rather than the real function's own section — would have silently patched the wrong bytes in a real blob. Rewritten to resolve the symbol's actual section via `readelf` with a bounds check.

## Current hardware test session
- C5 found on `COM3` (hardware ID `USB VID:PID=303A:1001`, USB Serial Device)
- New helper script: `scripts\c5_flash_and_monitor.ps1 -Port COM3` — sources `C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1`, then runs `idf.py -C c5_node -p COM3 flash monitor`
- **Currently stuck**: esptool hanging on "waiting for device to connect" during the bootloader handshake
- Troubleshooting suggested so far (not yet confirmed which fixed it):
  - Manual reset into bootloader: hold BOOT, tap/hold RESET, release BOOT once connecting
  - Confirm no other process (VS Code serial monitor, another terminal) holds COM3
  - Try lower baud: `idf.py -C c5_node -p COM3 -b 115200 flash monitor`
  - Check for a duplicate CDC/JTAG COM port enumeration on the C5's native USB-Serial/JTOG, in case the wrong interface was picked

## Next up after C5 testing
- Cardputer side via PlatformIO: unplug C5, plug in Cardputer, `pio device list` to get its COM port, then `pio run -e cardputer -t upload --upload-port COM<y>` followed by `pio device monitor -b 115200`
- Use the app-only `upload` target (not a factory flash) if the Cardputer is on a Launcher/dual-app layout
