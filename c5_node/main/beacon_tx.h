/*
 * beacon_tx.h — shared 802.11 beacon crafting for the C5 Tier 2 TX features.
 *
 * BEACON_SPAM (beacon_spam.c) and CIW (ciw.c) both put fake beacons on air;
 * they differ only in which SSID strings they advertise. The frame builder
 * and the per-SSID BSSID derivation live here so there is exactly ONE audited
 * beacon-crafting path on the node — the same philosophy as wifi_attacker.c's
 * wifi_tx_raw() for the deauth/injection path.
 *
 * Wire format is a straight port of src/features/wifi_probe.cpp's
 * build_open_beacon()/build_probe_resp() on the S3 (a known-good, on-hardware
 * tested layout): 24-byte MAC header, 12-byte fixed body (timestamp / beacon
 * interval / capability), then SSID + supported-rates + DS-param tags. OPEN
 * auth (Privacy=0) so passively-scanning clients list it without a saved PSK.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/* Largest frame c5_build_beacon() can emit: 36-byte header/fixed body +
 * (2 + 32) SSID + (2 + 8) rates + (2 + 1) DS = 83 bytes. Round up. */
#define C5_BEACON_FRAME_MAX 96

/* Derive a STABLE locally-administered BSSID from the SSID (FNV-1a), so each
 * fake network answers from one consistent MAC across beacon cycles — a radio
 * that claims many SSIDs from one MAC is exactly what client OSes flag. The
 * 0x02/0x53 prefix marks the frame as ours (locally administered, 'S'). */
void c5_ssid_bssid(const char *ssid, uint8_t ssid_len, uint8_t out[6]);

/* Build one OPEN-auth beacon for `ssid` on channel `ch` into `frame`
 * (must be >= C5_BEACON_FRAME_MAX). `seq` feeds the 802.11 sequence-control
 * field. ssid_len is clamped to 32. Returns the total frame length. */
int c5_build_beacon(uint8_t *frame, const uint8_t bssid[6],
                    const char *ssid, uint8_t ssid_len, uint8_t ch, uint16_t seq);
