# POSEIDON ADV — UI Review Findings

Branch: `ui-review/2026-11-15`
Reviewers: UI Engineer (rendering / UX / navigation) + Senior Programming
Engineer (concurrency / memory / correctness)
Scope: core UI framework + feature-screen sweep (91 feature files) +
build verification.

## Verdict

The UI is in good structural shape. No correctness or concurrency
defects were found in the core. The display/SPI single-owner invariant
holds project-wide, navigation is consistent and keyboard-complete, and
the renderer already does the right things (dirty-row redraw, cached
status/footer, allocation-free draw path, real reduced-motion). Every
finding below is UX, structure, or polish — none block a build.

## Verified solid (keep these invariants)

1. **Single display owner.** A brace-accurate scan of all 17 files that
   both `xTaskCreate` and draw found ZERO draw calls inside any task
   body. Tasks do radio/data work; drawing stays on loopTask. Do not
   regress this — never call `M5Cardputer.Display` / `ui_*` from a task.
2. **Dirty-row redraw.** `draw_menu()` full-repaints only on
   parent/window change, else repaints just prev+cursor rows
   (menu.cpp:1175-1194). `ui_draw_status` / `ui_draw_footer` cache and
   skip no-op repaints.
3. **Allocation-free draw path.** ui.cpp / ui_components.cpp allocate
   nothing on the heap per frame — fixed stack buffers and static
   pooled state (radar blips, matrix columns, EQ bars).
