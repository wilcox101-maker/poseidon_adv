/*
 * radio.cpp — lazy domain switcher.
 */
#include "radio.h"
#include "lora_hw.h"
#include "cc1101_hw.h"
#include "nrf24_hw.h"
#include "gps.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_bt.h>
#include <NimBLEDevice.h>
#include <string.h>

static radio_domain_t s_active = RADIO_NONE;

static void destroy_default_netifs(void)
{
    esp_netif_t *matches[4] = {};
    int count = 0;
    for (esp_netif_t *it = esp_netif_next_unsafe(nullptr);
         it && count < 4; it = esp_netif_next_unsafe(it)) {
        const char *key = esp_netif_get_ifkey(it);
        if (key && (strcmp(key, "WIFI_STA_DEF") == 0 ||
                    strcmp(key, "WIFI_AP_DEF") == 0)) {
            matches[count++] = it;
        }
    }
    for (int i = 0; i < count; ++i)
        esp_netif_destroy_default_wifi(matches[i]);
}

static esp_netif_t *find_default_netif(const char *wanted)
{
    for (esp_netif_t *it = esp_netif_next_unsafe(nullptr);
         it; it = esp_netif_next_unsafe(it)) {
        const char *key = esp_netif_get_ifkey(it);
        if (key && strcmp(key, wanted) == 0) return it;
    }
    return nullptr;
}

void wifi_release_driver(void)
{
    wifi_mode_t cur = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&cur) == ESP_OK) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_disconnect();
        esp_wifi_stop();
        delay(50);
        esp_wifi_deinit();
    }

    /* Netifs can survive a partially failed or already-deinitialized WiFi
     * driver; never let the mode probe suppress this cleanup. */
    destroy_default_netifs();
}

void wifi_force_clean_sta(void)
{
    /* Probe whether WiFi was ever inited this session. esp_wifi_get_mode
     * returns ESP_ERR_WIFI_NOT_INIT until esp_wifi_init has run; in
     * that case there's nothing to reset and the subsequent IDF calls
     * would themselves return errors and leave the driver in a
     * half-state that breaks Arduino's later WiFi.mode() init. */
    wifi_mode_t cur = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&cur) != ESP_OK) return;
    esp_wifi_set_promiscuous(false);
    esp_wifi_disconnect();
    esp_wifi_set_mode(WIFI_MODE_STA);
    delay(30);
}

