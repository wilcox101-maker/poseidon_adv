/*
 * beacon_spam.c — Tier 2 BEACON_SPAM (CMD 20) for the C5.
 *
 * Floods fake OPEN beacons so every nearby scanner's network list fills with
 * our SSIDs. The C5's headline over the S3/C6: it can do this on 5 GHz
 * channels too, not just 2.4 GHz.
 *
 * Request: posei_beacon_spam_req_t { channel; duration_ms; mode; ssid_n; ssids[5][33] }
 *   mode 0 = built-in meme SSID list (firmware strings below)
 *   mode 1 = "rickroll" themed loop (meme references, NOT song lyrics —
 *            reproducing lyrics is a copyright no-go, so these are original
 *            joke strings that evoke the meme)
 *   mode 2 = caller-supplied SSIDs (ssid_n, up to 5, from ssids[])
 *   channel: 36..165 for 5 GHz, 1..13 for 2.4 GHz
 *   duration_ms: 0 = run until STOP (clamped to the 5-min safety cap)
 *
 * TX-only, like wifi_attacker.c: promiscuous(true) + set channel once, then
 * loop esp_wifi_80211_tx(). Beacon subtype (0x80) is not dropped by the IDF
 * raw-frame sanity check, so this works on an unpatched toolchain. Streams a
 * RESP_STATUS { frames_sent:u32, channel:u8 } every ~250 ms, hopping to ch 1
 * to send it (POSEIDON RXes ESP-NOW on ch 1) exactly as the deauth path does.
 */
#include "proto.h"
#include "beacon_tx.h"
#include "c5_job.h"
#include "led_fx.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "beacon";

extern volatile bool g_pause_hello;

/* Built-in meme SSIDs (mode 0). Original joke strings / puns only — no song
 * lyrics or other copyrighted text. */
static const char *MEME_SSIDS[] = {
    "FBI Surveillance Van",
    "Pretty Fly for a WiFi",
    "Drop It Like Its Hotspot",
    "The LAN Before Time",
    "Bill Wi the Science Fi",
    "Loading...",
    "Hide Yo Kids Hide Yo WiFi",
    "Get Off My LAN",
    "Definitely Not a Honeypot",
    "Area 51 Guest",
};
#define MEME_N (sizeof(MEME_SSIDS) / sizeof(MEME_SSIDS[0]))

/* Mode 1 "rickroll" set. Deliberately NOT the song's lyrics (copyright) —
 * original meme-reference strings only. */
static const char *RICK_SSIDS[] = {
    "Never Gonna Give You WiFi",
    "You Just Got Rickrolled",
    "Free Rick Astley Tickets",
    "RA Approved Network",
    "Click Here For Free WiFi",
};
#define RICK_N (sizeof(RICK_SSIDS) / sizeof(RICK_SSIDS[0]))

struct bspam_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  mode;
    uint8_t  ssid_n;                 /* mode 2 only */
    char     ssids[5][33];           /* mode 2 only */
    volatile bool stop;
};
static struct bspam_ctx_t * volatile s_ctx = NULL;
static volatile bool s_running = false;

/* Resolve the active SSID set for this run into local pointers. For modes 0/1
 * these point at the const tables; for mode 2 they point into ctx->ssids. */
static int resolve_ssids(const struct bspam_ctx_t *ctx, const char **out, int max)
{
    if (ctx->mode == 2) {
        int n = ctx->ssid_n; if (n > 5) n = 5; if (n > max) n = max;
        for (int i = 0; i < n; ++i) out[i] = ctx->ssids[i];
        return n;
    }
    const char **tbl = (ctx->mode == 1) ? RICK_SSIDS : MEME_SSIDS;
    int n = (ctx->mode == 1) ? (int)RICK_N : (int)MEME_N;
    if (n > max) n = max;
    for (int i = 0; i < n; ++i) out[i] = tbl[i];
    return n;
}

/* RESP_STATUS { frames_sent:u32, channel:u8 } — identical shape to the deauth
 * path's status frame, so the S3's c5_status_frames()/c5_status_channel()
 * read it unchanged. Hop to ch 1 to send (POSEIDON RXes there), then back. */
