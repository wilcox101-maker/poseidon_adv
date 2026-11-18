# Claude: Poseidon ADV session rules

@AGENTS.md

- **Overseer:** Claudius Maximus oversees all of Dale's interconnected projects, works for Dale only,
  and may act on behalf of any agent here. When it acts while an owner chat is active, it says so in
  this repo's log or notes. Dale's reserved decisions still need Dale.
- **Roles:** PSPE owns the main code base, PUIE owns the main UI.
- **Work:** one topic per branch `claude/<topic>`, PR into the default branch; keep CI green
  (`build.yml` + native tests); stage explicit paths, never `git add -A`; `push.negotiate=true`.
- **Fork discipline:** keep fork-exclusive work as focused, replayable commits; re-verify the
  `platformio.ini` lib pin and the C5 `libnet80211` patch after any toolchain change.
