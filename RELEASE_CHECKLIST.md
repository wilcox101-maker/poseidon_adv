# POSEIDON Release Checklist

Operational cheat sheet — everywhere that has to be touched when
cutting a new POSEIDON version. Generated from the v0.5.0 → 0.6 push
audit (2026-05-22) where multiple stale strings + an untracked
`docs/flash/` dir + Jekyll silently failing bit us.

If you're cutting a release and reading this, work top-to-bottom.

---

## 0. Pre-flight

- [ ] Confirm the gating feedback rule: **do not push, tag, or release
      POSEIDON without explicit user go-ahead each time** (per
      `feedback_poseidon_github_gate.md`).
- [ ] `git status` clean except for files you intend to commit.
- [ ] All hardware-in-loop tests in the previous version still pass —
      regression sweep at minimum (boot, menu, one leaf per submenu).
- [ ] Pick the next version number (SemVer — bug-fix → patch,
      backward-compatible feature → minor, breaking change → major).

## 1. Source-code version constants

- [ ] `platformio.ini` → `-DPOSEIDON_VERSION=\"X.Y.Z\"` build flag.
      `src/version.h` reads it. Boot splash + serial harness use it.
- [ ] `c5_node/main/proto.h` → `POSEI_VERSION` if the wire protocol
      changed. Bump only when CMD/RESP IDs change — most C5 commits
      don't need this.
- [ ] If the C5 firmware version is independent of POSEIDON, also
      record it in `docs/flash/manifest-trident-c5.json` "version"
      field (currently `proto-vN-YYYY-MM-DD`).

## 2. Build fresh artifacts

- [ ] **POSEIDON (S3, Cardputer-Adv):**

  ```bash
  PYTHONIOENCODING=utf-8 PYTHONUTF8=1 \
    /c/Users/D/.platformio/penv/Scripts/pio.exe run -e cardputer
  ```

  Produces `.pio/build/cardputer/firmware.factory.bin`. The launcher
  variant (`-e cardputer-launcher`) is a separate build with a
  different partition table.

- [ ] **TRIDENT (C5):**

  ```bash
  cd c5_node
  ./_build.bat build
  ```

  Produces `c5_node/build/{bootloader,partition_table,poseidon_c5_node}.bin`.

- [ ] **TRIDENT factory.bin** (single combined image for the
      `install.html` flow) — concatenate the three C5 bins at their
      flash offsets:

  ```bash
  cd c5_node/build
  esptool.py --chip esp32c5 merge_bin -o trident-factory.bin \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x2000  bootloader/bootloader.bin \
    0x8000  partition_table/partition-table.bin \
    0x10000 poseidon_c5_node.bin
  ```

  This is what gets attached to the GitHub release for the
  install.html → `manifest-trident.json` web-flash flow.

## 3. Refresh web flasher binaries (`docs/flash`)

- [ ] Copy fresh C5 bins into the docs flasher tree:

  ```bash
  cp c5_node/build/bootloader/bootloader.bin            docs/flash/bin/trident-c5/
  cp c5_node/build/partition_table/partition-table.bin  docs/flash/bin/trident-c5/
  cp c5_node/build/poseidon_c5_node.bin                 docs/flash/bin/trident-c5/
  ```

  Also mirror into `docs/bin/trident-c5/` (duplicate location kept
  for legacy references — delete this when nothing points at it).

- [ ] `.gitignore` already has `!docs/flash/bin/**/*.bin` +
      `!docs/bin/**/*.bin` negation rules so the bins ARE trackable.
      Verify with `git check-ignore -v docs/flash/bin/trident-c5/*.bin`.

- [ ] Bump `version` in `docs/flash/manifest-trident-c5.json`.

- [ ] Sanity-check the manifest is **pure ASCII** — UTF-8 em-dashes
      in past versions got cp1252-encoded by editors and broke
      browser parsing. The current file uses `-` not `—` for safety:

  ```bash
  python -c "raw=open('docs/flash/manifest-trident-c5.json','rb').read(); print('all ASCII:', all(b<0x80 for b in raw))"
  ```

## 4. Documentation updates

The complete touch list from the v0.5.0 → 0.6 audit:

### Repo docs

- [ ] **`CHANGELOG.md`** — promote `[Unreleased]` to `[X.Y.Z] - YYYY-MM-DD`,
      open a new empty `[Unreleased]` section above it.
