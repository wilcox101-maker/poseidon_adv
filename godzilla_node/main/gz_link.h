/*
 * gz_link.h — GODZILLA node ESP-NOW link.
 *
 * Task 3a: bring up the ESP-NOW link to the Cardputer (S3) and announce this
 * node with a HELLO, the same presence beacon c5_node sends. STA mode on the
 * C5 link channel (1), a broadcast peer, and HELLO on a 5 s cadence.
 *
 * Passive only: the link transmits ESP-NOW HELLO (here) and, from 3b, batched
 * detection publishes — nothing else. It arms no transmitter and takes no
 * commands in P1 (fire-and-forget: the S3 sends no ack for 60/61).
 */
#pragma once

#include "../../c5_node/main/proto.h"

/* The Cardputer link channel. ESP-NOW peers must share a channel; the C5 node
 * and the S3 sit on 1, so the node stays on 1 to be heard (3c will hop for
 * Wi-Fi scanning and return to 1 for each publish). */
#define GZ_LINK_CHANNEL 1

/* WiFi STA on GZ_LINK_CHANNEL, esp_now_init, broadcast peer, recv cb. */
void gz_link_init(void);

/* Broadcast a ready-built message, sending only header + payload_len bytes
 * (posei_wire_len). Returns true on esp_now_send() == ESP_OK. */
bool gz_link_send(const posei_msg_t *m);

/* Build this node's HELLO (gz_hello.h) and broadcast it. */
void gz_link_send_hello(void);
