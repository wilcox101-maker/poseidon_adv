/*
 * clients_hunt.c — T1.3 STA discovery for the C5 (CLIENTS_HUNT / CLIENTS_AP).
 *
 * Same sniffer skeleton as probe_sniffer.c but on DATA frames: lifts the
 * (bssid, sta) pair from the 802.11 header using the tods/fromds logic from
 * hs_capture.c and emits one RESP_STA per frame. A small (sta,bssid) dedup
 * ring in the drain cuts repeats; the S3 dedups too.
 *
 * Request: posei_clients_req_t { target_bssid[6]; channel; duration_ms; hop_all }.
 *   target_bssid all-zero → every AP; non-zero → only that BSSID.
 *   hop_all=1 → cycle 5 GHz UNII 36..165; =0 → lock `channel`.
 *
 * CLIENTS_HUNT and CLIENTS_AP are the same routine with different request
 * defaults (hop-all vs channel-locked single BSSID); main.c routes both here.
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

static const char *TAG = "clients";

extern volatile bool g_pause_hello;

/* 5 GHz UNII working set for hop_all. */
static const uint8_t UNII_CH[] = {
    36,40,44,48,52,56,60,64,
    100,104,108,112,116,120,124,128,132,136,140,144,
    149,153,157,161,165
};
#define UNII_N (sizeof(UNII_CH) / sizeof(UNII_CH[0]))

struct cl_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  target_bssid[6];
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  hop_all;
    bool     have_target;
    volatile bool stop;
};
static struct cl_ctx_t *s_ctx = NULL;
static volatile uint8_t s_cur_ch = 1;

struct cl_hit_t { posei_sta_t rec; };
static QueueHandle_t s_q;
static TaskHandle_t  s_drain;
#define CL_Q_DEPTH 16

struct cseen_t { uint8_t sta[6]; uint8_t bssid[6]; };
#define CSEEN_N 24
static struct cseen_t s_seen[CSEEN_N];
static int s_seen_n = 0;

static bool sta_seen(const posei_sta_t *r)
{
    for (int i = 0; i < s_seen_n; ++i) {
        if (memcmp(s_seen[i].sta, r->sta, 6) == 0 &&
            memcmp(s_seen[i].bssid, r->bssid, 6) == 0) return true;
    }
    if (s_seen_n < CSEEN_N) {
        memcpy(s_seen[s_seen_n].sta, r->sta, 6);
        memcpy(s_seen[s_seen_n].bssid, r->bssid, 6);
        s_seen_n++;
    }
    return false;
}

static void drain_task(void *_)
{
    struct cl_hit_t h;
    while (1) {
        if (xQueueReceive(s_q, &h, portMAX_DELAY) != pdTRUE) continue;
        struct cl_ctx_t *ctx = s_ctx;
        if (!ctx || ctx->stop) continue;
        if (sta_seen(&h.rec)) continue;

        posei_msg_t msg;
        proto_init_msg(&msg, POSEI_TYPE_RESP_STA);
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
    if (type != WIFI_PKT_DATA) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *p = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    if (len < 24) return;
    if ((p[0] & 0x0C) != 0x08) return;      /* not a data frame */

    uint8_t tods   = p[1] & 0x01;
    uint8_t fromds = p[1] & 0x02;
    const uint8_t *bssid, *sta;
    if (tods && !fromds)      { bssid = &p[4];  sta = &p[10]; }
    else if (!tods && fromds) { bssid = &p[10]; sta = &p[4];  }
    else return;                            /* skip WDS (4-addr) and IBSS */

    if (s_ctx->have_target && memcmp(bssid, s_ctx->target_bssid, 6) != 0) return;

    struct cl_hit_t h;
    memset(&h, 0, sizeof(h));
    memcpy(h.rec.sta, sta, 6);
    memcpy(h.rec.bssid, bssid, 6);
    h.rec.channel = s_cur_ch;
    h.rec.rssi    = pkt->rx_ctrl.rssi;

    BaseType_t woken = pdFALSE;
    if (s_q) xQueueSendFromISR(s_q, &h, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static void cl_task(void *arg)
{
    struct cl_ctx_t *ctx = (struct cl_ctx_t *)arg;
    s_ctx = ctx;
    s_seen_n = 0;

    if (!s_q) s_q = xQueueCreate(CL_Q_DEPTH, sizeof(struct cl_hit_t));
    if (!s_drain && s_q) xTaskCreate(drain_task, "clients_drain", 4096, NULL, 4, &s_drain);

    led_fx_set(LED_MODE_SCAN);
    g_pause_hello = true;

    s_cur_ch = ctx->hop_all ? UNII_CH[0] : (ctx->channel ? ctx->channel : 1);

    ESP_LOGI(TAG, "clients %s ch=%u dur=%ums target=%d",
             ctx->hop_all ? "hop5G" : "lock", s_cur_ch, ctx->duration_ms,
             ctx->have_target);

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_promiscuous(true);

    uint32_t end = xTaskGetTickCount() + pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, 20000));
    int hi = 0;
    while (!ctx->stop && xTaskGetTickCount() < end) {
        if (ctx->hop_all) {
            hi = (hi + 1) % UNII_N;
            s_cur_ch = UNII_CH[hi];
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

void clients_hunt_start(const uint8_t requester[6],
                        const posei_clients_req_t *req,
                        uint16_t seq)
{
    if (s_ctx) {
        s_ctx->stop = true;
        esp_wifi_set_promiscuous_rx_cb(NULL);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    struct cl_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    memcpy(ctx->target_bssid, req->target_bssid, 6);
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->hop_all     = req->hop_all;
    ctx->stop        = false;

    const uint8_t zero[6] = { 0 };
    ctx->have_target = (memcmp(req->target_bssid, zero, 6) != 0);

    xTaskCreate(cl_task, "clients", 4096, ctx, 4, NULL);
}

void clients_hunt_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
