/*
 * probe_sniffer.c — T1.1 passive probe-request logger for the C5.
 *
 * Clones the hs_capture.c lifecycle (context / ISR→queue→drain split /
 * restart-race guard / g_pause_hello / restore-to-ch1) and swaps the
 * capture body for a probe-request parse. One RESP_PROBE per probe.
 *
 * Request: posei_probe_sniff_req_t { channel; duration_ms }.
 *   channel 0 → hop 1..13 (2.4 GHz); else lock that channel.
 *
 * POS-AUDIT-005/006: the promiscuous RX callback runs in ISR / high-prio
 * context and must stay IRAM-safe — it only parses, bounds-checks, and
 * xQueueSendFromISR()s a small POD. esp_now_send / ESP_LOGx / led_fx_set
 * happen in the normal-priority drain task.
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

static const char *TAG = "probe";

extern volatile bool g_pause_hello;

struct probe_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;        /* 0 = hop 1..13, else lock */
    uint16_t duration_ms;
    volatile bool stop;
};
static struct probe_ctx_t *s_ctx = NULL;

/* Task sets on every hop (or once when locked); the ISR reads it so each
 * emitted probe carries the channel it was actually heard on. */
static volatile uint8_t s_cur_ch = 1;

struct probe_hit_t { posei_probe_t rec; };
static QueueHandle_t s_q;
static TaskHandle_t  s_drain;
#define PROBE_Q_DEPTH 16

/* Small (src,ssid) dedup ring in the drain to cut spam; the S3 dedups too. */
struct pseen_t { uint8_t src[6]; uint8_t ssid_len; char ssid[33]; };
#define PSEEN_N 16
static struct pseen_t s_seen[PSEEN_N];
static int s_seen_n = 0;

static bool probe_seen(const posei_probe_t *r)
{
    for (int i = 0; i < s_seen_n; ++i) {
        if (memcmp(s_seen[i].src, r->src, 6) == 0 &&
            s_seen[i].ssid_len == r->ssid_len &&
            memcmp(s_seen[i].ssid, r->ssid, r->ssid_len) == 0)
            return true;
    }
    if (s_seen_n < PSEEN_N) {
        memcpy(s_seen[s_seen_n].src, r->src, 6);
        s_seen[s_seen_n].ssid_len = r->ssid_len;
        memcpy(s_seen[s_seen_n].ssid, r->ssid, sizeof(r->ssid));
        s_seen_n++;
    }
    return false;
}

static void drain_task(void *_)
{
    struct probe_hit_t h;
    while (1) {
        if (xQueueReceive(s_q, &h, portMAX_DELAY) != pdTRUE) continue;
        struct probe_ctx_t *ctx = s_ctx;
        if (!ctx || ctx->stop) continue;          /* drop late frames */
        if (probe_seen(&h.rec)) continue;

        posei_msg_t msg;
        proto_init_msg(&msg, POSEI_TYPE_RESP_PROBE);
        msg.seq = ctx->seq;
        memcpy(msg.payload, &h.rec, sizeof(h.rec));
        msg.payload_len = sizeof(h.rec);
        /* Channel discipline: POSEIDON RXes ESP-NOW on ch1. */
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
        proto_send_to(ctx->requester, &msg);
        /* Restore the capture channel so a locked sniff doesn't get stuck
         * on ch1 between hits (the hop task re-sets it anyway). */
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
    if ((p[0] & 0xF0) != 0x40) return;      /* not a probe request */

    struct probe_hit_t h;
    memset(&h, 0, sizeof(h));
    memcpy(h.rec.src, &p[10], 6);           /* addr2 = source STA */
    h.rec.channel = s_cur_ch;
    h.rec.rssi    = pkt->rx_ctrl.rssi;
    /* SSID element sits right after the 24-byte MGMT header: id p[24]
     * (0 = SSID), len p[25], bytes &p[26]. Empty SSID = wildcard probe —
     * still emit it. */
    uint8_t slen = (p[24] == 0) ? p[25] : 0;
    if (slen > 32) slen = 32;
    if (len < 26 + slen) slen = 0;          /* truncated */
    h.rec.ssid_len = slen;
    if (slen) memcpy(h.rec.ssid, &p[26], slen);

    BaseType_t woken = pdFALSE;
    if (s_q) xQueueSendFromISR(s_q, &h, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static void probe_task(void *arg)
{
    struct probe_ctx_t *ctx = (struct probe_ctx_t *)arg;
    s_ctx = ctx;
    s_seen_n = 0;

    if (!s_q) s_q = xQueueCreate(PROBE_Q_DEPTH, sizeof(struct probe_hit_t));
    if (!s_drain && s_q) xTaskCreate(drain_task, "probe_drain", 4096, NULL, 4, &s_drain);

    led_fx_set(LED_MODE_SCAN);
    g_pause_hello = true;

    bool hop = (ctx->channel == 0);
    s_cur_ch = hop ? 1 : ctx->channel;

    ESP_LOGI(TAG, "probe sniff ch=%u(%s) dur=%ums",
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
            esp_wifi_set_channel(hop_ch, WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            /* Re-assert the locked channel so a drain-task ch1 excursion
             * (to send a reply) doesn't leave us parked off-target. */
            esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
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

void probe_sniffer_start(const uint8_t requester[6],
                         const posei_probe_sniff_req_t *req,
                         uint16_t seq)
{
    if (s_ctx) {
        /* Restart-race guard (mirrors hs_capture.c): unregister the RX
         * callback before retiring the old context, then delay. */
        s_ctx->stop = true;
        esp_wifi_set_promiscuous_rx_cb(NULL);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    struct probe_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->stop        = false;

    xTaskCreate(probe_task, "probe", 4096, ctx, 4, NULL);
}

void probe_sniffer_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
