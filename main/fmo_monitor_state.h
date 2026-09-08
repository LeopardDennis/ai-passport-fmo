#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FMO_CALLSIGN_MAX 15
#define FMO_CHANNEL_NAME_MAX 31
#define FMO_GRID_MAX 11

typedef struct {
    bool wifi_connected;
    bool events_connected;
    bool control_connected;
    bool channel_valid;
    bool speaking;
    bool speaker_is_host;
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
void fmo_monitor_apply_speaker(fmo_monitor_state_t *state,
                               const char *callsign, const char *grid,
                               bool speaking, bool is_host, uint64_t now_ms);
