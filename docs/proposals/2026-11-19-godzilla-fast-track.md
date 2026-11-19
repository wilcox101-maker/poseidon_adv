# GODZILLA — standing orders: fast track (supersedes per-step orders)

**For:** GODZILLA. **From:** Claudius Maximus, 2026-11-19 01:00 ET, at Dale's direction
("streamline progress for faster turnaround"). Applies until Dale says otherwise.

## Work continuously — don't wait for orders between steps
Run straight through, in order, without waiting for a Claudius doc between steps:
1. **P2a** — finish #67, then ST77916 display + LVGL 9 status screen (per the #66 advice), then
   CST816S touch + PCF85063 RTC (pure BCD logic, native-tested).
2. **P2b** — radar UI of the node's own detections (orders #63).
3. **P2c** — functional pocket-watch face on the RTC (orders #63, Dale's spec).
4. ESP-NOW send-complete wait (carry-over from #62) — slot it wherever it's cheapest.
Then stop and write a short report PR; P3/P4 wait for Dale.

## PR mechanics
- **No drafts.** Open PRs ready for review; Claudius can't undraft (GitHub GraphQL is blocked for
  it), so a draft stalls until you flip it. **Mark #67 ready now.**
- **Stack, don't wait.** Start the next branch from your previous branch while the earlier PR is
  in review; base each PR on `master` but say "builds on #N" in the description. Claudius merges in
  order and you rebase-free merge `master` into the next branch if needed (no force-push).
- **Merge method (Claudius, from #74):** stacked GODZILLA PRs are merged with a **merge commit**,
  not squash. A squash gives `master` a new commit the next branch doesn't share, so every stacked
  PR after it conflicts (seen with #72 -> #73). With a merge commit the next PR stays clean and
  needs no rebase. Non-stacked one-off PRs may still be squashed.
- **Right-size PRs:** one topic each, but a topic may be a whole screen (driver + screen + tests),
  not one header. Fewer, fuller PRs = fewer CI cycles.
- Keep each PR's "Dale's bench" list cumulative so he can test several merged steps in one sitting.

## Decide, don't ask
Within the orders and these rules, make the call yourself and record it in the PR (what you chose,
why, what would change it). Ask Claudius only for: anything in `src/*`, any wire/C5 change, TX of
any kind, rule files, or spending. Put questions in the PR description under **"Needs Claudius"**
— Claudius reads every PR on each check-in and answers there via a follow-up doc or the merge.

## Unchanged
Passive only (nothing transmits but ESP-NOW HELLO/publish). Original artwork. Exact library version
pins. CI green incl. `godzilla`. Claudius reviews and merges, now on a ~15-minute cadence.
