#include "fmo_stations.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    fmo_stations_t s = {0};
    fmo_station_t rows[7] = {{.uid=1},{.uid=2},{.uid=3},{.uid=4},{.uid=5},{.uid=6},{.uid=7}};
    assert(fmo_stations_load(&s, 0, 100));
    assert(!fmo_stations_load(&s, 6, 101));
    fmo_stations_list(&s, rows, 7); assert(!s.count); // Unsolicited reply.
    s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, rows, 7);
    assert(s.count == 6 && s.has_next && !s.audio_paused);
    assert(!fmo_stations_switch(&s, 9, 1, 200));
    assert(fmo_stations_switch(&s, 1, 1, 200));
    assert(s.status == FMO_STATIONS_SUCCESS && !s.audio_paused);
    assert(fmo_stations_switch(&s, 2, 1, 200));
    assert(s.audio_paused && !fmo_stations_switch(&s, 3, 1, 201));
    fmo_stations_current(&s, 2); assert(s.audio_paused); // Old query must not finish switch.
    s.request = FMO_STATION_SET_WAIT;
    fmo_stations_ack(&s, true); assert(s.status == FMO_STATIONS_SWITCHING);
    fmo_stations_current(&s, 1); assert(s.audio_paused);
    fmo_stations_current(&s, 2); assert(s.status == FMO_STATIONS_SUCCESS && !s.audio_paused);
    assert(fmo_stations_switch(&s, 3, 2, 1000));
    s.request = FMO_STATION_SET_WAIT;
    assert(!fmo_stations_expire(&s, 10999));
    assert(fmo_stations_expire(&s, 11000));
    assert(s.status == FMO_STATIONS_UNKNOWN && s.audio_paused);
    fmo_stations_ack(&s, true); assert(s.request == FMO_STATION_RECONCILE);
    assert(!fmo_stations_switch(&s, 3, 2, 12000));
    fmo_stations_current(&s, 2); assert(!s.audio_paused && s.status == FMO_STATIONS_UNKNOWN);
    assert(fmo_stations_switch(&s, 3, 2, 12000));
    s.request = FMO_STATION_SET_WAIT;
    fmo_stations_ack(&s, false); fmo_stations_current(&s, 2);
    assert(s.status == FMO_STATIONS_FAILED && !s.audio_paused);
    assert(fmo_stations_switch(&s, 3, 2, 13000));
    fmo_stations_disconnect(&s); assert(!s.audio_paused && s.status == FMO_STATIONS_FAILED);
    assert(fmo_stations_load(&s, 6, 14000)); s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, rows, 2); assert(!s.has_next && s.count == 2);
    assert(fmo_stations_load(&s, 12, 15000)); s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, rows, 0); assert(s.status == FMO_STATIONS_EMPTY);
    assert(fmo_stations_load(&s, 0, 16000)); s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, NULL, 8); assert(s.status == FMO_STATIONS_LOAD_FAILED);
    assert(!fmo_stations_load(&s, UINT32_MAX, 17000));
    puts("FMO station paging and switch lifecycle: PASS");
}
