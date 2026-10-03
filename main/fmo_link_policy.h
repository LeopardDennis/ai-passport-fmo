#pragma once

/* Allow transient LAN/worker delays without treating a brief idle period as
 * a lost session. Channel replies still belong to a single outstanding query. */
#define FMO_LINK_IO_TIMEOUT_MS 3000
#define FMO_LINK_PING_INTERVAL_SEC 10
#define FMO_LINK_PONG_TIMEOUT_SEC 30
#define FMO_CHANNEL_QUERY_TIMEOUT_MS 10000
#define FMO_CHANNEL_MAX_AGE_MS 15000
