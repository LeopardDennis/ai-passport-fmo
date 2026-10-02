#include "fmo_monitor_state.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    fmo_monitor_state_t state;
    fmo_monitor_state_init(&state);
    assert(!state.wifi_connected);
    assert(!state.speaking);

    fmo_monitor_set_wifi(&state, true);
    fmo_monitor_set_events(&state, true);
    fmo_monitor_set_control(&state, true);
    fmo_monitor_set_channel(&state, 42, "LOCAL NET");
    assert(state.wifi_connected);
    assert(state.events_connected);
    assert(state.control_connected);
    assert(state.channel_uid == 42);
    assert(strcmp(state.channel_name, "LOCAL NET") == 0);

    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, true, 1000);
    assert(state.speaking);
    assert(state.speaker_is_host);
    assert(strcmp(state.speaker, "BG5ESN") == 0);
    assert(strcmp(state.last_speaker, "BG5ESN") == 0);
    assert(strcmp(state.grid, "PM00AA") == 0);

    /* A stale stop event for another callsign must not clear the active talker. */
    fmo_monitor_apply_speaker(&state, "BG1ABC", "", false, false, 1500);
    assert(state.speaking);

    fmo_monitor_apply_speaker(&state, "BG5ESN", "", false, false, 2000);
    assert(!state.speaking);
    assert(state.speaker[0] == '\0');
    assert(strcmp(state.last_speaker, "BG5ESN") == 0);
    assert(state.last_speaker_ms == 2000);

    /* Unnamed idle releases end the current talker without invalidating the
     * channel or overwriting last-heard metadata; duplicates do not reset age. */
    const char *releases[] = {"", NULL, "BG5ESN"};
    for (unsigned i = 0; i < sizeof(releases) / sizeof(releases[0]); ++i) {
        fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, true, 2100);
        fmo_monitor_apply_speaker(&state, releases[i], "", false, false, 2200);
        assert(!state.speaking && !state.speaker[0] && state.channel_valid);
        assert(state.channel_uid == 42 && !strcmp(state.last_speaker, "BG5ESN"));
        assert(!strcmp(state.grid, "PM00AA") && state.speaker_is_host);
        assert(state.last_speaker_ms == 2200);
        fmo_monitor_apply_speaker(&state, releases[i], "", false, false, 2300);
        assert(state.last_speaker_ms == 2200);
    }
    fmo_monitor_apply_speaker(&state, "", "", true, false, 2400);
    assert(!state.speaking && state.last_speaker_ms == 2200);

    /* Losing the events channel clears live state but keeps last-heard data. */
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "ON80", true, false, 3000);
    fmo_monitor_set_events(&state, false);
    assert(!state.speaking);
    assert(strcmp(state.last_speaker, "BI1XYZ") == 0);

    strcpy(state.radio.device_name,"TEST RADIO");
    strcpy(state.radio.antenna,"GP");
    state.radio.frequency_100hz=4398750;
    state.radio.antenna_height_m=63;state.radio.height_valid=true;
    fmo_monitor_set_wifi(&state, false);
    assert(!state.radio.device_name[0] && !state.radio.frequency_100hz && !state.radio.height_valid);
    assert(!state.events_connected);
    assert(!state.control_connected);
    assert(!state.channel_valid);
    assert(state.last_speaker[0] == '\0' && state.grid[0] == '\0');
    assert(state.last_speaker_ms == 0);
    assert(state.channel_uid == 0 && state.channel_name[0] == '\0');

    fmo_monitor_set_channel(&state, 42, "LOCAL NET");
    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, false, 4000);
    fmo_monitor_set_channel(&state, 42, "RENAMED");
    assert(state.speaking); /* Same channel refresh must retain the speaker. */
    strcpy(state.radio.device_name,"LOCAL RADIO");
    state.radio.frequency_100hz=4398750;
    fmo_monitor_set_channel(&state, 43, "OTHER NET");
    assert(!strcmp(state.radio.device_name,"LOCAL RADIO") && state.radio.frequency_100hz==4398750);
    assert(!state.speaking && state.speaker[0] == '\0');
    assert(state.last_speaker[0] == '\0' && state.grid[0] == '\0');
    strcpy(state.radio.device_name,"OTHER RADIO");
    state.radio.frequency_100hz=4398750;
    fmo_monitor_set_control(&state, false);
    assert(!state.radio.device_name[0] && !state.radio.frequency_100hz);
    assert(!state.channel_valid);
    fmo_monitor_set_channel(&state, 43, "OTHER NET");
    fmo_monitor_invalidate_channel(&state);
    assert(!state.channel_valid);
    return 0;
}
