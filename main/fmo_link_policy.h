#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Allow transient LAN/worker delays without treating a brief idle period as
 * a lost session. Channel replies still belong to a single outstanding query. */
#define FMO_LINK_IO_TIMEOUT_MS 3000
#define FMO_LINK_PING_INTERVAL_SEC 10
#define FMO_LINK_PONG_TIMEOUT_SEC 30
#define FMO_CHANNEL_QUERY_TIMEOUT_MS 10000
#define FMO_CHANNEL_MAX_AGE_MS 15000

/* Library reconnects normally suffice. Bound a failed startup/session even
 * when its client handle still exists; the coordinator then refreshes DNS and
 * replaces only the failed metadata client. Never count intentional pauses. */
#define FMO_LINK_RECOVERY_MS 30000
typedef struct {
    uint64_t since_ms;
    bool waiting;
} fmo_link_recovery_t;

static inline bool fmo_link_recovery_due(fmo_link_recovery_t *recovery,
                                         bool enabled, bool healthy,
                                         bool manual, uint64_t now_ms)
{
    if (!enabled || healthy) {
        recovery->waiting = false;
        return false;
    }
    if (!recovery->waiting || now_ms < recovery->since_ms) {
        recovery->waiting = true;
        recovery->since_ms = now_ms;
    }
    if (!manual && now_ms - recovery->since_ms < FMO_LINK_RECOVERY_MS) return false;
    recovery->since_ms = now_ms;
    return true;
}

/* Audio frames arrive in bursts and may pause mid-frame. Keep their transport
 * wait separate from interactive channel requests; delayed audio pongs must
 * not interrupt otherwise valid PCM. The audio worker applies a deadline to
 * ANY valid PCM/PING/PONG activity; TCP failures still reconnect normally. */
#define FMO_AUDIO_IO_TIMEOUT_MS 10000
#define FMO_AUDIO_ACTIVITY_TIMEOUT_MS 90000
#define FMO_AUDIO_BUFFER_WAIT_MS 2000
