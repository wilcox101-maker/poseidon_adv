# POSEIDON Advanced — 0.9.0

Firmware source in this fork is revision **0.9.0** (the TRIDENT C5-node release on top of upstream Deepwater). The binaries in this directory were **built as 0.9.0** from the matching source and carry the `0.9.0` version string.
`SHA256SUMS.txt` covers every binary in this directory and matches the assets attached to the [v0.9.0 GitHub release](https://github.com/wilcox101-maker/poseidon_adv/releases/tag/v0.9.0).

| File | Purpose |
| --- | --- |
| `poseidon-factory.bin` | Standalone/M5Burner factory image at `0x0` |
| `poseidon_adv_factory.bin` | Legacy alias of the factory image |
| `poseidon_adv.bin` | Standalone application only, for a matching existing partition layout |
| `poseidon-launcher.bin` | Application only for installation through M5Stack Launcher's SD/WebUI |
| `poseidon-launcher-dual.bin` | Application only for the verified development unit's dual-app layout |

**Factory images replace the flash layout. Never flash one over an existing
Launcher/multi-app installation.** Prefer Launcher's own app installer.
For raw app-only updates, read and verify the actual device partition table
before deciding the offset. The dual-app development unit has POSEIDON at
`0x1E0000`, capacity `0x2D0000`, and Meshtastic at `0x4B0000`.

Build all variants:

```text
pio run -e cardputer -e cardputer-launcher -e cardputer-launcher-dual
```

Then refresh these tracked release artifacts:

```powershell
.\scripts\prepare_deepwater_release.ps1 -Version 0.9.0
```

The personal website's Flash hub serves a checksum-verified copy of the
standalone image from its separate `firmware/poseidon_adv/` package.
This firmware repository's historical website is not the personal website
and is not deployed through GitHub Pages.
The M5Burner metadata at `docs/m5burner.json` points to the versioned GitHub
release. Actual M5Burner catalog publication requires an author upload;
committing that metadata alone does not publish a catalog entry.

Publish user-created firmware through M5Stack's authenticated
[WebBurner](https://burner.m5stack.com/). Preserve an existing Launcher listing's
app-only install format when uploading `poseidon-launcher.bin`; publish the
standalone factory image separately. Launcher catalog availability must be
checked independently after that upload. Direct GitHub app downloads work
without waiting for a catalog entry.

## Release status (2026-11-05)

The **0.9.0** binaries above are published as assets on the
[v0.9.0 GitHub release](https://github.com/wilcox101-maker/poseidon_adv/releases/tag/v0.9.0)
and can be downloaded and flashed directly without waiting for any catalog.
`docs/m5burner.json` points its firmware `url` at the standalone factory asset
of that release.

M5Burner **public catalog** publication is a separate, manual step: it requires
an upload through M5Stack's authenticated [WebBurner](https://burner.m5stack.com/)
author page and then M5Stack public review. Committing `docs/m5burner.json` does
not publish a catalog entry. Until that upload happens, use the GitHub release
assets (standalone factory, Launcher app-only, or the dual-app image). The older
`poseidon_adv` 0.7.0 catalog record still contains 0.7.0 firmware; do not mistake
it for this release.
