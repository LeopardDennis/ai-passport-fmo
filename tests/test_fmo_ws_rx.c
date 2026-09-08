#include "fmo_ws_rx.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    fmo_ws_rx_t rx = { 0 };
    assert(fmo_ws_rx_feed(&rx, 1, true, 2, 0, "{}", 2) == FMO_RX_COMPLETE);
    assert(strcmp(rx.text, "{}") == 0);
    /* A frame split across transport reads. */
    assert(fmo_ws_rx_feed(&rx, 1, true, 2, 0, "{", 1) == FMO_RX_MORE);
    assert(fmo_ws_rx_feed(&rx, 1, true, 2, 1, "}", 1) == FMO_RX_COMPLETE);
    /* Text fragmented across frames, with an interleaved ping. */
    assert(fmo_ws_rx_feed(&rx, 1, false, 1, 0, "{", 1) == FMO_RX_MORE);
    assert(fmo_ws_rx_feed(&rx, 9, true, 1, 0, "x", 1) == FMO_RX_IGNORED);
    assert(fmo_ws_rx_feed(&rx, 0, false, 1, 0, "}", 1) == FMO_RX_MORE);
    assert(fmo_ws_rx_feed(&rx, 0, true, 0, 0, NULL, 0) == FMO_RX_COMPLETE);
    assert(strcmp(rx.text, "{}") == 0);
    assert(fmo_ws_rx_feed(&rx, 0, true, 1, 0, "x", 1) == FMO_RX_INVALID);
    /* Reject discontinuities, binary data, and aggregate overflow; recover. */
    assert(fmo_ws_rx_feed(&rx, 1, true, 3, 0, "{", 1) == FMO_RX_MORE);
    assert(fmo_ws_rx_feed(&rx, 1, true, 3, 2, "}", 1) == FMO_RX_INVALID);
    assert(fmo_ws_rx_feed(&rx, 2, true, 1, 0, "x", 1) == FMO_RX_INVALID);
    char full[FMO_WS_MESSAGE_CAPACITY];
    memset(full, 'x', sizeof(full));
    assert(fmo_ws_rx_feed(&rx, 1, false, sizeof(full)-1, 0, full, sizeof(full)-1) == FMO_RX_MORE);
    assert(fmo_ws_rx_feed(&rx, 0, true, 1, 0, "x", 1) == FMO_RX_INVALID);
    assert(fmo_ws_rx_feed(&rx, 1, true, 2, 0, "{}", 2) == FMO_RX_COMPLETE);
    return 0;
}