- [ ] **`README.md`** — check these specifically:
  - Feature count in the hero blurb ("95+ features")
  - Feature count in the section header ("## Feature Matrix (95+)")
  - Per-section counts (WiFi 17, BLE 16, IR 4, etc.) — count menu
    entries in `src/menu.cpp` if a section grew
  - Theme count + theme list (currently 3: POSEIDON / MATRIX / E-INK)
  - Quick Start binary URL — points at `releases/latest/download/...`
    which auto-redirects, no version edit needed unless the asset
    name itself changes
- [ ] **`TESTERS.md`** — "Currently testing" line at the top,
      priority list ordering, anything that says "vX.Y.Z shipped".
- [ ] **`KnownDeauthBugFixSoon.md`** — refresh if any known bugs got
      fixed in this version, or new ones surfaced.
- [ ] **`c5_node/README.md`** — protocol version, build flow.

### Website pages (in `docs/`; GitHub Pages is not enabled)

- [ ] **`docs/index.html`** — four spots that go stale:
  - `<div id="hud">` HUD text (`LIVE · vX.Y.Z ...`)
  - `<div class="hero-badge">` (the lit banner above the hero title)
  - `<div class="hero-stats">` numbers (Features / RF + Wired / Themes
    / .sub Signals)
  - `<div class="hero-btns">` if a new CTA replaces an old one
- [ ] **`docs/install.html`** — the "once we tag vX.Y.Z" M5Burner
      sentence + the esp-web-tools script tag URL (currently
      `jsdelivr@10.2.1`, do NOT switch to unpkg, see §6).
- [ ] **`docs/arsenal.html`** — `v0.X HEADLINE` / `v0.X FIXED` tags
      on feature cards are historical labels; you can add new ones
      for the current version's headline features.
- [ ] **`docs/credits.html`** — `vX.Y headline credit` mentions if
      this version has a credit that headlines the release.
- [ ] **`docs/hardware.html`** — only touch if hardware list changes
      (new hat, new MCU variant, etc.).
- [ ] **`docs/roadmap.html`** — move items from "in flight" to
      "shipped"; add new "in flight" items for next milestone.
- [ ] **`docs/testers.html`** — mirror of `TESTERS.md` priorities;
      HUD string + meta description.
- [ ] **`docs/flash/index.html`** — only touch if you're adding a
      new target board card (e.g. POSEIDON itself once we get web
      flashing for the S3, or a new satellite).

### Manifests

- [ ] **`docs/flash/manifest-trident-c5.json`** — bump version,
      ASCII-clean. Used by `flash/index.html` (the C5 install button).
- [ ] **`docs/manifest-trident.json`** — C5 manifest. Upstream
      releases do not publish `trident-factory.bin`, so the parts
      point at the in-tree bins under `docs/flash/bin/trident-c5/`
      (same offsets as `docs/flash/manifest-trident-c5.json`). Bump
      the `"version"` field when those bins change.
- [ ] **`docs/m5burner.json`** — **PREPEND a new entry to the
      `firmware[]` array** for this version (do NOT replace the
      existing entries; M5Burner shows them as a version history
      dropdown). Required fields per entry:

  ```json
  {
    "name":        "POSEIDON vX.Y.Z",
    "version":     "X.Y.Z",
    "chip":        "esp32s3",
    "target":      "M5Cardputer-Adv",
    "flash_size":  "8MB",
    "psram":       true,
    "url":         "https://github.com/wilcox101-maker/poseidon_adv/releases/download/vX.Y.Z/poseidon-factory.bin",
    "offset":      "0x0",
    "description": "<one-paragraph release headline>",
    "date":        "YYYY-MM-DD",
    "changelog":   "https://github.com/wilcox101-maker/poseidon_adv/blob/master/CHANGELOG.md",
    "license":     "MIT"
  }
  ```

  Also refresh the top-level `description` field if the feature
  count or headline blurb has changed (currently says "90+ features"
  — bump to match README).

### Featured-feature labels in the website

- [ ] **`docs/arsenal.html`** — feature cards carry `<div class="count">vX.Y HEADLINE</div>`
      and `vX.Y FIXED` badges. Add ones for this version's
      headline + fixed features, and demote prior-version tags to
      historical (they can stay — they're useful "this landed in vN"
      breadcrumbs for users).
- [ ] **`docs/roadmap.html`** — move items from "in flight" to
      "shipped" with the release tag; add new "in flight" items for
      the next milestone.
- [ ] **`docs/credits.html`** — if a specific contributor / upstream
      project was load-bearing for this release (the way SaltyJack
      credit headlined v0.4), add a "vX.Y headline credit" pin.

### Featured-feature counts to keep in sync

Any time you add a feature, FOUR places need to be updated together
or the counts drift:

