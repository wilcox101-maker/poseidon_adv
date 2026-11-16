/*
 * proto.c — pack/unpack + send wrappers for the POSEIDON wire proto.
 */
#include "proto.h"
#include <string.h>
#include "esp_now.h"

static const uint8_t BROADCAST[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

void proto_init_msg(posei_msg_t *m, uint8_t type)
{
    memset(m, 0, sizeof(*m));
    m->magic   = POSEI_MAGIC;
    m->version = POSEI_VERSION;
    m->type    = type;
}

/* v5: send only header + used payload (posei_wire_len), not the whole struct,
 * so raising POSEI_PAYLOAD_MAX doesn't bloat small frames on air. */
void proto_send_broadcast(const posei_msg_t *m)
{
    esp_now_send(BROADCAST, (const uint8_t *)m, posei_wire_len(m));
}

void proto_send_to(const uint8_t mac[6], const posei_msg_t *m)
{
    esp_now_send(mac, (const uint8_t *)m, posei_wire_len(m));
}

int proto_validate(const uint8_t *data, int len, posei_msg_t *out)
{
    if (len < (int)POSEI_HEADER_SZ) return 0;
    memset(out, 0, sizeof(*out));
    /* Copy the header first so we can trust payload_len, then the declared
     * payload, bounded by both the ceiling and what actually arrived. */
    memcpy(out, data, POSEI_HEADER_SZ);
    if (out->magic != POSEI_MAGIC) return 0;
    if (out->version != POSEI_VERSION) return 0;
    uint16_t pl = out->payload_len;
    if (pl > POSEI_PAYLOAD_MAX) return 0;
    if ((int)(POSEI_HEADER_SZ + pl) > len) return 0;
    if (pl) memcpy(out->payload, data + POSEI_HEADER_SZ, pl);
    return 1;
}
