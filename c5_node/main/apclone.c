/*
 * apclone.c — Tier 2 APCLONE (CMD 24) for the C5.
 *
 * Stands up a SoftAP with an operator-supplied SSID (open or WPA2) so a target
 * will associate to our radio instead of the real AP — the manual building
 * block KARMA automates. Uses the shared softap.c bring-up.
 *
 * Request: posei_apclone_req_t { channel; ssid[33]; open; pass[33] }
 *   open=1 → open auth; open=0 → WPA2 with `pass` (8..63 chars)
 *   runs until CMD_STOP, capped at JOB_SAFETY_CAP_MS (5 min)
 *
 * Status: one RESP_STATUS at start (on ch 1, before the AP comes up) and one
 * at teardown. A live heartbeat is sent each second ONLY when the AP channel
 * is 1 — off-channel the ESP-NOW link to POSEIDON (ch 1) is unreachable while
 * the AP holds the radio (see softap.h's control-link caveat). frames_sent
 * carries the number of associated stations.
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

static const char *TAG = "apclone";

extern volatile bool g_pause_hello;

struct apc_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;
    char     ssid[33];
    uint8_t  open;
    char     pass[33];
    volatile bool stop;
};
static struct apc_ctx_t * volatile s_ctx = NULL;
static volatile bool s_running = false;

/* frames_sent field carries the associated-station count for AP features. */
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

static void apc_task(void *arg)
{
    struct apc_ctx_t *ctx = (struct apc_ctx_t *)arg;

    for (int i = 0; i < 30 && s_running; ++i) vTaskDelay(pdMS_TO_TICKS(10));
    s_ctx = ctx;
    s_running = true;

    led_fx_set(LED_MODE_ATTACK);
    g_pause_hello = true;

    /* Start status while still on ch 1 so POSEIDON hears the ack. */
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    send_status(ctx->requester, ctx->seq, ctx->channel);

    ESP_LOGI(TAG, "apclone '%s' ch=%u %s", ctx->ssid, ctx->channel,
             ctx->open ? "OPEN" : "WPA2");

    esp_err_t err = softap_up(ctx->ssid, ctx->open, ctx->pass, ctx->channel);
    if (err != ESP_OK) {
        led_fx_set(LED_MODE_ERROR);
        softap_down();
        goto done;
    }

    /* posei_apclone_req_t carries no duration_ms — APCLONE runs until CMD_STOP,
     * so the safety cap IS the deadline. */
    uint32_t end = xTaskGetTickCount() + pdMS_TO_TICKS(JOB_SAFETY_CAP_MS);

    while (!ctx->stop && xTaskGetTickCount() < end) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        /* Heartbeat only when reachable (AP on ch 1). */
        if (ctx->channel == 1 && !ctx->stop)
            send_status(ctx->requester, ctx->seq, ctx->channel);
    }

    softap_down();

done:
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    send_status(ctx->requester, ctx->seq, ctx->channel);
    if (s_ctx == ctx) {
        led_fx_set(LED_MODE_IDLE);
        g_pause_hello = false;
        s_ctx = NULL;
    }
    s_running = false;
    free(ctx);
    vTaskDelete(NULL);
}

void apclone_start(const uint8_t requester[6], const posei_apclone_req_t *req, uint16_t seq)
{
    if (s_ctx) s_ctx->stop = true;

    struct apc_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { led_fx_set(LED_MODE_ERROR); return; }
    memcpy(ctx->requester, requester, 6);
    ctx->seq     = seq;
    ctx->channel = req->channel;
    memcpy(ctx->ssid, req->ssid, 32); ctx->ssid[32] = '\0';
    ctx->open    = req->open;
    memcpy(ctx->pass, req->pass, 32); ctx->pass[32] = '\0';
    ctx->stop    = false;

    if (xTaskCreate(apc_task, "apclone", 4096, ctx, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        free(ctx);
        led_fx_set(LED_MODE_ERROR);
    }
}

void apclone_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
