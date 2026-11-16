# UI Review — progress log (resume from here)

Branch: ui-review/2026-11-15 · Started 2026-11-15
Roles: UI Engineer (rendering/UX/navigation) + Senior Programming Engineer (concurrency/memory/correctness)

## Steps
- [x] 1. Setup: branch, progress file, toolchain
- [x] 2. Core UI framework review (ui, ui_components, theme, input, menu, menu_carousel, main loop)
- [x] 3. Senior Programming Engineer review (tasks, display/SPI ownership, heap, blocking calls)
- [x] 4. Feature-screen sweep (src/features/*: patterns across ~112 screens)
- [~] 5. Build verification (native-test PASS 100/100; cardputer build running)
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

### Step 3 — Senior Programming Engineer (concurrency / memory)

MAIN FINDING (positive, high-confidence): the display/SPI
single-owner invariant HOLDS project-wide.
- main.cpp loop() (line 200) only runs menu_run(); the whole UI is
  cooperative on loopTask. No per-tick servicing lives in loop(), so a
  blocking modal cannot starve it.
- Of 17 files that both xTaskCreate and draw, a brace-accurate scan
  (matched each `void fn(void*)` body) found ZERO draw calls inside any
  task body. Tasks do radio/data work (hop_task, decay_task,
  deauth_task, ...); drawing stays on loopTask. No two-context SPI use.
- triton.cpp and ble_spam.cpp deliberately avoid xTaskCreate and run
  cooperatively on loopTask (heap reasons) — matches prior analysis.
- Background services (GPS, mesh_rx pinned core1 prio3, sfx, radio
  tasks) are all independent FreeRTOS tasks. => a modal blocking
  loopTask does NOT stall radio/GPS/mesh; only menu ambient/screensaver
  pause. This DOWNGRADES the step-2 "modal starves main loop" concern
  to cosmetic.

Heap: menu_execute_action() (menu.cpp:1205) reclaims caches on feature
entry and logs "LEAK" when exit delta < -2048B. Good instrumentation.
Known one-way heap releases (portal/evil-twin BTDM) are documented in
the menu hints themselves.

NET: no concurrency/ownership defects found in the core. Remaining
candidate findings are UX/structure (letter-jump auto-fire, recursive
run_submenu) — not correctness bugs.

### Step 4 — feature-screen sweep (91 feature .cpp files)

- (b) Back path: every interactive feature (input_poll/input_line/
  ui_confirm/ui_show_text) has a return/ESC path. No dead ends.
- (a) Input lag: large delay()s are in radio TASK bodies (timing) or in
  one-shot "show result, hold, return" screens — NOT in live input
  loops. ble_finder delay(1500..2200) are post-connect result holds,
  not the hunt loop. No live-input lag found.
- (c) String: 19/91 feature files use Arduino String. Heap-frag watch
  item on a heap-tight device; not confirmed in redraw hot paths.
  Not a defect — flag for future per-file check.
- (d) Empty states: wifi_scan/ble_scan/subghz_scan all handle zero
  results. Representative list screens are fine.

CANDIDATE FINDINGS (step 6):
- [polish] ~6 files hand-roll "print msg + delay(>=800) + return"
  instead of ui_toast(): can't be dismissed early and bypass themed
  toast styling. Low. Consider an ui_hold(msg,color,ms) helper that
  polls input for early dismiss, mirroring ui_toast.

NET across steps 2-4: core architecture is sound (clean display
ownership, good dirty-redraw, consistent KB nav, real reduced-motion).
No correctness/concurrency defects. All findings are UX / structure /
polish.

Next (step 5): build. `pio run -e cardputer` (and native test env if
present). Record pass/fail + any warnings into FINDINGS.md.
