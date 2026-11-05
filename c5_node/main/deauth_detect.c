/*
 * deauth_detect.c — T1.2 passive deauth/disassoc detector for the C5.
 *
 * Same sniffer skeleton as probe_sniffer.c (ISR→queue→drain, restart-race
 * guard, g_pause_hello, restore-to-ch1). Emits one RESP_DEAUTH_HIT per
 * observed deauth (subtype 0xC0) or disassoc (0xA0) management frame. No
 * dedup — a storm is exactly what the operator wants to see.
 *
 * Request: posei_deauth_detect_req_t { channel; duration_ms }.
 *   channel 0 → hop 1..13; else lock.
 */
#include "proto.h"
#include "c5_job.h"
#include "led_fx.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "deauthd";

extern volatile bool g_pause_hello;

struct dd_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;
    uint16_t duration_ms;
    volatile bool stop;
};
static struct dd_ctx_t *s_ctx = NULL;
static volatile uint8_t s_cur_ch = 1;

struct dd_hit_t { posei_deauth_hit_t rec; };
static QueueHandle_t s_q;
static TaskHandle_t  s_drain;
#define DD_Q_DEPTH 16

static void drain_task(void *_)
{
    struct dd_hit_t h;
    while (1) {
        if (xQueueReceive(s_q, &h, portMAX_DELAY) != pdTRUE) continue;
        struct dd_ctx_t *ctx = s_ctx;
        if (!ctx || ctx->stop) continue;

        posei_msg_t msg;
        proto_init_msg(&msg, POSEI_TYPE_RESP_DEAUTH_HIT);
        msg.seq = ctx->seq;
        memcpy(msg.payload, &h.rec, sizeof(h.rec));
        msg.payload_len = sizeof(h.rec);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        proto_send_to(ctx->requester, &msg);
        esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
        led_fx_set(LED_MODE_ZB_RX);
    }
}

static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!s_ctx || s_ctx->stop) return;
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 26) return;
    if ((p[0] & 0x0C) != 0x00) return;      /* not a management frame */
    uint8_t subtype = p[0] & 0xF0;
    if (subtype != 0xC0 && subtype != 0xA0) return;  /* deauth / disassoc */

    struct dd_hit_t h;
    memset(&h, 0, sizeof(h));
    memcpy(h.rec.dst, &p[4],  6);           /* addr1 = destination */
    memcpy(h.rec.src, &p[10], 6);           /* addr2 = source      */
    h.rec.channel = s_cur_ch;
    /* Reason code = first byte of the fixed body after the 24-byte MGMT
     * header (the uint8 field is enough). */
    h.rec.reason  = p[24];

    BaseType_t woken = pdFALSE;
    if (s_q) xQueueSendFromISR(s_q, &h, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static void dd_task(void *arg)
{
    struct dd_ctx_t *ctx = (struct dd_ctx_t *)arg;
    s_ctx = ctx;

    if (!s_q) s_q = xQueueCreate(DD_Q_DEPTH, sizeof(struct dd_hit_t));
    if (!s_drain && s_q) xTaskCreate(drain_task, "deauthd_drain", 4096, NULL, 4, &s_drain);

    led_fx_set(LED_MODE_SCAN);
    g_pause_hello = true;

    bool hop = (ctx->channel == 0);
    s_cur_ch = hop ? 1 : ctx->channel;

    ESP_LOGI(TAG, "deauth detect ch=%u(%s) dur=%ums",
             s_cur_ch, hop ? "hop" : "lock", ctx->duration_ms);

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_promiscuous(true);

    uint32_t end = xTaskGetTickCount() + pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, 20000));
    uint8_t hop_ch = 1;
    while (!ctx->stop && xTaskGetTickCount() < end) {
        if (hop) {
            hop_ch = (hop_ch >= 13) ? 1 : hop_ch + 1;
            s_cur_ch = hop_ch;
        }
        esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    s_ctx = NULL;
    led_fx_set(LED_MODE_IDLE);
    g_pause_hello = false;

    free(ctx);
    vTaskDelete(NULL);
}

void deauth_detect_start(const uint8_t requester[6],
                         const posei_deauth_detect_req_t *req,
                         uint16_t seq)
{
    if (s_ctx) {
        s_ctx->stop = true;
        esp_wifi_set_promiscuous_rx_cb(NULL);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    struct dd_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->stop        = false;

    xTaskCreate(dd_task, "deauthd", 4096, ctx, 4, NULL);
}

void deauth_detect_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
