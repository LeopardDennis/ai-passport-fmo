#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FMO_WS_MESSAGE_CAPACITY 768
typedef struct {
    char text[FMO_WS_MESSAGE_CAPACITY];
    size_t length;
    size_t frame_offset;
    size_t frame_length;
    uint8_t opcode;
    bool active;
    bool fin;
} fmo_ws_rx_t;

typedef enum { FMO_RX_MORE, FMO_RX_COMPLETE, FMO_RX_IGNORED, FMO_RX_INVALID } fmo_rx_result_t;
void fmo_ws_rx_reset(fmo_ws_rx_t *rx);
/* Offsets refer to a frame; text remains valid until the next call. */
fmo_rx_result_t fmo_ws_rx_feed(fmo_ws_rx_t *rx, uint8_t opcode, bool fin,
                              size_t frame_length, size_t offset,
                              const char *data, size_t length);