bool wifi_lean_sta_init(void)
{
    /* If WiFi is already inited (we got here from another feature in
     * the same session), just ensure mode is STA + started.
     * esp_wifi_get_mode returns ESP_OK only after esp_wifi_init has run.
     *
     * On-device repro fix 2026-06-06: after POS-AUDIT-008 the
     * teardown(RADIO_WIFI) leaves the driver stopped-but-inited.
     * Previous code here just returned true on the inited check —
     * subsequent esp_wifi_set_promiscuous / esp_wifi_set_channel /
     * esp_wifi_80211_tx then all failed with ESP_ERR_WIFI_NOT_STARTED
     * and the WiFi task's coex path eventually panicked, freezing the
     * device and forcing a reset. Symptom seen as "Deauth All freezes
     * and restarts". Now we explicitly esp_wifi_start() — a second
     * start on an already-started driver returns ESP_ERR_WIFI_STATE
     * (harmless), but covers the stopped-but-inited case which is the
     * only one POS-AUDIT-008 introduced. */
    wifi_mode_t cur = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&cur) == ESP_OK) {
        if (cur != WIFI_MODE_STA) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_disconnect();
            esp_wifi_set_mode(WIFI_MODE_STA);
            delay(30);
        }
        (void)esp_wifi_start();
        return true;
    }
    /* Fresh init — raw IDF with shrunk buffers to fit in fragmented
     * DMA RAM (M5GFX framebuffer holds ~60 KB at boot, leaves no
     * room for default 32-buffer Arduino init). */
    esp_netif_init();
    esp_event_loop_create_default();
    /* Reuse an enumerated default STA when the Arduino/IDF registry still
     * owns one but the direct ifkey lookup missed it. Creating a second
     * default STA is an assert, so only create one when none exists. */
    esp_netif_t *existing_sta = find_default_netif("WIFI_STA_DEF");
    if (!existing_sta) {
        destroy_default_netifs();
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();
    } else {
        esp_netif_t *stale_ap = find_default_netif("WIFI_AP_DEF");
        if (stale_ap) esp_netif_destroy_default_wifi(stale_ap);
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* TIGHT buffer config. Cardputer-Adv has ~52 KB DMA-capable RAM
     * after M5GFX framebuffer at boot. WiFi init grabs most of it for
     * its tx_buf + rx_buf pools (each ~1.7 KB). At 8/8 we hit ~14 KB
     * of TX + ~14 KB of RX = 28 KB consumed, leaving ~1 KB DMA free
     * for runtime allocations — raw 802.11 TX then OOMs after one
     * frame. 4/4 leaves ~14 KB DMA-free which is plenty for sustained
     * raw TX bursts with the 2 ms inter-frame delay. Sacrifice: brief
     * RX bursts may drop occasional frames (capture rate ~halved at
     * very high traffic), but TX reliability is far more important. */
    cfg.static_tx_buf_num  = 0;
    cfg.dynamic_tx_buf_num = 4;
    cfg.tx_buf_type        = 1;
    cfg.cache_tx_buf_num   = 4;
    cfg.static_rx_buf_num  = 4;
    cfg.dynamic_rx_buf_num = 4;
    cfg.ampdu_tx_enable    = 0;
    cfg.ampdu_rx_enable    = 0;
    cfg.amsdu_tx_enable    = 0;
    esp_err_t ie = esp_wifi_init(&cfg);
    if (ie != ESP_OK) return false;
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_err_t se = esp_wifi_start();
    if (se != ESP_OK) return false;
    /* #A5 / 2026-06-09: bump TX power once at session entry — 84 is
     * the driver max in 0.25 dBm units (21 dBm ~126 mW). Per the
     * wifi_probe.cpp comment, setting this per-burst destabilises the
     * driver; setting it once after esp_wifi_start is the supported
     * pattern. Benefits every STA-mode feature that goes through
     * wifi_lean_sta_init: Deauth All, Scan, Probe, PMKID, Beacon
     * Spam, Wardrive. */
    esp_wifi_set_max_tx_power(84);
    return true;
}

const char *radio_name(void)
{
    switch (s_active) {
    case RADIO_WIFI:   return "wifi";
    case RADIO_BLE:    return "ble";
    case RADIO_LORA:   return "lora";
    case RADIO_SUBGHZ: return "subghz";
    case RADIO_NRF24:  return "nrf24";
    default:           return "idle";
    }
}

radio_domain_t radio_current(void) { return s_active; }

static void teardown_current(void)
{
    switch (s_active) {
    case RADIO_WIFI:
        /* POS-AUDIT-008: PORKCHOP pattern. WiFi.mode(WIFI_OFF) is BANNED
         * — combined with esp_wifi_deinit, repeated session churn
         * fragments the heap and the next esp_wifi_init returns
         * ESP_ERR_NO_MEM (257), eventually deadlocking the driver.
         * esp_wifi_stop alone leaves the driver structures resident so
         * the next feature's WiFi.mode(STA/AP) + start is clean off a
         * hot driver. Heap-aware deinit gate (only deinit when heap is
         * healthy AND largest-free > kMinHeapForTls) lands with
         * POS-AUDIT-118. WiFi.disconnect(false,false) just drops the
         * association without driver-state poke. */
        WiFi.disconnect(false, false);
        esp_wifi_stop();
        break;
    case RADIO_BLE:
        /* Only deinit if NimBLE is actually initialized — features that
         * explicitly deinit on exit (ble_hid) leave a dangling state
         * flag, and a second deinit crashes on some NimBLE builds. */
        if (NimBLEDevice::isInitialized()) NimBLEDevice::deinit(true);
        break;
    case RADIO_LORA:
        if (lora_is_up()) lora_end();
        break;
    case RADIO_SUBGHZ:
        cc1101_end();
        /* GPIO 13 is Hydra CC1101 CS while sub-GHz is up, and GPS UART TX
         * on the LoRa-GNSS hat otherwise. cc1101_end left CS held high.
         * Hand the pad back only when the user opted into GPS — starting
         * the UART with GPS off would wiggle the CC1101 chip-select. */
        if (gps_user_enabled()) {
            pinMode(CC1101_CS, INPUT);
            gps_begin();
        }
        break;
    case RADIO_NRF24:
        nrf24_end();
        /* nrf24_park_others() holds CC1101 CS (GPIO 13) high for the
         * whole nRF24 session. Same restore rule as sub-GHz. */
        if (gps_user_enabled()) {
            pinMode(CC1101_CS, INPUT);
            gps_begin();
        }
        break;
    default: break;
    }
    s_active = RADIO_NONE;
    delay(100);
}

