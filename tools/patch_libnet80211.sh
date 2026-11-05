#!/usr/bin/env bash
#
# patch_libnet80211.sh — make ieee80211_raw_frame_sanity_check() return 0
# unconditionally inside the ESP32-C5 ESP-IDF WiFi blob (libnet80211.a).
#
# Background (CHANGELOG.md, v0.5.0 "Added — TRIDENT"): esp_wifi_80211_tx()
# on the C5's IDF 5.5 build is blocked by ieee80211_raw_frame_sanity_check
# inside libnet80211.a for management subtypes 0xA (disassoc) / 0xC
# (deauth) — the same filter class documented in KnownDeauthBugFixSoon.md
# for the Cardputer side. esp_wifi_80211_tx() still returns ESP_OK even
# when the blob drops the frame internally, so an unpatched build LOOKS
# like it's working (status counter climbs, no error) while transmitting
# nothing. On the Cardputer/Xtensa side this is fixed with a link-time
# symbol override (src/features/wifi_sanity_override.cpp). That does NOT
# work here: the caller (esp_wifi_80211_tx) and the sanity-check function
# live in the SAME object file inside the C5's libnet80211.a, so the linker
# has already resolved the call internally before our object file is even
# considered — neither --wrap nor --allow-multiple-definition can intercept
# it. The only fix that works is a direct binary patch: overwrite the
# function body in place with two RISC-V instructions,
#     li  a0, 0      ; 13 05 00 00  (addi a0, x0, 0)
#     ret            ; 67 80 00 00  (jalr x0, ra, 0)
# so it unconditionally returns 0 ("not filtered") for any input.
#
# IMPLEMENTATION NOTE — why this patches the archive in place rather than
# ar-extract / modify / ar-replace (which an earlier version of this script
# did): under the riscv32-esp-elf ar in a Windows/MSYS toolchain, that round
# trip was NOT byte-stable — ar replaced the member but reading it back no
# longer matched the bytes written, so the verify always failed and rolled
# back, silently shipping an unpatched node. We instead parse the `ar`
# format and the member's ELF section/symbol tables directly (pure python,
# no nm/ar/readelf dependency), locate the function's absolute byte offset
# in the .a, and overwrite it there. The write is length-preserving (padded
# to the exact symbol size with NOPs, which never execute because `ret`
# returns first) so the archive structure is untouched.
#
# Usage:
#   tools/patch_libnet80211.sh [path/to/libnet80211.a]
#
# With no argument, searches $IDF_PATH's esp32c5 component libs for the
# first libnet80211.a it finds. Safe to re-run: detects an already-patched
# archive and exits 0 without touching it. On any uncertainty it refuses
# and exits non-zero rather than guess — this patches a prebuilt blob.
#
# Requires: python3 (or python) from your ESP-IDF toolchain environment.

set -euo pipefail

err()  { echo "patch_libnet80211.sh: $*" >&2; exit 1; }
info() { echo "patch_libnet80211.sh: $*"; }

# ---- locate the archive ----------------------------------------------
LIBNET80211="${1:-}"
if [ -z "$LIBNET80211" ]; then
    [ -n "${IDF_PATH:-}" ] || err "no path given and \$IDF_PATH is not set — pass the libnet80211.a path explicitly"
    LIBNET80211="$(find "$IDF_PATH" -path '*esp32c5*' -name 'libnet80211.a' 2>/dev/null | head -n1 || true)"
    [ -n "$LIBNET80211" ] || err "couldn't find an esp32c5 libnet80211.a under \$IDF_PATH ($IDF_PATH) — pass the path explicitly"
fi
[ -f "$LIBNET80211" ] || err "not a file: $LIBNET80211"

# ---- pick a working python ------------------------------------------
PYTHON=""
for cand in python3 python; do
    if command -v "$cand" >/dev/null 2>&1 && "$cand" -c 'import sys' >/dev/null 2>&1; then
        PYTHON="$cand"; break
    fi
