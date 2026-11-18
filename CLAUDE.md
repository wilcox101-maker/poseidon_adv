# Claude: Poseidon ADV session rules

@AGENTS.md

- **Overseer:** Claudius Maximus oversees across projects and works for Dale only; it may align rules
  and docs here, landing changes as PRs for Dale to merge.
- **Roles:** PSPE owns the main code base, PUIE owns the main UI.
- **Work:** one topic per branch `claude/<topic>`, PR into the default branch; keep CI green
  (`build.yml` + native tests); stage explicit paths, never `git add -A`; `push.negotiate=true`.
- **Fork discipline:** keep fork-exclusive work as focused, replayable commits; re-verify the
  `platformio.ini` lib pin and the C5 `libnet80211` patch after any toolchain change.
