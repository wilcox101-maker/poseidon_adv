/*
 * spectrum.c — T1.4 per-channel peak-RSSI sweep for the C5.
 *
 * NOT an RF spectrum analyzer: the ESP32 Wi-Fi driver exposes no true
 * energy-detect API here, so this measures the peak RSSI of *received
 * frames* per channel — a frame-activity heatmap. The S3 labels it as such.
 *
 * Unlike the other Tier 1 sniffers there is no per-frame emit: the ISR just
 * updates peak[current] = max(peak, rssi). The task sweeps a fixed working
 * set (<=24 channels, because the S3 keeps only the latest batch), dwelling
 * ~120 ms per channel, and after each full sweep packs ONE RESP_SPECTRUM
 * batch holding every channel, restores ch1, and emits. Repeat until the
 * duration elapses or CMD_STOP.
 *
 * Request: posei_spectrum_req_t { duration_ms } (0 = until STOP).
 */
#include "proto.h"
#include "c5_job.h"
#include "led_fx.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "spectrum";

extern volatile bool g_pause_hello;

/* Working set: 2.4 GHz 1..13 + a representative 5 GHz spread = 22 channels
 * (<=24 so it fits one batch / the S3's latest-snapshot slot). */
static const uint8_t SWEEP_CH[] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,
    36,40,44,48,149,153,157,161,165
};
#define SWEEP_N (sizeof(SWEEP_CH) / sizeof(SWEEP_CH[0]))

struct sp_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint16_t duration_ms;
    volatile bool stop;
};
static struct sp_ctx_t *s_ctx = NULL;

static volatile int   s_cur_idx = 0;        /* index into SWEEP_CH the cb attributes to */
static volatile int8_t s_peak[SWEEP_N];     /* max RSSI seen this sweep, per channel */

static void IRAM_ATTR promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)type;
    if (!s_ctx || s_ctx->stop) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    int8_t rssi = pkt->rx_ctrl.rssi;
    int idx = s_cur_idx;
    if (idx < 0 || idx >= (int)SWEEP_N) return;
    if (rssi > s_peak[idx]) s_peak[idx] = rssi;
}

static void sp_task(void *arg)
{
    struct sp_ctx_t *ctx = (struct sp_ctx_t *)arg;
    s_ctx = ctx;

    led_fx_set(LED_MODE_SCAN);
    g_pause_hello = true;

    ESP_LOGI(TAG, "spectrum sweep (%u ch) dur=%ums", (unsigned)SWEEP_N, ctx->duration_ms);

    esp_wifi_set_promiscuous(false);
    wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_promiscuous(true);

    /* 0 = "until STOP" still gets the firmware-side safety cap so a forgotten
     * sweep auto-stops (job_effective_ms maps 0 -> JOB_SAFETY_CAP_MS). */
    uint32_t end = xTaskGetTickCount() +
                   pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, JOB_SAFETY_CAP_MS));

    while (!ctx->stop && xTaskGetTickCount() < end) {
        /* Reset peaks for this sweep. */
        for (int i = 0; i < (int)SWEEP_N; ++i) s_peak[i] = -128;

        for (int i = 0; i < (int)SWEEP_N && !ctx->stop; ++i) {
            s_cur_idx = i;
            esp_wifi_set_channel(SWEEP_CH[i], WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(120));
        }
        if (ctx->stop) break;

        /* Pack one batch holding every channel and emit it. */
        posei_msg_t msg;
        proto_init_msg(&msg, POSEI_TYPE_RESP_SPECTRUM);
        msg.seq = ctx->seq;
        posei_spectrum_t *s = (posei_spectrum_t *)msg.payload;
        memset(s, 0, sizeof(*s));
        s->ch_count = (SWEEP_N > 24) ? 24 : (uint8_t)SWEEP_N;
        for (int i = 0; i < s->ch_count; ++i) {
            s->ch[i].ch        = SWEEP_CH[i];
            s->ch[i].peak_rssi = s_peak[i];
        }
        msg.payload_len = sizeof(posei_spectrum_t);
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);   /* ESP-NOW RX on ch1 */
        proto_send_to(ctx->requester, &msg);
        led_fx_set(LED_MODE_ZB_RX);
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

void spectrum_start(const uint8_t requester[6],
                    const posei_spectrum_req_t *req,
                    uint16_t seq)
{
    if (s_ctx) {
        s_ctx->stop = true;
        esp_wifi_set_promiscuous_rx_cb(NULL);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    struct sp_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->duration_ms = req->duration_ms;
    ctx->stop        = false;

    xTaskCreate(sp_task, "spectrum", 4096, ctx, 4, NULL);
}

void spectrum_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
