/*
 * beacon_tx.c — shared beacon crafting (see beacon_tx.h).
 *
 * Pure frame building + BSSID derivation; no radio state is touched here.
 * The caller owns promiscuous mode, the channel, and esp_wifi_80211_tx()
 * (via wifi_attacker.c's wifi_tx_raw() or its own TX loop). Beacon subtype
 * (0x80) is NOT dropped by IDF's ieee80211_raw_frame_sanity_check, so this
 * path works on an UNPATCHED toolchain (unlike deauth/disassoc) — see the
 * NOTE in wifi_attacker.c.
 */
#include "beacon_tx.h"
#include "esp_timer.h"
#include <string.h>

void c5_ssid_bssid(const char *ssid, uint8_t ssid_len, uint8_t out[6])
{
    uint32_t h = 2166136261u;                 /* FNV-1a, matches the S3 side */
    for (uint8_t i = 0; i < ssid_len; ++i) { h ^= (uint8_t)ssid[i]; h *= 16777619u; }
    out[0] = 0x02;                            /* locally administered, unicast */
    out[1] = 0x53;                            /* 'S' marker so our fakes cluster */
    out[2] = (uint8_t)(h >> 0);
    out[3] = (uint8_t)(h >> 8);
    out[4] = (uint8_t)(h >> 16);
    out[5] = (uint8_t)(h >> 24);
}

int c5_build_beacon(uint8_t *frame, const uint8_t bssid[6],
                    const char *ssid, uint8_t ssid_len, uint8_t ch, uint16_t seq)
{
    static const uint8_t BCAST[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
    if (ssid_len > 32) ssid_len = 32;

    /* MAC header (24 B) */
    frame[0] = 0x80;  /* type=mgmt, subtype=beacon */
    frame[1] = 0x00;
    frame[2] = 0x00; frame[3] = 0x00;                 /* duration */
    memcpy(&frame[4],  BCAST, 6);                      /* DA = broadcast */
    memcpy(&frame[10], bssid, 6);                      /* SA = spoofed BSSID */
    memcpy(&frame[16], bssid, 6);                      /* BSSID */
    frame[22] = (uint8_t)((seq & 0xF) << 4);           /* fragment=0, seq lo */
    frame[23] = (uint8_t)((seq >> 4) & 0xFF);          /* seq hi */

    /* Fixed body (12 B): timestamp, beacon interval, capabilities. */
    uint64_t ts = (uint64_t)esp_timer_get_time();
    memcpy(&frame[24], &ts, 8);                        /* timestamp */
    frame[32] = 0x64; frame[33] = 0x00;                /* beacon interval 100 TU */
    frame[34] = 0x21; frame[35] = 0x00;                /* cap: ESS|ShortPreamble (Privacy=0) */

    /* Tagged params. */
    uint8_t *p = frame + 36;
    *p++ = 0x00; *p++ = ssid_len;                      /* SSID */
    if (ssid_len) { memcpy(p, ssid, ssid_len); p += ssid_len; }
    *p++ = 0x01; *p++ = 0x08;                          /* Supported rates */
    *p++ = 0x82; *p++ = 0x84; *p++ = 0x8B; *p++ = 0x96;
    *p++ = 0x24; *p++ = 0x30; *p++ = 0x48; *p++ = 0x6C;
    *p++ = 0x03; *p++ = 0x01; *p++ = ch;               /* DS Parameter Set */
    return (int)(p - frame);
}
