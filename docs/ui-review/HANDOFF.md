# Work-Progress Handoff Method (UI Engineer × Senior Programming Engineer)

A repeatable way to run multi-step UI work across sessions that may be
interrupted (model/environment swaps, token limits, "continue later").
The goal: any cold session — or the SPE picking up the concurrency side
— can resume in minutes without redoing verified work or re-deriving the
invariants that make the UI safe.

## The method (5 rules)

1. **One branch per effort.** `ui-review/<yyyy-mm-dd>` (or
   `ui-work/<slug>`). All checkpoints land here; master stays clean.
2. **A step log is the source of truth.** `PROGRESS.md` holds a step
   checklist (`[ ] / [~] / [x]`), plus a Notes section per completed
   step. The log is written *as work happens*, not at the end.
3. **Commit per step (checkpoint).** After each step, commit
   PROGRESS.md (and any artifact) with a `docs(ui-review): step N …`
   message. A dropped session loses at most one step.
4. **Record VERIFIED NON-ISSUES, not just findings.** Every time a
   suspected bug is checked and cleared, write it down with the evidence
   (file:line / command). This is what stops the next session from
   re-investigating the same thing. (Examples already logged: theme
   table length OK, `ui_ripple` dead, capture-frame injected-only.)
5. **Carry the invariant ledger forward (SPE).** The concurrency/memory
   rules the UI depends on live in the ledger below. Any change that
   could touch one must re-verify it and update the ledger — never
   silently.

## Resume procedure (do this first in a new session)

```
git -C <repo> switch ui-review/2026-11-15
sed -n '1,40p' docs/ui-review/PROGRESS.md   # find the first [ ]/[~] step
git log --oneline -8                         # see the last checkpoint
```
Then continue at the first unchecked step. Re-read "Verified non-issues"
in PROGRESS.md and the ledger below before touching code, so you don't
redo cleared work or break a held invariant.

## SPE invariant ledger (must stay true)

These were verified in this review (see FINDINGS.md "Verified solid").
Treat each as a guard: a change that risks it must re-prove it.

- **I1 — Single display owner.** Only loopTask draws. No
  `M5Cardputer.Display` / `ui_*` call from any FreeRTOS task body.
  (Verified: 0 draws in 17 task+draw files.)
- **I2 — Allocation-free draw path.** No per-frame heap; fixed stack
  buffers + static pooled widget state. No `new`/`String` in redraw.
- **I3 — Static, sentinel-terminated menu tree.** `menu_node_t` tree is
  `const` in flash; every child array ends with `{0,...}`. No runtime
  construction of menu nodes.
- **I4 — Plain function pointers for actions.** No capturing lambdas /
  bound members as menu actions (would pull in `std::function` heap).
- **I5 — `g_current_feature_item` is single-thread-only.** Safe only
  because the UI runs on one task. Re-entrancy needs save/restore.
- **I6 — loop() stays a delegator.** main.cpp loop() only runs
  menu_run(); background work stays in its own tasks so modals can't
  starve radio/GPS/mesh.

## Current handoff state (2026-11-15, end of review)

- Steps 1-4, 6: **done**. Step 5: native tests **100/100 pass**;
  cardputer firmware build is **env-blocked in the cloud sandbox**
  (pioarduino penv install can't complete here) — run on the Windows
  box: `pio run -e cardputer`.
- Deliverables: `FINDINGS.md` (verdict + 6 prioritized findings + SPE
  pointer-usage section), this `HANDOFF.md`, `PROGRESS.md` (step log).

### Open follow-ups (pick up here)
1. Run `pio run -e cardputer` on Windows; paste warnings/size into
   FINDINGS.md "Build status". (gate)
2. Finding #1 (Med): gate high-risk TX leaves behind `ui_confirm()` or a
   per-node `FLAG_CONFIRM`. Decide design with Dale first.
3. Finding #3 (Low): add `ui_hold(msg,color,ms)` (toast-style, early
   dismiss) and migrate the ~6 hand-rolled `print+delay+return` holds.
4. Finding #4 (Low): per-file `String`→`char[]`/`snprintf` pass in the
   19 feature files; prioritize any in redraw/log loops.
5. Findings #5/#6 (cosmetic): fix theme.h comment (7→11); remove or
   non-blocking-ify dead `ui_ripple()`.
6. Dangling-index audit in scan-detail views (deauth/clients/BLE scan)
   per the SPE note.
7. After any fix: re-run `pio test -e native-test`, then the on-device
   checks in FINDINGS.md, then a POS-AUDIT entry (next free #127).