bool radio_switch(radio_domain_t target)
{
    if (target == s_active) return true;
    radio_domain_t previous = s_active;
    teardown_current();
    if (target == RADIO_NONE) return true;

    /* BLE needs roughly 60 KB of internal heap for controller + NimBLE
     * startup. Wi-Fi normally stays stopped-but-initialized to avoid
     * fragmentation between Wi-Fi features, but that retained driver state
     * makes a Wi-Fi -> BLE transition fail with ESP_ERR_NO_MEM. Release the
     * Wi-Fi driver only for this cross-domain transition. */
    if (target == RADIO_BLE && previous == RADIO_WIFI) {
        wifi_release_driver();
    }

    switch (target) {
    case RADIO_WIFI:
        /* State-only switch — don't touch WiFi here. Calling WiFi.mode(WIFI_STA)
         * left the driver in a half-init state ("STA not started!" warning
         * on disconnect), then any later WiFi.mode(WIFI_AP) crashed in
         * ieee80211_hostap_attach (+0x2c null deref) because the AP-side
         * driver structures were never allocated. Features now bring up
         * WiFi themselves from a clean state (WiFi.mode(WIFI_STA) for
         * scan/wardrive, WiFi.mode(WIFI_AP) for portal/spam). Country
         * code is still applied lazily on first esp_wifi_init by the
         * driver default, and esp_wifi_set_country can be called by
         * features that hop ch12-14. */
        break;
    case RADIO_BLE:
        Serial.printf("[radio] enter RADIO_BLE setup. bt_ctrl_status=%d\n",
                      (int)esp_bt_controller_get_status());
        Serial.flush();
        if (!NimBLEDevice::isInitialized()) {
            Serial.println("[radio] NimBLEDevice::init() begin"); Serial.flush();
            bool ok = NimBLEDevice::init("");
            Serial.printf("[radio] NimBLEDevice::init() -> %d bt_ctrl_status=%d\n",
                          (int)ok, (int)esp_bt_controller_get_status());
            Serial.flush();
            if (!ok) {
                /* NimBLEDevice::init() can fail AFTER esp_bt_controller_init()
                 * already succeeded (bt_ctrl_status left at INITED, not IDLE).
                 * NimBLEDevice::isInitialized() stays false in that case, so
                 * the normal teardown path above never runs and the
                 * controller's ~50-60 KB stays stuck resident for the rest of
                 * the boot -- starves the very next feature's WiFi/BLE init
                 * too. Force-release here regardless of NimBLE's own flag. */
                if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
                    esp_bt_controller_disable();
                    esp_bt_controller_deinit();
                }
                esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
                return false;
            }
        } else {
            Serial.println("[radio] NimBLE already initialized"); Serial.flush();
        }
        break;
    case RADIO_LORA:
        break;
    case RADIO_SUBGHZ:
        gps_end();  /* GPIO 13: GPS UART TX shares Hydra CC1101 CS */
        break;
    case RADIO_NRF24:
        gps_end();  /* parking CC1101 CS claims GPIO 13 (GPS UART TX) */
        break;
    default: break;
    }
    s_active = target;
    return true;
}