- [ ] `README.md` — section counts (`### WiFi (17)`, `### BLE (16)`,
      etc.) AND the top-level `95+` blurb AND the `## Feature
      Matrix (95+)` heading.
- [ ] `docs/index.html` — `<div class="hero-stats">` "Features"
      counter (`data-count="100"`).
- [ ] `docs/m5burner.json` — top-level `description` blurb.
- [ ] `docs/arsenal.html` — the feature you added needs a card.

## 5. Commit + push order

```bash
# Stage doc updates separately from version bumps if you want a
# clean release commit history. Otherwise one commit is fine.
git add CHANGELOG.md README.md TESTERS.md docs/
git add platformio.ini c5_node/main/proto.h   # if version constants changed
git add docs/flash/bin/trident-c5/            # if rebuilt
git commit -m "release: vX.Y.Z"

# Tag and push BEFORE creating the release, so the release can
# reference the tag.
git tag vX.Y.Z
git push origin master --tags
```

## 6. GitHub release

```bash
gh release create vX.Y.Z \
  --title "POSEIDON vX.Y.Z — <release codename>" \
  --notes-file CHANGELOG-extract.md \
  .pio/build/cardputer/firmware.factory.bin#poseidon-factory.bin \
  .pio/build/cardputer-launcher/firmware.bin#poseidon-launcher.bin \
  c5_node/build/trident-factory.bin#trident-factory.bin
```

`CHANGELOG-extract.md` is just the `[X.Y.Z]` section copy-pasted from
`CHANGELOG.md`. The `#friendly-name.bin` suffix renames the uploaded
asset so it matches what `install.html` + `manifest-trident.json`
expect (`releases/latest/download/poseidon-factory.bin` etc.).

For this fork's Deepwater release, build all three Cardputer profiles and run
`scripts/prepare_deepwater_release.ps1 -Version X.Y.Z` to refresh tracked
standalone/Launcher/dual-app binaries and SHA256 sums. Update the separate
`Evil0ctopus/Evil0ctopus.github.io` personal website package from those artifacts.
Publish against `wilcox101-maker/poseidon_adv`, not the upstream repository.
Use `firmware.bin`, never `firmware.factory.bin`, for the Launcher release asset.
The dual-app asset is only for the verified custom layout; never distribute it
as a factory install. Do not rebuild or relabel unchanged TRIDENT firmware for
an S3-only UI release.

M5Burner catalog publication is a separate authenticated author upload, not an
automatic consequence of pushing a GitHub tag or editing `docs/m5burner.json`.
Verify the public catalog after publishing. Launcher can also install the
app-only GitHub release directly through SD/WebUI or a URL favorite.

## 7. Post-push verification

GitHub Pages is not enabled for this fork or for
`wilcox101-maker/poseidon_adv`. Do not poll a `github.io` host. Confirm
the docs files on `master` via raw GitHub. Raw updates a few seconds
after the push.

```bash
until curl -fsSL https://raw.githubusercontent.com/wilcox101-maker/poseidon_adv/master/docs/index.html | grep -q "vX.Y.Z"; do sleep 5; done
echo "on master"
```

Check each file independently:

```bash
base=https://raw.githubusercontent.com/wilcox101-maker/poseidon_adv/master
curl -fsSL -o /dev/null -w "%{http_code}\n" $base/docs/index.html
curl -fsSL -o /dev/null -w "%{http_code}\n" $base/docs/flash/index.html
curl -fsSL -o /dev/null -w "%{http_code}\n" $base/docs/flash/manifest-trident-c5.json
curl -fsSL -o /dev/null -w "%{http_code} %{size_download}\n" \
  $base/docs/flash/bin/trident-c5/poseidon_c5_node.bin
```

If a raw URL 404s, that file is not on `master` yet.

If the release-asset flow is broken, check the upstream asset list
(releases are published on `wilcox101-maker/poseidon_adv`):

```bash
gh release view vX.Y.Z --repo wilcox101-maker/poseidon_adv --json assets --jq '.assets[].name'
```

## 8. Common gotchas (each cost a debug round)

- **`*.bin` is gitignored.** Anything you copy into `docs/` won't
  appear in `git status` without the negation rules. Verify with
  `git check-ignore -v <file>`.
- **`docs/.nojekyll` must exist.** Without it, GitHub Pages runs
  Jekyll, which chokes on any `{{` literal in any md file under docs/
  (Python f-strings under `docs/superpowers/plans/` killed the build
  for the entire site on the v0.5.0 → 0.6 push). Build silently
  reports `building` while the worker shows red.
