#pragma once

/* Allow transient LAN/worker delays without treating a brief idle period as
 * a lost session. Channel replies still belong to a single outstanding query. */
#define FMO_LINK_IO_TIMEOUT_MS 3000
#define FMO_LINK_PING_INTERVAL_SEC 10
#define FMO_LINK_PONG_TIMEOUT_SEC 30
#define FMO_CHANNEL_QUERY_TIMEOUT_MS 10000
#define FMO_CHANNEL_MAX_AGE_MS 15000

/* Audio frames arrive in bursts and may pause mid-frame. Keep their transport
 * wait separate from interactive channel requests; delayed audio pongs must
 * not interrupt otherwise valid PCM. The audio worker applies a deadline to
 * ANY valid PCM/PING/PONG activity; TCP failures still reconnect normally. */
#define FMO_AUDIO_IO_TIMEOUT_MS 10000
#define FMO_AUDIO_ACTIVITY_TIMEOUT_MS 90000
#define FMO_AUDIO_BUFFER_WAIT_MS 2000
