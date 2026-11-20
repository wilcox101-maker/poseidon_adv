/*
 * gz_wifi_scan.cpp — GODZILLA passive Wi-Fi scan → batch → publish (task 3c).
 *
 * Mirrors the Cardputer surveillance hunter's promiscuous path
 * (src/features/surveillance_hunter.cpp): mgmt-only capture, classify each
 * frame with the shared src/flock_frame.h, key by address. Here the hit is
 * added to a spinlock-protected staging batch instead of logged.
 *
 * Producer/consumer split (as in 3b):
 *   - the promiscuous callback (Wi-Fi driver context) classifies and
 *     gz_wifi_batch_add()s — allocation-free, ISR-critical lock, never sends;
 *   - the Arduino loop hops the channel (~250 ms) and packs + broadcasts the
 *     batch (~1 s). gz_link_send() returns to the C5 link channel for the
 *     transmit and restores the dwell channel, so ESP-NOW reaches the S3 even
 *     while we hop. Trade-off: the ~1 s publish briefly leaves the current
 *     channel, dropping a few frames — acceptable for passive monitoring.
 *
 * flock_frame.h reports a tier-2 vendor prefix alone as a plain AP/STA by
 * design (see its header note), so those publish as DET_FAM_WIFI_AP/STA, not
 * FLOCK.
 */
#include "gz_wifi_scan.h"
#include "gz_ble_scan.h"   /* reuse the shared gz_publish_due() predicate */
#include "gz_batch.h"
#include "gz_link.h"
#include "gz_stats.h"
#include "gz_radar_store.h"  /* P2b: radar of what was published */
#include "gz_camera.h"       /* §4 unit 2: camera-maker Wi-Fi OUI table */
#include "gz_le_equip.h"     /* §4 unit 3: LE-equipment Wi-Fi OUI table */

#include <Arduino.h>
#include <esp_wifi.h>

#include "../../src/flock_frame.h"   /* read-only shared classifier (#52) */

static portMUX_TYPE     s_mux = portMUX_INITIALIZER_UNLOCKED;
static posei_wifi_det_t s_stage;
static volatile bool    s_flush_req   = false;
static uint16_t         s_seq         = 0;
static uint32_t         s_last_pub_ms = 0;
static uint32_t         s_last_hop_ms = 0;
static uint8_t          s_cur_ch      = GZ_WIFI_CH_MIN;

static void promisc_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;

    /* flock_frame_classify wants the length INCLUDING the 4-byte FCS, which is
     * exactly rx_ctrl.sig_len. Pure + allocation-free. */
    flock_frame_t fr;
    if (flock_frame_classify(pkt->payload, pkt->rx_ctrl.sig_len, &fr) < 0) return;

    uint8_t ch   = (uint8_t)pkt->rx_ctrl.channel;
    int8_t  rssi = (int8_t)pkt->rx_ctrl.rssi;
    uint8_t fam  = flock_frame_family(&fr);

    /* §4 unit 2: refine a still-generic Wi-Fi contact to CAMERA when its source
     * OUI belongs to a dedicated camera / surveillance maker (IEEE registry,
     * gz_camera.h). Only a HIGH match is promoted; a randomized MAC is never
     * attributed. A surveillance family flock_frame already found is untouched. */
    if (fam == POSEI_DET_FAM_WIFI_AP || fam == POSEI_DET_FAM_WIFI_STA) {
        if (gz_camera_classify_mac(fr.addr) == GZ_CAMERA_HIGH)
            fam = POSEI_DET_FAM_CAMERA;
        /* §4 unit 3: an OUI IEEE-assigned to an LE-equipment maker (Axon) tags
         * LE_EQUIP. The family is low-confidence by definition (gz_le_equip.h),
         * so it informs the radar but the alert overlay (§4 unit 6) gates on it. */
        else if (gz_le_equip_classify_mac(fr.addr) == GZ_LE_EQUIP_LOW)
            fam = POSEI_DET_FAM_LE_EQUIP;
    }

    portENTER_CRITICAL_ISR(&s_mux);
    if (gz_wifi_batch_add(&s_stage, fr.addr, ch, rssi, fam) == GZ_ADD_FULL)
        s_flush_req = true;
    portEXIT_CRITICAL_ISR(&s_mux);
}

void gz_wifi_scan_init(uint8_t node_id)
{
    gz_wifi_batch_init(&s_stage, node_id);
    uint32_t now = millis();
    s_last_pub_ms = now;
    s_last_hop_ms = now;
    s_cur_ch      = GZ_WIFI_CH_MIN;

    /* STA + ESP-NOW are already up (gz_link_init); just turn on capture. */
    wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(promisc_cb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
}

static void publish_if_due(uint32_t now)
{
    if (!gz_publish_due(s_last_pub_ms, now, GZ_WIFI_PUBLISH_MS, s_flush_req))
        return;

    posei_wifi_det_t snap;
    portENTER_CRITICAL(&s_mux);
    snap = s_stage;
    gz_wifi_batch_reset(&s_stage);
    s_flush_req = false;
    portEXIT_CRITICAL(&s_mux);

    posei_msg_t msg;
    size_t n = gz_wifi_batch_pack(&snap, &msg, s_seq++);
    if (n) {
        gz_link_send(&msg);   /* gz_link_send returns to ch1 for the TX */
        gz_stats_note_publish(GZ_RADIO_WIFI, snap.count);   /* status screen (P2a) */
        gz_radar_feed(GZ_RADAR_WIFI, (const uint8_t *)snap.det, snap.count, now);  /* P2b radar */
    }
    s_last_pub_ms = now;
}

void gz_wifi_scan_tick(void)
{
    uint32_t now = millis();

    /* Publish first (briefly returns to ch1), then advance the dwell channel. */
    publish_if_due(now);

    if ((uint32_t)(now - s_last_hop_ms) >= GZ_WIFI_HOP_MS) {
        s_last_hop_ms = now;
        s_cur_ch = gz_next_channel(s_cur_ch);
        esp_wifi_set_channel(s_cur_ch, WIFI_SECOND_CHAN_NONE);
    }
}