- **Pages serves from `master:/docs`** not `main`. Don't rebase onto
  a `main` branch and assume Pages will pick it up — config it first.
- **Em-dash encoding hazard.** Windows editors save manifest JSON as
  cp1252 (em-dash = byte `0x97`), browsers parse as UTF-8 and see an
  invalid byte. Stick to ASCII `-` in JSON or verify the file is
  proper UTF-8 (`raw[i] for i where 0xC2 <= raw[i] <= 0xF4`).
- **esp-web-tools CDN.** `cdn.jsdelivr.net/npm/esp-web-tools@10.2.1`
  works. `unpkg.com/esp-web-tools@10` serves edge-cached 10.0-era
  bundles that lack ESP32-C5 support — users see false "Browser not
  supported" because the custom-element script never registers.
- **ESP32-C5 + Cardputer-Adv share USB VID/PID** (0x303A:0x1001).
  When both are plugged in, esp-web-tools will let you pick either
  port but flash will fail with "Failed to initialize" if you pick
  the wrong chip (S3 vs C5). Test by enumeration order or USB
  device path.
- **C5 doesn't always auto-reset into download mode.** Hold BOOT
  (GPIO 9), tap RESET, release BOOT. Some C5 dev boards lack the
  DTR/RTS auto-reset wiring.

## 9. Getting into the official M5Burner + Launcher catalogs

### M5Burner (M5Stack's official desktop flasher)

M5Burner has a built-in "custom firmware" tab where users paste a
manifest URL and it adds the firmware to their personal catalog.
Ours is at `https://raw.githubusercontent.com/wilcox101-maker/poseidon_adv/master/docs/m5burner.json`
— shareable today without M5Stack's involvement.

**To get on the *default* catalog** (shows up for every M5Burner
user without them adding a URL):

1. Open an issue at <https://github.com/m5stack/M5Burner> requesting
   inclusion. Reference the manifest URL above and the homepage.
2. The bar for inclusion: working public-release firmware with a
   stable manifest schema. We meet both — manifest matches the same
   schema Bruce/EvilCardputer/Marauder use, and we have tagged
   releases.
3. Bruce, ESP32Marauder, and Evil-M5Project all got in via the same
   path. Look at their inclusion PRs for the exact metadata format
   M5Stack wants in the issue body.
4. Approval usually takes ~1-2 weeks; M5Stack rebuilds the bundled
   catalog with new releases.

### bmorcelli's Launcher (community alternative bootloader)

The Launcher app at <https://github.com/bmorcelli/Launcher> reads a
remote manifest of community firmwares. To get listed:

1. Verify the `cardputer-launcher` PIO env still produces a
   compatible image — partition table is `support_files/launcher_8Mb.csv`
   linking to `app1/ota_0` slot at `0x170000`, capped 0x4F0000.
2. Open a PR against the launcher manifest in bmorcelli's repo
   adding a POSEIDON entry. Reference the launcher-variant release
   asset (`poseidon-launcher.bin` in our `gh release create` step).
3. The Launcher's "Install from SD" flow already works for users
   who manually copy the launcher.bin to their SD — community
   listing just makes discovery easier.

### Cardputer-Adv firmware showcases (third-party)

- **Evil-M5Project's awesome list** at
  <https://github.com/7h30th3r0n3/Evil-Cardputer> often lists
  related Cardputer firmwares. PR a one-liner pointing at our
  homepage to land in the "Other Cardputer firmware projects"
  section.
- **r/Cardputer** + **r/M5Stack** subreddits — a one-time release
  announcement with screenshot of the web flasher tends to drive
  the most traffic. Pin it to user's profile after posting.

## 10. Things that auto-update (don't touch)

- `releases/latest/download/<asset>.bin` URLs — GitHub auto-resolves
  `latest` to whatever tag was published most recently. No manifest
  edit needed unless the asset *name* changes.
- POSEIDON's boot splash version — read from
  `-DPOSEIDON_VERSION=...` in `platformio.ini` at compile time via
  `src/version.h`. Only the build flag needs editing.
- The c5_node build's flash offsets (`build/flash_args`) are
  regenerated by IDF every build; don't hand-edit.

## 11. Update memory after a release

Update `~/.claude/projects/.../memory/` so future-me knows where things landed:

- [ ] `project_poseidon.md` — bump current version, summarize the
      release headline.
- [ ] Any `feedback_poseidon_*` memory whose recipe changed (e.g.
      WiFi AP recipe, IR LED polarity) — verify still accurate.
- [ ] Memory pointer to this file (`RELEASE_CHECKLIST.md`) in
      `MEMORY.md` index — already in place.
