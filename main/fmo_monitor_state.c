#include "fmo_monitor_state.h"
#include "fmo_text.h"

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
        memset(&state->history, 0, sizeof(state->history));
        state->events_connected = false;
        state->control_connected = false;
        state->local_position_valid = false;
        state->speaking = false;
        state->speaker_cross_server = false;
        state->speaker[0] = '\0';
        state->last_speaker[0] = '\0';
        state->grid[0] = '\0';
        state->location[0] = '\0';
        state->position_valid = false;
        state->last_speaker_ms = 0;
        state->channel_uid = 0;
        state->channel_name[0] = '\0';
    }
}

void fmo_monitor_set_events(fmo_monitor_state_t *state, bool connected)
{
    if (!state) return;
    state->events_connected = connected;
    if (!connected) {
        memset(&state->history, 0, sizeof(state->history));
        state->speaking = false;
        state->speaker_cross_server = false;
        state->speaker[0] = '\0';
    }
}

void fmo_monitor_set_control(fmo_monitor_state_t *state, bool connected)
{
    if (!state) return;
    state->control_connected = connected;
    if (!connected) {
        state->local_position_valid = false;
        state->channel_valid = false;
    }
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
        state->speaker_cross_server = false;
        state->speaker_is_host = false;
        state->speaker[0] = '\0';
        state->last_speaker[0] = '\0';
        state->grid[0] = '\0';
        state->location[0] = '\0';
        state->position_valid = false;
        state->last_speaker_ms = 0;
    }
    state->channel_uid = uid;
    state->channel_valid = uid != 0;
    fmo_text_copy_utf8(state->channel_name, sizeof(state->channel_name), name);
}

void fmo_monitor_apply_speaker(fmo_monitor_state_t *state,
                               const char *callsign, const char *grid,
                               bool speaking, bool is_host, bool cross_server, uint64_t now_ms)
{
    if (!state) return;
    if (!callsign) callsign = "";

    if (speaking) {
        if (!callsign[0]) return;
        if (strcmp(state->last_speaker, callsign) || strcmp(state->grid, grid ? grid : "")) {
            state->location[0] = 0;
            state->position_valid = false;
        }
        copy_text(state->speaker, sizeof(state->speaker), callsign);
        copy_text(state->last_speaker, sizeof(state->last_speaker), callsign);
        copy_text(state->grid, sizeof(state->grid), grid);
        state->speaking = true;
        state->speaker_is_host = is_host;
        state->speaker_cross_server = cross_server;
        state->last_speaker_ms = now_ms;
        return;
    }

    if (state->speaking && (!callsign[0] || strcmp(state->speaker, callsign) == 0)) {
        state->speaking = false;
        state->speaker_cross_server = false;
        state->speaker[0] = '\0';
        state->last_speaker_ms = now_ms;
    }
}

void fmo_monitor_set_position(fmo_monitor_state_t *state, bool valid, fmo_position_t position)
{
    if (state->position_valid != valid || (valid &&
        (state->position.lat_e6 != position.lat_e6 || state->position.lon_e6 != position.lon_e6)))
        state->location[0] = 0;
    state->position_valid = valid;
    state->position = position;
}

bool fmo_monitor_apply_location(fmo_monitor_state_t *state,
                                const fmo_monitor_state_t *request, const char *name)
{
    if (!name || !name[0] || !state->wifi_connected || !state->events_connected ||
        !state->last_speaker[0] || state->channel_uid != request->channel_uid ||
        strcmp(state->last_speaker, request->last_speaker) || strcmp(state->grid, request->grid) ||
        state->position_valid != request->position_valid ||
        (state->position_valid && (state->position.lat_e6 != request->position.lat_e6 ||
                                  state->position.lon_e6 != request->position.lon_e6)) ||
        !strcmp(state->location, name)) return false;
    return fmo_text_copy_utf8(state->location, sizeof(state->location), name);
}
