#include "fmo_stations.h"
#include <string.h>

bool fmo_stations_load(fmo_stations_t *s, uint32_t start, uint64_t now)
{
    if (s->request != FMO_STATION_NONE || start > UINT32_MAX - FMO_STATION_PAGE_SIZE - 1)
        return false;
    s->start = start;
    s->count = 0;
    s->has_next = false;
    s->status = FMO_STATIONS_LOADING;
    s->request = FMO_STATION_LIST_QUEUED;
    s->started_ms = now;
    return true;
}

bool fmo_stations_switch(fmo_stations_t *s, uint32_t uid, uint32_t current, uint64_t now)
{
    if (!uid || s->request != FMO_STATION_NONE) return false;
    bool present = false;
    for (unsigned i = 0; i < s->count; ++i) present |= s->rows[i].uid == uid;
    if (!present) return false;
    s->target_uid = uid;
    if (uid == current) { s->status = FMO_STATIONS_SUCCESS; return true; }
    s->status = FMO_STATIONS_SWITCHING;
    s->request = FMO_STATION_SWITCH_QUEUED;
    s->audio_paused = true;
    s->started_ms = now;
    return true;
}

void fmo_stations_list(fmo_stations_t *s, const fmo_station_t *rows, unsigned count)
{
    if (s->request != FMO_STATION_LIST_WAIT) return;
    s->request = FMO_STATION_NONE;
    if (!rows || count > FMO_STATION_PAGE_SIZE + 1) {
        s->status = FMO_STATIONS_LOAD_FAILED;
        return;
    }
    s->has_next = count > FMO_STATION_PAGE_SIZE;
    s->count = s->has_next ? FMO_STATION_PAGE_SIZE : count;
    memcpy(s->rows, rows, s->count * sizeof(*rows));
    s->status = count ? FMO_STATIONS_READY : FMO_STATIONS_EMPTY;
}

void fmo_stations_ack(fmo_stations_t *s, bool success)
{
    if (s->request != FMO_STATION_SET_WAIT) return;
    s->request = success ? FMO_STATION_VERIFY : FMO_STATION_RECONCILE;
    if (!success) s->status = FMO_STATIONS_FAILED;
}

void fmo_stations_current(fmo_stations_t *s, uint32_t uid)
{
    if (s->request != FMO_STATION_VERIFY && s->request != FMO_STATION_RECONCILE) return;
    if (uid == s->target_uid) s->status = FMO_STATIONS_SUCCESS;
    else if (s->request == FMO_STATION_VERIFY) return;
    s->request = FMO_STATION_NONE;
    s->audio_paused = false;
}

void fmo_stations_disconnect(fmo_stations_t *s)
{
    if (s->request == FMO_STATION_LIST_QUEUED || s->request == FMO_STATION_LIST_WAIT) {
        s->status = FMO_STATIONS_LOAD_FAILED;
        s->request = FMO_STATION_NONE;
    } else if (s->request == FMO_STATION_SWITCH_QUEUED) {
        s->status = FMO_STATIONS_FAILED;
        s->request = FMO_STATION_NONE;
        s->audio_paused = false;
    } else if (s->request == FMO_STATION_SET_WAIT || s->request == FMO_STATION_VERIFY) {
        s->status = FMO_STATIONS_UNKNOWN;
        s->request = FMO_STATION_RECONCILE;
    }
}

bool fmo_stations_expire(fmo_stations_t *s, uint64_t now)
{
    if (s->request == FMO_STATION_NONE || s->request == FMO_STATION_RECONCILE ||
        now - s->started_ms < FMO_STATION_TIMEOUT_MS) return false;
    fmo_stations_disconnect(s);
    return true;
}
