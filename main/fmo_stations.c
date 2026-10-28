#include "fmo_stations.h"
#include <string.h>

const char *fmo_stations_query_name(fmo_station_category_t category)
{
    static const char *const names[] = {"getListRange", "getPinnedList", "getActiveList"};
    return (unsigned)category < FMO_STATION_CATEGORY_COUNT ? names[category] : NULL;
}

fmo_station_category_t fmo_stations_response_category(const char *subtype)
{
    static const char *const names[] = {"getListResponse", "getPinnedListResponse", "getActiveListResponse"};
    if (subtype) for (unsigned i = 0; i < FMO_STATION_CATEGORY_COUNT; ++i)
        if (!strcmp(subtype, names[i])) return (fmo_station_category_t)i;
    return FMO_STATION_CATEGORY_COUNT;
}

bool fmo_stations_load(fmo_stations_t *s, fmo_station_category_t category, uint32_t start, uint64_t now)
{
    if ((unsigned)category >= FMO_STATION_CATEGORY_COUNT ||
        s->request != FMO_STATION_NONE || start > UINT32_MAX - FMO_STATION_PAGE_SIZE - 1)
        return false;
    if (category != s->category || !start) {
        s->total_known = false;
        s->total = 0;
    }
    s->category = category;
    s->counting = false;
    s->start = start;
    s->count = 0;
    s->has_next = false;
    s->status = FMO_STATIONS_LOADING;
    s->request = FMO_STATION_LIST_QUEUED;
    s->started_ms = now;
    return true;
}

void fmo_stations_begin_count(fmo_stations_t *s, uint64_t now)
{
    if (s->request != FMO_STATION_LIST_QUEUED || (s->start && s->total_known)) return;
    s->total_known = false;
    s->counting = true;
    s->count_low = 0;
    s->count_high = UINT32_MAX;
    s->count_probe = 64;
    s->count_queries = 0;
    s->count_started_ms = now;
}

void fmo_stations_prepare_query(fmo_stations_t *s, uint64_t now)
{
    if (!s->counting) return;
    if (s->count_queries >= 32 || now - s->count_started_ms >= FMO_STATION_COUNT_BUDGET_MS)
        s->counting = false; /* Fall back to the normal page, never invent a total. */
    else ++s->count_queries;
}

static void count_reply(fmo_stations_t *s, const fmo_station_t *rows, unsigned count)
{
    s->request = FMO_STATION_LIST_QUEUED;
    if (!rows || count > 1) {
        s->counting = false;
        return;
    }
    if (count) s->count_low = s->count_probe + 1;
    else s->count_high = s->count_probe;
    if (s->count_high != UINT32_MAX) {
        if (s->count_low == s->count_high) {
            s->total = s->count_low;
            s->total_known = true;
            s->counting = false;
        } else s->count_probe = s->count_low + (s->count_high - s->count_low) / 2;
    } else if (s->count_probe >= FMO_STATION_COUNT_LIMIT) s->counting = false;
    else {
        s->count_probe = s->count_probe * 2 + 1;
        if (s->count_probe > FMO_STATION_COUNT_LIMIT) s->count_probe = FMO_STATION_COUNT_LIMIT;
    }
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
    if (s->counting) { count_reply(s, rows, count); return; }
    s->request = FMO_STATION_NONE;
    if (!rows || count > FMO_STATION_PAGE_SIZE + 1) {
        s->status = FMO_STATIONS_LOAD_FAILED;
        return;
    }
    if (s->total_known && (s->start + count > s->total ||
        (count < FMO_STATION_PAGE_SIZE + 1 && s->start + count != s->total)))
        s->total_known = false; /* The remote list may change between queries. */
    if (count <= FMO_STATION_PAGE_SIZE && (count || !s->start)) {
        s->total = s->start + count;
        s->total_known = true;
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
    s->counting = false;
    s->total_known = false;
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
