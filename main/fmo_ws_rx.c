#include "fmo_ws_rx.h"
#include <string.h>

void fmo_ws_rx_reset(fmo_ws_rx_t *rx)
{
    memset(rx, 0, sizeof(*rx));
}

fmo_rx_result_t fmo_ws_rx_feed(fmo_ws_rx_t *rx, uint8_t opcode, bool fin,
                              size_t frame_length, size_t offset,
                              const char *data, size_t length)
{
    /* Control frames can appear between fragments without disturbing text. */
    if (opcode >= 8) return FMO_RX_IGNORED;
    if (opcode != 0 && opcode != 1) goto invalid;
    if ((length && !data) || offset > frame_length || length > frame_length - offset)
        goto invalid;
    if (offset == 0) {
        if (rx->frame_offset != rx->frame_length) goto invalid;
        if (opcode == 1) {
            if (rx->active) goto invalid;
            fmo_ws_rx_reset(rx);
            rx->active = true;
        } else if (!rx->active) {
            goto invalid;
        }
        rx->frame_length = frame_length;
        rx->frame_offset = 0;
        rx->opcode = opcode;
        rx->fin = fin;
    }
    if (!rx->active || offset != rx->frame_offset || frame_length != rx->frame_length ||
        opcode != rx->opcode || fin != rx->fin ||
        length > sizeof(rx->text) - 1 - rx->length) goto invalid;
    if (length) memcpy(rx->text + rx->length, data, length);
    rx->length += length;
    rx->frame_offset += length;
    if (rx->frame_offset == rx->frame_length && fin) {
        rx->text[rx->length] = '\0';
        rx->active = false;
        return FMO_RX_COMPLETE;
    }
    return FMO_RX_MORE;
invalid:
    fmo_ws_rx_reset(rx);
    return FMO_RX_INVALID;
}
