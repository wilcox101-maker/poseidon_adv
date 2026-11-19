/*
 * gz_link.cpp — GODZILLA node ESP-NOW link (task 3a).
 *
 * Mirrors c5_node's ESP-NOW bring-up and HELLO on the Arduino/pioarduino side:
 * STA mode pinned to the C5 link channel, esp_now_init, a broadcast peer, and
 * a HELLO beacon. ESP-NOW v2 is auto-negotiated on IDF >= 5.4 (this tree runs
 * 5.5.4), which the v5 wire layout needs for the larger detection batches in
 * 3b/3c; the HELLO itself is tiny and fits either version.
 */
#include "gz_link.h"
#include "gz_hello.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const uint8_t GZ_BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* Upper bound on how long gz_link_send() waits for a publish to actually air
 * before it moves the radio off the C5 link channel (task 3c hops). 10-20 ms
 * per the P2a advice; bounded so a lost send-callback can never stall the loop. */
#define GZ_SEND_WAIT_MS 15u

/* Set false just before each esp_now_send(), set true by the send callback when
 * the frame has left the air. Written by the Wi-Fi driver's send-cb context and
 * polled by the main loop — volatile, single-writer, no lock needed. */
static volatile bool s_send_done = false;

/* IDF >= 5.5 signature: the send callback gets wifi_tx_info_t, not the peer
 * MAC (esp_now_send_cb_t in esp_now.h). We only need "it aired". */
static void gz_on_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    (void)status;
    s_send_done = true;
}

/* Passive node: validate the fixed header and otherwise ignore. P1 is
 * fire-and-forget and takes no commands; detections flow node -> S3 only. A
 * callback is registered anyway so the stack has a home for inbound frames
 * (and so malformed frames are dropped cleanly rather than ignored blindly). */
static void gz_on_recv(const esp_now_recv_info_t *info,
                       const uint8_t *data, int len)
{
    (void)info;
    if (len < (int)POSEI_HEADER_SZ) return;
    const posei_msg_t *m = (const posei_msg_t *)data;
    if (m->magic != POSEI_MAGIC || m->version != POSEI_VERSION) return;
    if (m->payload_len > POSEI_PAYLOAD_MAX) return;
    /* No command handling in P1. */
}

void gz_link_init(void)
{
    WiFi.mode(WIFI_STA);                 /* STA up, never associated */
    esp_wifi_set_channel(GZ_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);

    esp_now_init();
    esp_now_register_recv_cb(gz_on_recv);
    esp_now_register_send_cb(gz_on_sent);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GZ_BROADCAST, 6);
    peer.ifidx   = WIFI_IF_STA;
    peer.channel = 0;                    /* 0 = use the interface's current ch */
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

bool gz_link_send(const posei_msg_t *m)
{
    /* Return to the C5 link channel for the transmit, then restore whatever
     * channel the Wi-Fi scanner is dwelling on (task 3c hops 1..13). ESP-NOW
     * only reaches the S3 on its channel, so a publish sent mid-hop would be
     * lost. All sends run on the main loop, single-threaded with the hopper,
     * so there is no race with the channel changes. On a node that is not
     * hopping (3a/3b) the channel is already 1 and this is a no-op. */
    uint8_t prim = GZ_LINK_CHANNEL;
    wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&prim, &sec);
    bool off_channel = (prim != GZ_LINK_CHANNEL);

    if (off_channel) esp_wifi_set_channel(GZ_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);

    s_send_done = false;                 /* arm before the send, not after */
    bool ok = esp_now_send(GZ_BROADCAST, (const uint8_t *)m, posei_wire_len(m)) == ESP_OK;

    if (off_channel) {
        /* esp_now_send() only queues the frame; restoring the scan channel
         * immediately could move the radio before it airs. Wait for the send
         * callback, bounded by GZ_SEND_WAIT_MS so a dropped callback can't
         * stall the loop. Only needed when hopping — on ch1 we stay put. */
        uint32_t t0 = millis();
        while (!s_send_done && (uint32_t)(millis() - t0) < GZ_SEND_WAIT_MS)
            delay(1);
        esp_wifi_set_channel(prim, sec);
    }
    return ok;
}

void gz_link_send_hello(void)
{
    posei_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.magic   = POSEI_MAGIC;
    msg.version = POSEI_VERSION;
    msg.type    = POSEI_TYPE_HELLO;
    gz_build_hello((posei_hello_t *)msg.payload, ESP.getFreeHeap() / 1024);
    msg.payload_len = sizeof(posei_hello_t);
    gz_link_send(&msg);
}
