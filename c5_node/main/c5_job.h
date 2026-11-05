/*
 * c5_job.h — Tier 2 prep (phase1-infra): shared sniffer-job scaffold.
 *
 * The four Tier 1 passive sniffers (probe_sniffer.c, deauth_detect.c,
 * clients_hunt.c, spectrum.c) all share one lifecycle, cloned from
 * hs_capture.c:
 *
 *   ctx = { requester[6], seq, channel/target, duration, volatile stop }
 *   ISR promiscuous_cb: parse + bounds-check, xQueueSendFromISR(POD)   (IRAM-safe)
 *   drain_task: pop queue, (dedup), set ch1, esp_now_send, restore ch  (task ctx)
 *   task: g_pause_hello=true, set filter+cb+promiscuous, hop/lock loop
 *         until stop/deadline, then teardown (promisc off, cb NULL, ch1,
 *         s_ctx=NULL, g_pause_hello=false, free ctx)
 *   start(): restart-race guard — stop old, unregister cb, delay, then spawn
 *   stop():  s_ctx->stop = true
 *
 * Tier 2's KARMA and APCLONE reuse this lifecycle; BEACON_SPAM reuses the
 * task/stop pattern. The intended Tier 2 consolidation is to lift
 * job_begin(filter, channel, hop, on_frame) / job_end() / the drain plumbing
 * out of these four files into a c5_job.c. That extraction is deferred to the
 * Tier 2 feature work because it must be proven behaviour-neutral by
 * re-running the Tier 1 hardware acceptance tests (§5 of the handoff), which
 * can't be done from a build host. Until then this header carries the one
 * piece that is safe to share now: the run-length safety watchdog.
 */
#pragma once

#include <stdint.h>

/* Maximum any single C5 job may run, regardless of the requested duration.
 * The wire request's duration_ms is uint16_t (so ~65 s max over the wire);
 * this firmware-side cap is uint32_t because the "0 = until STOP" features
 * (SPECTRUM now; KARMA / APCLONE in Tier 2) must still auto-stop. 5 minutes
 * matches the safety cap those Tier 2 request structs already imply. */
#define JOB_SAFETY_CAP_MS 300000u   /* 5 minutes */

/* Resolve a request's duration_ms into an effective run length in ms:
 *   requested 0      -> default_ms (use JOB_SAFETY_CAP_MS for until-STOP jobs)
 *   otherwise        -> requested, clamped down to JOB_SAFETY_CAP_MS.
 * Returned as uint32_t so a deadline survives the uint16_t wire limit. */
static inline uint32_t job_effective_ms(uint16_t requested_ms, uint32_t default_ms)
{
    uint32_t ms = requested_ms ? (uint32_t)requested_ms : default_ms;
    if (ms > JOB_SAFETY_CAP_MS) ms = JOB_SAFETY_CAP_MS;
    return ms;
}
