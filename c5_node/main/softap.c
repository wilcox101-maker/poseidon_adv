/*
 * softap.c — shared SoftAP honeypot bring-up (see softap.h).
 */
#include "softap.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "softap";

/* Created once, kept for the session. esp_netif_create_default_wifi_ap() also
 * starts a DHCP server on the AP interface, so associating clients get an IP. */
static esp_netif_t *s_ap_netif = NULL;

esp_err_t softap_up(const char *ssid, bool open, const char *pass, uint8_t channel)
{
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "AP netif create failed");
            return ESP_FAIL;
        }
    }

    /* Keep STA alive so ESP-NOW to the S3 survives — APSTA, not AP. */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) { ESP_LOGE(TAG, "set_mode APSTA: %s", esp_err_to_name(err)); return err; }

    wifi_config_t ap = { 0 };
    size_t slen = strnlen(ssid, 32);
    memcpy(ap.ap.ssid, ssid, slen);
    ap.ap.ssid_len       = slen;
    ap.ap.channel        = channel;
    ap.ap.max_connection = 4;
    ap.ap.beacon_interval = 100;

    size_t plen = pass ? strnlen(pass, 63) : 0;
    if (!open && plen >= 8) {
        ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
        memcpy(ap.ap.password, pass, plen);
    } else {
        if (!open) ESP_LOGW(TAG, "pass too short (%u), falling back to OPEN", (unsigned)plen);
        ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config AP ch=%u: %s (5 GHz SoftAP may be unsupported here)",
                 channel, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SoftAP '%.*s' up ch=%u auth=%s",
             (int)slen, ssid, channel, open ? "OPEN" : "WPA2");
    return ESP_OK;
}

void softap_down(void)
{
    /* Dropping back to STA-only stops beaconing and disconnects clients; the
     * AP netif/DHCP server stay allocated but idle for the next run. */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) ESP_LOGW(TAG, "set_mode STA on teardown: %s", esp_err_to_name(err));
}
