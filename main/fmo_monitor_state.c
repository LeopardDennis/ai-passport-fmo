#include "fmo_monitor_state.h"

#include <stddef.h>
#include <string.h>

static void copy_text(char *destination, size_t destination_size,
                      const char *source)
{
    if (!destination || destination_size == 0) return;
    if (!source) source = "";
    size_t length = 0;
    while (length + 1 < destination_size && source[length] != '\0') ++length;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

void fmo_monitor_state_init(fmo_monitor_state_t *state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

void fmo_monitor_set_wifi(fmo_monitor_state_t *state, bool connected)
{
    if (!state) return;
    state->wifi_connected = connected;
    if (!connected) {
        state->channel_valid = false;
        state->events_connected = false;
        state->control_connected = false;
        state->speaking = false;
        state->speaker[0] = '\0';
    }
}

void fmo_monitor_set_events(fmo_monitor_state_t *state, bool connected)
{
    if (!state) return;
    state->events_connected = connected;
    if (!connected) {
        state->speaking = false;
        state->speaker[0] = '\0';
    }
}

void fmo_monitor_set_control(fmo_monitor_state_t *state, bool connected)
{
    if (!state) return;
    state->control_connected = connected;
    if (!connected) state->channel_valid = false;
}

void fmo_monitor_invalidate_channel(fmo_monitor_state_t *state)
{
    if (state) state->channel_valid = false;
}

void fmo_monitor_set_channel(fmo_monitor_state_t *state, uint32_t uid,
                             const char *name)
{
    if (!state) return;
    if (state->channel_uid != uid) {
        state->speaking = false;
        state->speaker_is_host = false;
        state->speaker[0] = '\0';
        state->last_speaker[0] = '\0';
        state->grid[0] = '\0';
        state->last_speaker_ms = 0;
    }
    state->channel_uid = uid;
    state->channel_valid = uid != 0;
    copy_text(state->channel_name, sizeof(state->channel_name), name);
}

void fmo_monitor_apply_speaker(fmo_monitor_state_t *state,
                               const char *callsign, const char *grid,
                               bool speaking, bool is_host, uint64_t now_ms)
{
    if (!state || !callsign || callsign[0] == '\0') return;

    if (speaking) {
        copy_text(state->speaker, sizeof(state->speaker), callsign);
        copy_text(state->last_speaker, sizeof(state->last_speaker), callsign);
        copy_text(state->grid, sizeof(state->grid), grid);
        state->speaking = true;
        state->speaker_is_host = is_host;
        state->last_speaker_ms = now_ms;
        return;
    }

    if (state->speaking && strcmp(state->speaker, callsign) == 0) {
        state->speaking = false;
        state->speaker[0] = '\0';
        state->last_speaker_ms = now_ms;
    }
}