static void send_status(const uint8_t *requester, uint16_t seq,
                        uint32_t frames_sent, uint8_t ch)
{
    posei_msg_t m;
    proto_init_msg(&m, POSEI_TYPE_RESP_STATUS);
    m.seq = seq;
    memcpy(m.payload, &frames_sent, 4);
    m.payload[4] = ch;
    m.payload_len = 5;

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    proto_send_to(requester, &m);
    vTaskDelay(pdMS_TO_TICKS(2));
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

static void bspam_task(void *arg)
{
    struct bspam_ctx_t *ctx = (struct bspam_ctx_t *)arg;

    /* Restart-race guard: if a previous run is still tearing down, wait for it
     * to clear s_running before we claim the radio (TX-only, so there's no RX
     * callback to unregister as a sync point — poll with a bounded timeout). */
    for (int i = 0; i < 30 && s_running; ++i) vTaskDelay(pdMS_TO_TICKS(10));
    s_ctx = ctx;
    s_running = true;

    const char *ssids[5];
    int n = resolve_ssids(ctx, ssids, 5);
    if (n <= 0) {
        ESP_LOGW(TAG, "no SSIDs to spam (mode=%u ssid_n=%u)", ctx->mode, ctx->ssid_n);
        send_status(ctx->requester, ctx->seq, 0, ctx->channel);
        goto done;
    }

    led_fx_set(LED_MODE_ATTACK);
    g_pause_hello = true;

    ESP_LOGI(TAG, "beacon spam ch=%u mode=%u ssids=%d dur=%ums",
             ctx->channel, ctx->mode, n, ctx->duration_ms);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ctx->channel, WIFI_SECOND_CHAN_NONE);

    uint32_t end  = xTaskGetTickCount() +
                    pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, JOB_SAFETY_CAP_MS));
    uint32_t sent = 0;
    uint32_t last_status = 0;
    uint16_t seqctr = 0;
    uint8_t  frame[C5_BEACON_FRAME_MAX];

    while (!ctx->stop && xTaskGetTickCount() < end) {
        for (int i = 0; i < n && !ctx->stop; ++i) {
            uint8_t bssid[6];
            uint8_t slen = (uint8_t)strnlen(ssids[i], 32);
            c5_ssid_bssid(ssids[i], slen, bssid);
            int len = c5_build_beacon(frame, bssid, ssids[i], slen, ctx->channel, seqctr++);
            esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
            sent++;
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        if (xTaskGetTickCount() - last_status > pdMS_TO_TICKS(250)) {
            send_status(ctx->requester, ctx->seq, sent, ctx->channel);
            last_status = xTaskGetTickCount();
        }
    }
    send_status(ctx->requester, ctx->seq, sent, ctx->channel);

    esp_wifi_set_promiscuous(false);

done:
    /* Only restore shared radio state if we're still the current run (a newer
     * beacon_spam_start() taking over owns the radio and restores it itself —
     * same ownership gate as wifi_attacker.c POS-AUDIT-119b). */
    if (s_ctx == ctx) {
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        led_fx_set(LED_MODE_IDLE);
        g_pause_hello = false;
        s_ctx = NULL;
    }
    s_running = false;
    free(ctx);
    vTaskDelete(NULL);
}

void beacon_spam_start(const uint8_t requester[6],
                       const posei_beacon_spam_req_t *req,
                       uint16_t seq)
{
    if (s_ctx) s_ctx->stop = true;   /* signal predecessor; task waits on s_running */

    struct bspam_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { led_fx_set(LED_MODE_ERROR); return; }
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->mode        = req->mode;
    ctx->ssid_n      = req->ssid_n;
    memcpy(ctx->ssids, req->ssids, sizeof(ctx->ssids));
    ctx->stop        = false;

    if (xTaskCreate(bspam_task, "bspam", 4096, ctx, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        free(ctx);
        led_fx_set(LED_MODE_ERROR);
    }
}

void beacon_spam_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