4. **Keyboard nav is complete and consistent.** `;`/`.` or arrows,
   letter jump, ENTER, backtick/Ctrl+[/Ctrl+C → ESC. Every interactive
   feature has a back/return path (swept all 91).
5. **loop() delegates.** main.cpp loop() only runs menu_run(); GPS,
   mesh_rx (pinned core1 prio3), sfx and per-feature radio work are all
   independent tasks, so a blocking modal starves only cosmetic menu
   ambient — not radio/GPS/mesh.
6. **Theme table integrity.** `THEMES[]` has 11 entries == `THEME__COUNT`
   (no out-of-bounds). Native tests: 100/100 pass incl. `test_ui_layout`.

## Findings (prioritized)

| # | Sev | Area | Finding | Location | Recommendation |
|---|-----|------|---------|----------|----------------|
| 1 | Med | UX/safety | Letter mnemonic BOTH moves the cursor AND immediately fires the action. A single mis-key launches a TX feature (deauth / jammer / beacon / evil-twin) with no menu-level confirm; safety is left entirely to each feature. | menu.cpp:1344-1366 | Gate high-risk leaves behind `ui_confirm()` (or a per-node `FLAG_CONFIRM`). Keeps the 2-keystroke speed for passive tools. |
| 2 | Low | Structure | `run_submenu()` recurses one frame per menu level; the whole parent chain stays on the loopTask stack while a feature runs at the deepest frame. Bounded (~3-4 deep) so not a bug. | menu.cpp:1331,1359 | Optional: flatten to an explicit nav stack. Until then, keep the tree shallow and watch loopTask stack headroom. |
| 3 | Low | Polish | ~6 features hand-roll `print msg + delay(>=800) + return` to hold a result on screen. Can't be dismissed early and bypass themed toast styling. | e.g. ble_finder.cpp:295,317,371 | Add `ui_hold(msg,color,ms)` that polls input for early dismiss (mirror `ui_toast`), and migrate these sites. |
| 4 | Low | Heap watch | 19/91 feature files use Arduino `String`. Not confirmed in redraw hot paths, but `String` churn fragments a heap that's already tight (portal/NimBLE pressure is documented). | 19 files under src/features | Per-file pass: replace `String` in any redraw/log-loop with fixed `char[]` + `snprintf`. Not urgent. |
| 5 | Cosmetic | Docs | theme.h header comment lists 7 palettes; there are 11. | src/theme.h:1-19 | Update the comment to match the enum. |
| 6 | Cosmetic | Dead code | `ui_ripple()` is defined but never called anywhere. | ui.cpp:329-338 | Remove, or wire it into keystroke feedback (note: its `delay(20)` loop would block ~140ms — if adopted, make it non-blocking). |

## Pointer usage for UI support (Senior Programming Engineer)

How pointers underpin the UI today, and the rules to keep it safe:

- **Menu tree — `const menu_node_t *`.** The entire menu is a
  compile-time-static tree in flash (`.rodata`). Child arrays are
  `const menu_node_t *children`, each terminated by a `{0}` sentinel;
  traversal walks `for (c=parent->children; c && c->hotkey; ++c)`.
  Lifetime is program-duration and read-only, so these raw pointers are
  safe by construction.
  - **Sharpest footgun:** a child array with no `{0,...}` terminator
    makes the pointer walk run off the end (buffer over-read / UB). Any
    new submenu MUST keep the sentinel. Consider a build-time check or a
    macro that emits the terminator automatically.
  - **Rule:** never construct a `menu_node_t` on the heap or stack at
    runtime. The static-tree guarantee is what makes every menu pointer
    and index safe.

- **Function pointers — `menu_action_fn` and `ui_draw_fn`
  (`void(*)(void)`).** Leaf actions dispatch via `item->action()`; the
  slide-transition painter is a `ui_draw_fn` trampoline. Both point at
  static file-scope functions (`extern void feat_*`).
  - **Rule:** keep menu actions as plain functions. Don't store a
    capturing lambda or a bound member function — a captureful lambda
    won't convert to a plain function pointer and would drag in
    `std::function` (heap). This matters on a heap-tight device.

- **Mutable global — `g_current_feature_item` (`const menu_node_t *`).**
  Set right before a feature runs so `?`/help can find its long-form
  info, cleared to `nullptr` after (menu.cpp:1203,1213-1215).
  - Safe today **only because the UI is single-threaded on loopTask**.
    If a feature ever re-enters the menu or runs help from another
    context, this needs save/restore (or a small stack), not a single
    global.

- **Trampoline statics — `s_slide_parent` / `s_slide_cursor`.** Stash a
  pointer + index so the `void()` painter callback can reach
  `draw_menu` args. Single-threaded use; no reentrancy hazard beyond the
  recursion in finding #2.

- **Caller-owned buffers, no ownership transfer.** `input_line(prompt,
  char *out_buf, size_t out_sz)` writes into a caller buffer with a size
  guard. `ui_label` / `ui_text` take `const char *` and copy into fixed
  stack buffers via `ui_layout::fit`. Nothing in the draw path allocates
  or frees. Keep it that way: new widgets that need per-frame state
  should use static/pooled storage, not `new` / `String`.

- **Dangling-index risk (not a pointer per se, same failure class).**
  Feature screens that keep a selected index into a scan-result vector
  must re-validate that index after a rescan mutates the backing store,
  and must not cache a pointer to an entry across a rescan. Flagged for
  the per-feature pass; verify in deauth/clients/BLE-scan detail views.

**Net:** the UI's pointer model is deliberately simple — static
read-only tree + plain function pointers + caller-owned buffers, zero
per-frame heap. That's the right design here. The only hard rules to
enforce going forward are: keep the menu tree static and
sentinel-terminated, keep actions as plain function pointers, and treat
`g_current_feature_item` as single-threaded-only.

## Build status

- **native-test (host):** PASS — 14 suites, 100/100 cases in 6.7s,
  including `test_ui_layout`. This covers the host-buildable UI logic.
- **cardputer (on-target firmware):** NOT completed in the cloud
  sandbox — the pioarduino ESP32 platform's `penv` dependency install
  does not finish in this environment (toolchain/platform fetch). This
  is an environment limitation, not a code defect. Run the real build on
  the Windows box: `pio run -e cardputer` (and flash per the usual
  app-only `upload`).

## On-device checks (for the test matrix, once built on hardware)

1. Menu nav feel: `;`/`.`, letter-jump, ENTER, backtick-back across ≥3
   depth levels — no dropped keystrokes, cursor position preserved on
   return from a feature.
2. Reduced-motion ON vs OFF (Display → M): confirm decorative anims stop
   but live measurements keep updating.
3. Large-text ON (Display → B): menu rows, scan results and help stay
   legible and don't overflow the 240px width.
4. A long scan (WiFi Scan with many APs): scrolling, scrollbar thumb,
   and long-SSID truncation behave; status bar C5/battery slots don't
   overlap the radio text.
5. Theme switch across several palettes while a live screen is up — no
   stale-cache artifacts in status/footer (they invalidate on theme
   change).
