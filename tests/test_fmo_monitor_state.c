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

    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, true, false, 1000);
    assert(state.speaking);
    assert(state.speaker_is_host);
    assert(strcmp(state.speaker, "BG5ESN") == 0);
    assert(strcmp(state.last_speaker, "BG5ESN") == 0);
    assert(strcmp(state.grid, "PM00AA") == 0);

    /* A stale stop event for another callsign must not clear the active talker. */
    fmo_monitor_apply_speaker(&state, "BG1ABC", "", false, false, false, 1500);
    assert(state.speaking);

    fmo_monitor_apply_speaker(&state, "BG5ESN", "", false, false, false, 2000);
    assert(!state.speaking);
    assert(state.speaker[0] == '\0');
    assert(strcmp(state.last_speaker, "BG5ESN") == 0);
    assert(state.last_speaker_ms == 2000);

    /* Unnamed idle releases end the current talker without invalidating the
     * channel or overwriting last-heard metadata; duplicates do not reset age. */
    const char *releases[] = {"", NULL, "BG5ESN"};
    for (unsigned i = 0; i < sizeof(releases) / sizeof(releases[0]); ++i) {
        fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, true, false, 2100);
        fmo_monitor_apply_speaker(&state, releases[i], "", false, false, false, 2200);
        assert(!state.speaking && !state.speaker[0] && state.channel_valid);
        assert(state.channel_uid == 42 && !strcmp(state.last_speaker, "BG5ESN"));
        assert(!strcmp(state.grid, "PM00AA") && state.speaker_is_host);
        assert(state.last_speaker_ms == 2200);
        fmo_monitor_apply_speaker(&state, releases[i], "", false, false, false, 2300);
        assert(state.last_speaker_ms == 2200);
    }
    fmo_monitor_apply_speaker(&state, "", "", true, false, false, 2400);
    assert(!state.speaking && state.last_speaker_ms == 2200);

    /* Losing the events channel clears live state but keeps last-heard data. */
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "ON80", true, false, false, 3000);
    fmo_monitor_set_events(&state, false);
    assert(!state.speaking);
    assert(strcmp(state.last_speaker, "BI1XYZ") == 0);

    state.history.count=1;
    fmo_monitor_set_wifi(&state, false);
    assert(!state.history.count);
    assert(!state.events_connected);
    assert(!state.control_connected);
    assert(!state.channel_valid);
    assert(state.last_speaker[0] == '\0' && state.grid[0] == '\0');
    assert(state.last_speaker_ms == 0);
    assert(state.channel_uid == 0 && state.channel_name[0] == '\0');

    fmo_monitor_set_channel(&state, 42, "LOCAL NET");
    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, false, false, 4000);
    fmo_monitor_set_channel(&state, 42, "RENAMED");
    assert(state.speaking); /* Same channel refresh must retain the speaker. */
    state.history.count=2;
    fmo_monitor_set_channel(&state, 43, "OTHER NET");
    assert(state.history.count==2); /* History has no channel UID; wait for the next FMO push. */
    assert(!state.speaking && state.speaker[0] == '\0');
    assert(state.last_speaker[0] == '\0' && state.grid[0] == '\0');
    fmo_monitor_set_control(&state, false);
    assert(state.history.count==2);
    assert(!state.channel_valid);
    fmo_monitor_set_channel(&state, 43, "OTHER NET");
    fmo_monitor_apply_speaker(&state, "BG5ESN", "PM00AA", true, false, false, 5000);
    fmo_monitor_apply_speaker(&state, "BG5ESN", "", false, false, false, 5100);
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "", true, false, false, 5200);
    assert(!state.grid[0]); /* A new talker without a grid clears the old one. */
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "ON80", true, false, false, 5300);
    fmo_monitor_apply_speaker(&state, "BI1XYZ", "", false, false, false, 5400);
    assert(!strcmp(state.grid, "ON80"));
    fmo_monitor_apply_speaker(&state, "BG1ABC", "PM11", true, false, false, 5500);
    assert(!strcmp(state.grid, "PM11") && !strcmp(state.last_speaker, "BG1ABC"));
    state.history.count=3;
    fmo_monitor_set_events(&state,false);
    assert(!state.history.count);
    fmo_monitor_invalidate_channel(&state);
    assert(!state.channel_valid);
    /* Cross-server color applies only to the active speaker. */
    fmo_monitor_set_wifi(&state,true);
    fmo_monitor_set_events(&state,true);
    fmo_monitor_set_channel(&state,99,"COLOR TEST");
    fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,6000);
    assert(state.speaker_cross_server);
    fmo_monitor_apply_speaker(&state,"OTHER","",false,false,false,6100);
    assert(state.speaking && state.speaker_cross_server);
    fmo_monitor_apply_speaker(&state,"BG5ESN","",false,false,true,6200);
    assert(!state.speaking && !state.speaker_cross_server);
    assert(!strcmp(state.grid,"PM01"));
    fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,6300);
    fmo_monitor_apply_speaker(&state,"BI1XYZ","ON80",true,false,false,6400);
    assert(!state.speaker_cross_server);
    fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,6500);
    fmo_monitor_set_events(&state,false);
    assert(!state.speaker_cross_server);
    fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,6600);
    fmo_monitor_set_channel(&state,100,"NEW CHANNEL");
    assert(!state.speaker_cross_server);
    fmo_monitor_apply_speaker(&state,"BG5ESN","PM01",true,false,true,6700);
    fmo_monitor_set_wifi(&state,false);
    assert(!state.speaker_cross_server);
    return 0;
}
