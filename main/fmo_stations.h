#pragma once
#include <stdbool.h>
#include <stdint.h>

#define FMO_STATION_PAGE_SIZE 6
#define FMO_STATION_TIMEOUT_MS 10000
#define FMO_STATION_COUNT_BUDGET_MS 3000
#define FMO_STATION_COUNT_LIMIT 65535

typedef enum {
    FMO_STATION_ALL, FMO_STATION_PINNED, FMO_STATION_ACTIVE,
    FMO_STATION_CATEGORY_COUNT
} fmo_station_category_t;
const char *fmo_stations_query_name(fmo_station_category_t category);
fmo_station_category_t fmo_stations_response_category(const char *subtype);

typedef struct { uint32_t uid; char name[48]; } fmo_station_t;
typedef enum {
    FMO_STATIONS_IDLE, FMO_STATIONS_LOADING, FMO_STATIONS_READY,
    FMO_STATIONS_EMPTY, FMO_STATIONS_LOAD_FAILED, FMO_STATIONS_SWITCHING,
    FMO_STATIONS_SUCCESS, FMO_STATIONS_FAILED, FMO_STATIONS_UNKNOWN
} fmo_station_status_t;
typedef enum {
    FMO_STATION_NONE, FMO_STATION_LIST_QUEUED, FMO_STATION_LIST_WAIT,
    FMO_STATION_SWITCH_QUEUED, FMO_STATION_SET_WAIT,
    FMO_STATION_VERIFY, FMO_STATION_RECONCILE
} fmo_station_request_t;
typedef struct {
    fmo_station_category_t category;
    fmo_station_t rows[FMO_STATION_PAGE_SIZE];
    uint32_t start, target_uid;
    unsigned count;
    bool has_next, audio_paused;
    bool total_known, counting;
    uint32_t total, count_low, count_high, count_probe;
    unsigned count_queries;
    uint64_t count_started_ms;
    fmo_station_status_t status;
    fmo_station_request_t request;
    uint64_t started_ms;
} fmo_stations_t;

bool fmo_stations_load(fmo_stations_t *s, fmo_station_category_t category, uint32_t start, uint64_t now);
/* Count with bounded one-row range probes before loading the requested page. */
void fmo_stations_begin_count(fmo_stations_t *s, uint64_t now);
void fmo_stations_prepare_query(fmo_stations_t *s, uint64_t now);
bool fmo_stations_switch(fmo_stations_t *s, uint32_t uid, uint32_t current, uint64_t now);
void fmo_stations_list(fmo_stations_t *s, const fmo_station_t *rows, unsigned count);
void fmo_stations_ack(fmo_stations_t *s, bool success);
void fmo_stations_current(fmo_stations_t *s, uint32_t uid);
void fmo_stations_disconnect(fmo_stations_t *s);
/* True means drain the control socket before assigning a new reply owner. */
bool fmo_stations_expire(fmo_stations_t *s, uint64_t now);
