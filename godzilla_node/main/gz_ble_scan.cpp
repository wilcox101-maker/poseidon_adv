/*
 * gz_ble_scan.cpp — GODZILLA passive BLE scan → batch → publish (task 3b).
 *
 * NimBLE 2.x passive scan set up like the Cardputer's defensive monitor
 * (src/features/defensive_monitor.cpp): callback-only, store no results
 * (POS-AUDIT-011 — NimBLE's internal results vector grows unbounded and
 * reboots the device otherwise), active scan OFF so we never emit scan
 * requests.
 *
 * Producer/consumer split:
 *   - the NimBLE host task (callback) classifies each advert and adds it to a
 *     spinlock-protected staging batch — allocation-free, never transmits;
 *   - the Arduino loop packs + broadcasts the batch on a ~1 s tick.
 * ESP-NOW is NOT sent from the callback: sending from the host-task context
 * (and the flash/radio contention it implies) is exactly what the C5's
 * zb_sniffer drain-task note warns against, so publishes stay on the loop.
 *
 * Overflow: if a window sees more than POSEI_DET_MAX (64) distinct devices the
 * batch fills and asks the loop to publish early (s_flush_req); the device
 * that overflowed is dropped for that window and picked up on the next, since
 * BLE beacons advertise continuously. Documented trade-off, not a loss of a
 * persistent device.
 */
#include "gz_ble_scan.h"
#include "gz_batch.h"
#include "gz_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>

#include "../../src/det_classify.h"   /* read-only shared classifier (#47) */

/* Staging batch shared between the NimBLE host task (writer) and the Arduino
 * loop (reader). portMUX is the correct cross-core primitive: the two tasks
 * can run on different cores on the S3. */
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;
static posei_ble_det_t s_stage;
static volatile bool   s_flush_req   = false;
static uint16_t        s_seq         = 0;
static uint32_t        s_last_pub_ms = 0;

class GzBleScanCb : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override {
        uint8_t addr[6];
        memcpy(addr, dev->getAddress().getBase()->val, 6);
        uint8_t atype = dev->getAddress().getType();
        int8_t  rssi  = (int8_t)dev->getRSSI();

        /* getPayload() returns a const ref to NimBLE's stored buffer — no copy.
         * det_classify_ble is pure and allocation-free. */
        const std::vector<uint8_t> &pl = dev->getPayload();
        uint8_t fam = det_classify_ble(pl.data(), pl.size());

        portENTER_CRITICAL(&s_mux);
        if (gz_ble_batch_add(&s_stage, addr, atype, rssi, fam) == GZ_ADD_FULL)
            s_flush_req = true;        /* batch full → ask the loop to publish */
        portEXIT_CRITICAL(&s_mux);
    }

    void onScanEnd(const NimBLEScanResults &results, int reason) override {
        (void)results;
        (void)reason;
        /* Keep the scan continuous if the stack ever ends it. */
        NimBLEDevice::getScan()->start(0, false, false);
    }
};

static GzBleScanCb s_cb;

void gz_ble_scan_init(uint8_t node_id)
{
    gz_ble_batch_init(&s_stage, node_id);
    s_last_pub_ms = millis();

    if (!NimBLEDevice::isInitialized())
        NimBLEDevice::init("");

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&s_cb, false);
    scan->setMaxResults(0);            /* POS-AUDIT-011: callback only */
    scan->setActiveScan(false);        /* PASSIVE — never send scan requests */
    scan->setInterval(97);
    scan->setWindow(67);
    scan->setDuplicateFilter(false);
    scan->start(0, false, false);      /* 0 = scan continuously */
}

void gz_ble_scan_tick(void)
{
    uint32_t now = millis();
    if (!gz_publish_due(s_last_pub_ms, now, GZ_BLE_PUBLISH_MS, s_flush_req))
        return;

    /* Snapshot + reset under the lock; pack + send outside it to keep the
     * critical section short. */
    posei_ble_det_t snap;
    portENTER_CRITICAL(&s_mux);
    snap = s_stage;
    gz_ble_batch_reset(&s_stage);
    s_flush_req = false;
    portEXIT_CRITICAL(&s_mux);

    posei_msg_t msg;
    size_t n = gz_ble_batch_pack(&snap, &msg, s_seq++);
    if (n) gz_link_send(&msg);         /* empty window → nothing on the wire */
    s_last_pub_ms = now;
}
