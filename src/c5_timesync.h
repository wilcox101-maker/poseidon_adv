/*
 * c5_timesync.h — pure decision logic for the Cardputer → node time-sync
 * (time-sync design, 2026-11-19). Host-testable: stdint/stdbool only, no
 * Arduino/ESP-NOW, so test/test_c5_timesync includes it directly.
 *
 * The Cardputer already seeds its own wall clock from GPS UTC (gps.cpp). A
 * detection node (GODZILLA on the RTC-less ES3C28P, or any RTC that drifted or
 * was never set) can only publish passively, so the Cardputer pushes it the
 * time as one additive downlink (CMD_TIMESET, c5_cmd.h / proto.h). Nodes HELLO
 * periodically, so the send is driven off HELLOs rather than a timer: on each
 * HELLO we ask "is this peer due a time-set?" — true on its first HELLO once
 * our clock is valid (covers a fresh pairing AND a node that joined before the
 * GPS fix), then again only every resync interval (covers soft-clock drift on
 * the RTC-less node without spending airtime on every HELLO).
 *
 * Passive: CMD_TIMESET is a command the node only RECEIVES; nothing here arms a
 * transmitter. The existing ESP-NOW command path already unicasts to peers.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Below this, time(nullptr) is boot-relative, not a real UTC clock. 2023-11-14
 * UTC — comfortably before this project and after any plausible boot epoch, so
 * it cleanly rejects an unset clock while accepting every real GPS fix. */
#define C5_TIME_VALID_MIN 1700000000u

/* Re-push the time to a peer at most this often (ms). 30 min bounds the
 * RTC-less soft-clock's drift while keeping the airtime cost one frame per peer
 * per half-hour. Driven off HELLO arrivals, so the real cadence is the slower
 * of this and the node's HELLO period. */
#define C5_TIMESET_RESYNC_MS (30u * 60u * 1000u)

/* A wall-clock epoch counts as real (settable onward to a node) only once it is
 * past the sentinel above. */
static inline bool c5_time_is_valid(uint32_t epoch)
{
    return epoch >= C5_TIME_VALID_MIN;
}

/* Should we send CMD_TIMESET to this peer now?
 *   clock_valid  — our own wall clock is a real UTC time (c5_time_is_valid)
 *   now_ms       — millis() now
 *   last_sent_ms — millis() when we last time-set this peer (any value if never)
 *   ever_sent    — have we ever time-set this peer?
 *   resync_ms    — minimum gap between re-pushes (C5_TIMESET_RESYNC_MS)
 * Never send without a valid clock; otherwise send on the first chance, then
 * once per resync window. The elapsed compare is unsigned so it is correct
 * across a millis() wrap. */
static inline bool c5_timeset_due(bool clock_valid, uint32_t now_ms,
                                  uint32_t last_sent_ms, bool ever_sent,
                                  uint32_t resync_ms)
{
    if (!clock_valid) return false;
    if (!ever_sent)   return true;
    return (uint32_t)(now_ms - last_sent_ms) >= resync_ms;
}
