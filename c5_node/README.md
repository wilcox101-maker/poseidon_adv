# POSEIDON C5 Node

Companion firmware for the ESP32-C5. The C5 is the only Espressif chip with:

- **5 GHz WiFi** (2.4 + 5, 802.11ax)
- **802.15.4** (Zigbee / Thread)

This firmware turns a C5 into a remote radio for POSEIDON (running on the Cardputer's S3). Communication happens over ESP-NOW — the C5 listens for command frames from POSEIDON, executes the requested scan or attack on its native radio, and streams results back.

## Protocol (over PigSync ESP-NOW)

```c
struct posei_msg_t {
    uint32_t magic;       // 0x504F5345 "POSE"
    uint8_t  version;     // 3
    uint8_t  type;         // see POSEI_TYPE_*
    uint16_t seq;
    uint8_t  payload[230];
    uint8_t  payload_len;
} __packed;
```

Types:

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 1    | HELLO            | both | node name, heap, gps |
| 10   | CMD_PING         | S3→C5 | — |
| 11   | CMD_SCAN_5G      | S3→C5 | duration_ms |
| 12   | CMD_SCAN_ZB      | S3→C5 | channel or 0xFF for hop |
| 13   | CMD_SCAN_2G      | S3→C5 | duration_ms |
| 14   | CMD_DEAUTH_5G    | S3→C5 | bssid + channel |
| 15   | CMD_STOP         | S3→C5 | — |
| 20   | RESP_PONG        | C5→S3 | — |
| 21   | RESP_AP_BATCH    | C5→S3 | count + array of APs |
| 22   | RESP_ZB_FRAME    | C5→S3 | pan id + src + dst |
| 23   | RESP_STATUS      | C5→S3 | last_cmd + ok flag |

## Building

Requires ESP-IDF v5.2 or newer (first version with C5 support). This firmware builds against v5.5.4.

**Required one-time patch — do this before the first build, and again after any ESP-IDF
reinstall/upgrade.** Stock `libnet80211.a` filters raw-TX management frames (deauth/disassoc,
subtypes 0xA/0xC) the same way it does on the Cardputer side — `esp_wifi_80211_tx()` returns
`ESP_OK` even when the blob drops the frame internally, so an unpatched build *looks* like deauth
is working (status counter climbs, no error anywhere) while nothing actually reaches the air. See
`tools/patch_libnet80211.sh` for the technical detail; it's a direct binary patch, not a config
flag, because the caller and the filter function live in the same object file inside the blob —
neither a linker `--wrap` nor `--allow-multiple-definition` can intercept it on this target.

```bash
. $IDF_PATH/export.sh
../tools/patch_libnet80211.sh        # patches $IDF_PATH's esp32c5 libnet80211.a in place; safe to re-run
cd c5_node
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Status

Shipped and hardware-verified (protocol **v3**). Implements HELLO/PING, 5 GHz
scan, **5 GHz deauth**, **PMKID capture**, and **Zigbee / 802.15.4 sniff**, all
driven from POSEIDON over ESP-NOW, plus 2.4 GHz scan and GPS passthrough.

Known limitation: full 4-way handshake capture over ESP-NOW is constrained by
the 230-byte payload (large EAPOL frames must be fragmented) — 5 GHz scan,
PMKID, and deauth are unaffected.
