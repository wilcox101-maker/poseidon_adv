/*
 * softap.h — shared SoftAP honeypot bring-up for the C5 Tier 2 features.
 *
 * APCLONE (apclone.c) and KARMA (karma.c) both stand up a real SoftAP (not a
 * raw beacon loop — a client must be able to associate), differing only in
 * where the SSID comes from (operator-supplied vs harvested from a probe).
 * The netif/mode/config dance lives here so both share one audited path.
 *
 * The node boots in WIFI_MODE_STA for ESP-NOW; softap_up() switches it to
 * WIFI_MODE_APSTA so the STA side (and thus ESP-NOW to the S3) stays alive,
 * and softap_down() returns it to WIFI_MODE_STA. A default AP netif (with its
 * DHCP server) is created once, lazily, and kept for the session.
 *
 * CONTROL-LINK CAVEAT: while the AP is up on a non-1 channel the radio sits on
 * that channel, and POSEIDON transmits ESP-NOW on ch 1 — so a mid-run CMD_STOP
 * is only heard reliably when the AP channel is 1. Off-channel runs rely on the
 * requested duration and the 5-min JOB_SAFETY_CAP_MS backstop. This is the
 * channel-bound return-path constraint the project already documents, not a
 * bug in these handlers.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Bring up a SoftAP. open=true → WIFI_AUTH_OPEN (pass ignored); open=false →
 * WIFI_AUTH_WPA2_PSK with `pass` (must be 8..63 chars, else falls back to open
 * and logs). Returns the esp_wifi_set_config() result (ESP_OK on success). */
esp_err_t softap_up(const char *ssid, bool open, const char *pass, uint8_t channel);

/* Tear the SoftAP down (back to WIFI_MODE_STA). The AP netif is kept but idle.
 * Does not touch the channel — the caller restores ch 1 for ESP-NOW. */
void softap_down(void);
