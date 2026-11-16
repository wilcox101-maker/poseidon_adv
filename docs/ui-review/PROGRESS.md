# UI Review — progress log (resume from here)

Branch: ui-review/2026-11-15 · Started 2026-11-15
Roles: UI Engineer (rendering/UX/navigation) + Senior Programming Engineer (concurrency/memory/correctness)

## Steps
- [x] 1. Setup: branch, progress file, toolchain
- [x] 2. Core UI framework review (ui, ui_components, theme, input, menu, menu_carousel, main loop)
- [ ] 3. Senior Programming Engineer review (tasks, display/SPI ownership, heap, blocking calls)
- [ ] 4. Feature-screen sweep (src/features/*: patterns across ~112 screens)
- [ ] 5. Build verification (pio run -e cardputer, pio test -e native-test)
- [ ] 6. Consolidated findings + prioritized fix plan -> docs/ui-review/FINDINGS.md

## Notes

### Step 2 — core framework (files read in full)
ui.h, ui.cpp (738L), ui_components.cpp, input.h, input.cpp, theme.h,
menu.h, menu.cpp (1378L). ui_layout consts: 240x135, STATUS_H 12,
FOOTER_H 10, BODY between. Menu tree is compile-time static; ~16 root
domains, letter-mnemonic nav, dirty-row redraw.

Strengths (keep):
- draw_menu() does true dirty-redraw: full repaint only on
  parent/window change, otherwise repaints just prev+cursor rows
  (menu.cpp:1175-1194). ui_draw_status/ui_draw_footer both cache and
  skip no-op repaints. This is the right model — don't regress it.
- Keyboard-only nav is consistent and documented: ;/. or arrows, letter
  jump, ENTER, backtick/Ctrl+[/Ctrl+C -> ESC (input.cpp). Back always
  works.
- Reduced-motion + large-text are real, NVS-backed, and checked in the
  decorative paths.

VERIFIED NON-ISSUES (checked, do not re-flag):
- THEMES[] table has 11 entries == THEME__COUNT (theme.cpp). No OOB.
  Only theme.h header *comment* still lists 7 palettes — cosmetic.
- ui_ripple() is defined but never called (grep: only ui.cpp/ui.h).
  Dead decorative code; its delay(20) loop is moot.
- ui_capture_frame()'s 135ms Serial blast fires only on PK_UI_CAPTURE
  (0x106), an injected TRIDENT/serial code unreachable from the
  physical keyboard. Not a keystroke hot-path hazard.

CANDIDATE FINDINGS (to confirm/prioritize in step 6):
- [UX/safety] Letter mnemonic both moves cursor AND immediately fires
  the action (menu.cpp:1344-1366). A single mis-key launches TX
  features (deauth/jammer/beacon) with no menu-level confirm; safety is
  left entirely to each feature. Review whether high-risk leaves should
  gate on ui_confirm().
- [structure] run_submenu() recurses per menu level (menu.cpp:1331,
  1359); the whole parent chain stays on the loopTask stack while a
  feature runs at the deepest frame. Depth is bounded (~3-4) so not a
  bug, but note stack usage; an explicit nav stack would flatten it.
- [systemic] Every modal (ui_confirm, ui_show_text, input_line,
  ui_toast, ui_action_overlay) spins its own while(){ input_poll();
  delay(10); } loop. Matches the project's cooperative-on-loopTask
  model, BUT whatever the main loop() services each tick does NOT run
  while a modal is up. Step 3 must read main.cpp loop() and list what
  stalls (GPS NMEA drain, mesh_rx is its own task so OK, screensaver,
  any SD flush). This is the key cross-cutting question.
- [minor] ui_label/ui_text use fixed 64B buffers; silent truncation via
  ui_layout::fit is intended, but confirm no feature needs >60 chars on
  one line.

Next (step 3): read src/main.cpp setup()/loop(), c5_cmd task model,
heap_budget, and grep features/* for raw delay() in render loops and
any M5Cardputer.Display writes from non-loopTask contexts
(display/SPI single-owner check).
