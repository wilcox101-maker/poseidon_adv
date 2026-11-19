/*
 * tx_arm — master transmit-arm switch.
 *
 * A single global safety gate for every high-risk transmitting feature
 * (deauth, jammer, beacon spam, evil-twin, BLE spam/flood, sub-GHz TX,
 * nRF24 jam/inject, C5 TX commands). Default is SAFE. IR TX is NOT gated
 * yet — see docs/tx-arm.md "Known gaps". A
 * feature that actually puts energy on the air must call
 * tx_arm_require() at its transmit entry point and bail if it returns
 * false. Passive features (scan, sniff, detect, spectrum) never call it.
 *
 * Rationale (UI review finding #1): the menu fires a letter mnemonic
 * immediately, so a mis-key could launch a TX feature. With the switch
 * SAFE, the feature still opens but transmits nothing until the operator
 * deliberately arms it.
 *
 * State persists across reboots (NVS namespace "ptx", key "armed"): the
 * device powers up in whatever state it was left. To make it reset to
 * SAFE on every boot instead, force false in tx_armed()'s first load.
 */
#pragma once

#include <Arduino.h>

/* Current arm state. Loads from NVS on first call. */
bool tx_armed(void);

/* Set and persist the arm state. */
void tx_arm_set(bool on);

/* Transmit gate. Returns true when armed. When SAFE, shows a brief
 * notice and returns false — the caller must not transmit. Call this at
 * the point a feature is about to put a frame on the air, not merely on
 * screen entry, so passive setup and UI still work while disarmed. */
bool tx_arm_require(void);
