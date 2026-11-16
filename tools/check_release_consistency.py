#!/usr/bin/env python3
"""Release-consistency gate — fail if version/flash metadata drift apart.

Catches the exact class of drift that has bitten this repo before:
  - a web-flasher manifest left at an old version string (PR #26/#27), and
  - a flasher partition image that no longer matches partitions.csv.

Checks performed:
  1. docs/manifest.json "version"            == firmware POSEIDON_VERSION (platformio.ini)
  2. docs/flash/manifest-trident-c5.json      carries that same x.y.z AND says proto-v<N>
     where <N> is POSEI_VERSION from c5_node/main/proto.h
  3. src/version.h fallback POSEIDON_VERSION   == platformio.ini POSEIDON_VERSION
  4. c5_node/main/proto.h POSEI_VERSION        == src/c5_cmd.h C5_VERSION

The committed C5 partition-table.bin vs partitions.csv is checked separately in
CI (build.yml, c5-node job) against the freshly IDF-built table — not here,
because regenerating it needs the ESP-IDF toolchain.

Run in CI and locally before cutting a release:  python3 tools/check_release_consistency.py
"""
import json
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
fails = []


def read(p):
    return (ROOT / p).read_text(encoding="utf-8")


# 1. firmware version (source of truth) from platformio.ini -D flag
m = re.search(r"POSEIDON_VERSION=\D*([0-9]+(?:\.[0-9]+)+)", read("platformio.ini"))
fw = m.group(1) if m else None
if not fw:
    fails.append("could not find -DPOSEIDON_VERSION in platformio.ini")

# proto version from proto.h
pm = re.search(r"#define\s+POSEI_VERSION\s+(\d+)", read("c5_node/main/proto.h"))
proto_v = pm.group(1) if pm else None
if not proto_v:
    fails.append("could not find POSEI_VERSION in c5_node/main/proto.h")

# 2. src/version.h fallback must match the -D (so a build without the flag agrees)
vm = re.search(r'#define\s+POSEIDON_VERSION\s+"([^"]+)"', read("src/version.h"))
if vm and fw and vm.group(1) != fw:
    fails.append(f"src/version.h fallback {vm.group(1)!r} != platformio.ini {fw!r}")

# 3. c5_cmd.h C5_VERSION must equal proto.h POSEI_VERSION (wire parity)
cm = re.search(r"#define\s+C5_VERSION\s+(\d+)", read("src/c5_cmd.h"))
if cm and proto_v and cm.group(1) != proto_v:
    fails.append(f"src/c5_cmd.h C5_VERSION {cm.group(1)} != proto.h POSEI_VERSION {proto_v}")

# 4. S3 web-flasher manifest version == firmware version
man = json.loads(read("docs/manifest.json"))
if fw and man.get("version") != fw:
    fails.append(f"docs/manifest.json version {man.get('version')!r} != firmware {fw!r}")

# 5. C5 web-flasher manifest carries the same x.y.z and the current proto number
tv = json.loads(read("docs/flash/manifest-trident-c5.json")).get("version", "")
if fw and fw not in tv:
    fails.append(f"trident-c5 manifest version {tv!r} does not contain firmware version {fw!r}")
if proto_v and f"proto-v{proto_v}" not in tv:
    fails.append(f"trident-c5 manifest version {tv!r} does not say proto-v{proto_v}")

if fails:
    print("Release consistency FAILED:")
    for f in fails:
        print("  -", f)
    sys.exit(1)

print(f"Release consistency OK (firmware {fw}, wire proto v{proto_v}).")
