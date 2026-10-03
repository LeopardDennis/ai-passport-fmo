#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FMO_CALLSIGN_MAX 15
#define FMO_CHANNEL_NAME_MAX 31
#define FMO_GRID_MAX 11

#define FMO_HISTORY_COUNT 3
typedef struct {
    char callsign[FMO_CALLSIGN_MAX + 1];
    int64_t timestamp;
} fmo_history_entry_t;
/* Most recent FMO event history, newest first; independent of live PTT state. */
typedef struct {
    fmo_history_entry_t entries[FMO_HISTORY_COUNT];
    unsigned count;
} fmo_history_t;

typedef struct {
    fmo_history_t history;
    bool wifi_connected;
    bool events_connected;
    bool control_connected;
    bool channel_valid;
    bool speaking;
    bool speaker_is_host;
    bool speaker_cross_server;
    uint32_t channel_uid;
    uint64_t last_speaker_ms;
    uint64_t channel_confirmed_ms;
    char channel_name[FMO_CHANNEL_NAME_MAX + 1];
    char speaker[FMO_CALLSIGN_MAX + 1];
    char last_speaker[FMO_CALLSIGN_MAX + 1];
    char grid[FMO_GRID_MAX + 1];
} fmo_monitor_state_t;

void fmo_monitor_state_init(fmo_monitor_state_t *state);
void fmo_monitor_set_wifi(fmo_monitor_state_t *state, bool connected);
void fmo_monitor_set_events(fmo_monitor_state_t *state, bool connected);
void fmo_monitor_set_control(fmo_monitor_state_t *state, bool connected);
void fmo_monitor_set_channel(fmo_monitor_state_t *state, uint32_t uid,
                             const char *name);
void fmo_monitor_invalidate_channel(fmo_monitor_state_t *state);
/* An empty/null callsign with speaking=false is an explicit idle release. */
void fmo_monitor_apply_speaker(fmo_monitor_state_t *state,
                               const char *callsign, const char *grid,
                               bool speaking, bool is_host, bool cross_server, uint64_t now_ms);
