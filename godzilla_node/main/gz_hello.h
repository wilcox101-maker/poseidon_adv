/*
 * gz_hello.h — GODZILLA HELLO payload builder.
 *
 * Fills the v5 posei_hello_t (c5_node/main/proto.h) for this node, exactly as
 * c5_node's send_hello() does for the C5, but with GODZILLA's identity:
 * name "GODZILLA", a sensor/detection role (deliberately != 1, the C5's role),
 * no 5 GHz and no 802.15.4 radio. The S3 admits peers by capability, not role
 * (src/c5_cmd.cpp), and stores name/role/radios for its Diagnostics view.
 *
 * Pure: proto.h + string.h only, no radio or Arduino/ESP-IDF headers, so the
 * identity is pinned natively by test/test_godzilla_hello.
 */
#pragma once

#include <string.h>
#include "../../c5_node/main/proto.h"

/* One name per board so the Cardputer can tell two GODZILLAs apart. Must fit
 * posei_hello_t.name (11 chars + NUL); checked below. */
#if defined(GZ_BOARD_AMOLED206)
#define GZ_HELLO_NAME "GODZILLA206"
#elif defined(GZ_BOARD_ES3C28P)
#define GZ_HELLO_NAME "GODZILLA28"
#else
#define GZ_HELLO_NAME "GODZILLA"
#endif
#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#include <assert.h>
static_assert(sizeof(GZ_HELLO_NAME) <= sizeof(((posei_hello_t *)0)->name),
              "GZ_HELLO_NAME does not fit posei_hello_t.name");
#endif

/* Role advertised in HELLO. 1 is the C5; the S3 does not gate admission on
 * role, so this is a display/identity tag. 2 marks a passive detection node. */
#define GZ_ROLE 2

/* Build this node's HELLO payload. heap_kb is the caller's free-heap reading
 * (ESP.getFreeHeap()/1024 on-target); passed in so this stays pure. */
static inline void gz_build_hello(posei_hello_t *h, uint32_t heap_kb)
{
    memset(h, 0, sizeof(*h));
    /* Fits with its NUL (static_assert above), so copy it exactly. */
    memcpy(h->name, GZ_HELLO_NAME, sizeof(GZ_HELLO_NAME));
    h->heap_kb        = heap_kb;
    h->role           = GZ_ROLE;
    h->has_5g         = 0;  /* ESP32-S3: 2.4 GHz only */
    h->has_ieee802154 = 0;  /* no Zigbee/Thread radio */
}