done
[ -n "$PYTHON" ] || err "need python3 (or python) on PATH — run \$IDF_PATH/export.sh first"

info "target archive: $LIBNET80211"

# ---- do the work in python ------------------------------------------
"$PYTHON" - "$LIBNET80211" <<'PY'
import sys, struct, shutil

SYMBOL = b"ieee80211_raw_frame_sanity_check"
PATCH  = bytes.fromhex("1305000067800000")   # li a0,0 ; ret  (RV32, little-endian)
NOP4   = bytes.fromhex("13000000")           # addi x0,x0,0
CNOP2  = bytes.fromhex("0100")               # c.nop (RVC), for 2-byte top-up

path = sys.argv[1]

def fail(msg):
    sys.stderr.write("patch_libnet80211.sh: %s\n" % msg); sys.exit(1)

with open(path, "rb") as f:
    data = f.read()

# ---- parse the `ar` archive, resolving // long names -----------------
if data[:8] != b"!<arch>\n":
    fail("not an ar archive: %s" % path)
members = []        # (name, data_off, size)
longnames = b""
pos = 8
while pos + 60 <= len(data):
    hdr = data[pos:pos+60]
    raw = hdr[0:16].decode("latin1").rstrip()
    try:
        size = int(hdr[48:58].decode("latin1").strip())
    except ValueError:
        fail("malformed ar header at 0x%x" % pos)
    doff = pos + 60
    if raw == "//":
        longnames = data[doff:doff+size]
        name = raw
    elif raw.startswith("/") and raw[1:].isdigit():
        off = int(raw[1:]); end = longnames.find(b"/\n", off)
        name = longnames[off:end].decode("latin1")
    elif raw.endswith("/"):
        name = raw[:-1]
    else:
        name = raw
    members.append((name, doff, size))
    pos = doff + size + (size & 1)   # members are 2-byte aligned

# ---- find which member defines SYMBOL, and its absolute file offset --
def elf_find_symbol(obj):
    """Return (sh_offset_of_symbols_section, st_value, st_size) for SYMBOL, or None."""
    if obj[:4] != b"\x7fELF":
        return None
    is64 = obj[4] == 2
    if is64:
        e_shoff = struct.unpack_from("<Q", obj, 0x28)[0]
        e_shentsize, e_shnum = struct.unpack_from("<HH", obj, 0x3a)
    else:
        e_shoff = struct.unpack_from("<I", obj, 0x20)[0]
        e_shentsize, e_shnum = struct.unpack_from("<HH", obj, 0x2e)
    def sh(i):
        b = e_shoff + i*e_shentsize
        if is64:
            sh_type = struct.unpack_from("<I", obj, b+4)[0]
            sh_off  = struct.unpack_from("<Q", obj, b+0x18)[0]
            sh_size = struct.unpack_from("<Q", obj, b+0x20)[0]
            sh_link = struct.unpack_from("<I", obj, b+0x28)[0]
            sh_esz  = struct.unpack_from("<Q", obj, b+0x38)[0]
        else:
            sh_type = struct.unpack_from("<I", obj, b+4)[0]
            sh_off  = struct.unpack_from("<I", obj, b+0x10)[0]
            sh_size = struct.unpack_from("<I", obj, b+0x14)[0]
            sh_link = struct.unpack_from("<I", obj, b+0x18)[0]
            sh_esz  = struct.unpack_from("<I", obj, b+0x24)[0]
        return sh_type, sh_off, sh_size, sh_link, sh_esz
    for i in range(e_shnum):
        sh_type, sh_off, sh_size, sh_link, sh_esz = sh(i)
        if sh_type != 2:            # SHT_SYMTAB
            continue
        _, str_off, str_size, _, _ = sh(sh_link)
        esz = sh_esz or (24 if is64 else 16)
        for n in range(0, sh_size, esz):
            e = sh_off + n
            if is64:
                st_name  = struct.unpack_from("<I", obj, e)[0]
                st_info  = obj[e+4]
                st_shndx = struct.unpack_from("<H", obj, e+6)[0]
                st_value = struct.unpack_from("<Q", obj, e+8)[0]
                st_size  = struct.unpack_from("<Q", obj, e+16)[0]
            else:
                st_name  = struct.unpack_from("<I", obj, e)[0]
                st_value = struct.unpack_from("<I", obj, e+4)[0]
                st_size  = struct.unpack_from("<I", obj, e+8)[0]
                st_info  = obj[e+12]
                st_shndx = struct.unpack_from("<H", obj, e+14)[0]
            if (st_info & 0xf) != 2:   # STT_FUNC
                continue
            end = obj.find(b"\x00", str_off+st_name)
            if obj[str_off+st_name:end] != SYMBOL:
                continue
            _, tgt_off, _, _, _ = sh(st_shndx)   # sh() -> (type, OFFSET, size, link, entsize)
            return (tgt_off, st_value, st_size)
    return None

