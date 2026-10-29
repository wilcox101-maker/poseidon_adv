# POSEIDON Advanced — source 0.8.1

Firmware source in this fork is revision 0.8.1 (Hydra RF Cap pin and SPI fixes on top of upstream Deepwater). The binaries in this directory were built as **0.8.0** and were not rebuilt for 0.8.1.
`SHA256SUMS.txt` covers every binary in this directory.

| File | Purpose |
|---|---|
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
.\scripts\prepare_deepwater_release.ps1 -Version 0.8.1
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

## Catalog status (2026-10-03)

Both **POSEIDON Advanced - Deepwater** (standalone factory) and
**POSEIDON Advanced - Deepwater (Launcher)** (app only), version 0.8.0, were
uploaded through the authenticated WebBurner author page and are **pending
M5Stack public review**. Their saved Project Link is
`https://github.com/Evil0ctopus/poseidon_adv`, with a digit zero in `Evil0ctopus`.
That URL returns HTTP 200.

The separate legacy `poseidon_adv` 0.7.0 record's broken `EvilOctopus` (letter O)
project link was repaired through the authenticated desktop author editor.
The saved record and canonical public M5Burner feed now both contain the
correct `Evil0ctopus` (digit zero) URL. Launcher's cached mirror may take time
to refresh. That record still contains 0.7.0 firmware; do not mistake it for
this new release. Use the app-only GitHub download in the meantime.