found = []
for name, doff, size in members:
    if name in ("//", "/", "/SYM64/"):
        continue
    res = elf_find_symbol(data[doff:doff+size])
    if res:
        found.append((name, doff, res))

if not found:
    fail("no archive member defines %s — is this the esp32c5 libnet80211.a, "
         "and has its layout changed?" % SYMBOL.decode())
if len(found) > 1:
    fail("%s defined in %d members (%s) — refusing to guess"
         % (SYMBOL.decode(), len(found), ", ".join(m[0] for m in found)))

name, doff, (sec_off, st_value, st_size) = found[0]
abs_off = doff + sec_off + st_value
sys.stderr  # keep linters quiet
print("patch_libnet80211.sh: symbol defined in: %s" % name)
print("patch_libnet80211.sh: function at archive offset 0x%x, size %d bytes" % (abs_off, st_size))

if st_size < len(PATCH):
    fail("%s is only %d bytes, need %d — layout may have changed, refusing"
         % (SYMBOL.decode(), st_size, len(PATCH)))

cur = data[abs_off:abs_off+len(PATCH)]
if cur == PATCH:
    print("patch_libnet80211.sh: already patched (bytes match) — nothing to do")
    sys.exit(0)
print("patch_libnet80211.sh: current bytes at patch site: %s" % cur.hex())

# ---- build a length-EXACT body (critical: wrong length malforms the ar) --
body = bytearray(PATCH)
while len(body) + 4 <= st_size:
    body += NOP4
while len(body) < st_size:
    body += CNOP2
body = bytes(body[:st_size])
if len(body) != st_size:
    fail("internal: body %d != symbol size %d" % (len(body), st_size))

buf = bytearray(data)
buf[abs_off:abs_off+st_size] = body
if len(buf) != len(data):
    fail("internal: archive length changed (%d != %d)" % (len(buf), len(data)))

shutil.copyfile(path, path + ".bak")
with open(path, "wb") as f:
    f.write(buf)

# ---- verify straight off disk ----------------------------------------
with open(path, "rb") as f:
    back = f.read()
if back[abs_off:abs_off+len(PATCH)] != PATCH or len(back) != len(data):
    shutil.copyfile(path + ".bak", path)
    fail("verification failed after write — restored original from .bak, nothing left patched")

print("patch_libnet80211.sh: wrote li a0,0 ; ret at offset 0x%x, NOP-padded to %d bytes" % (abs_off, st_size))
print("patch_libnet80211.sh: patched and verified: %s now unconditionally returns 0" % SYMBOL.decode())
print("patch_libnet80211.sh: original archive backed up to %s.bak" % path)
PY

info "re-run a full c5_node build so the patched archive is linked in:"
info "  idf.py -C c5_node build   (ninja relinks on the changed .a; fullclean not required)"
