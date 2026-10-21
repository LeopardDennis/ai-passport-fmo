#include "fmo_stations.h"
#include <assert.h>
#include <stdio.h>
static void verify_total(unsigned total)
{
    fmo_stations_t s = {0};
    fmo_station_t row = {.uid=42};
    assert(fmo_stations_load(&s,0,100));
    fmo_stations_begin_count(&s,100);
    unsigned queries=0;
    while (s.counting) {
        assert(s.request==FMO_STATION_LIST_QUEUED && !s.count && !s.audio_paused);
        fmo_stations_prepare_query(&s,100+queries*10);
        assert(++queries<=32);
        s.request=FMO_STATION_LIST_WAIT;
        fmo_stations_list(&s,&row,s.count_probe<total ? 1 : 0);
    }
    assert(s.request==FMO_STATION_LIST_QUEUED);
    if (total<=FMO_STATION_COUNT_LIMIT) assert(s.total_known && s.total==total);
    else assert(!s.total_known);
}

int main(void)
{
    const unsigned totals[]={0,1,6,7,162,163,65535,65536};
    for(unsigned i=0;i<sizeof(totals)/sizeof(totals[0]);++i) verify_total(totals[i]);
    fmo_stations_t probe={0};
    assert(fmo_stations_load(&probe,0,100));fmo_stations_begin_count(&probe,100);
    fmo_stations_prepare_query(&probe,3100);
    assert(!probe.counting && !probe.total_known && probe.request==FMO_STATION_LIST_QUEUED);
    fmo_stations_begin_count(&probe,3200);probe.count_queries=32;
    fmo_stations_prepare_query(&probe,3201);assert(!probe.counting);
    fmo_stations_begin_count(&probe,3300);probe.request=FMO_STATION_LIST_WAIT;
    fmo_stations_list(&probe,NULL,2);
    assert(!probe.counting && !probe.total_known && probe.request==FMO_STATION_LIST_QUEUED);
    fmo_stations_begin_count(&probe,3400);probe.request=FMO_STATION_LIST_WAIT;
    fmo_stations_disconnect(&probe);assert(!probe.counting && !probe.total_known);
    probe.request=FMO_STATION_NONE;probe.total=162;probe.total_known=true;
    assert(fmo_stations_load(&probe,6,3500));fmo_stations_begin_count(&probe,3500);
    assert(!probe.counting && probe.total_known); // Reuse the total between pages.
    probe.request=FMO_STATION_NONE;
    assert(fmo_stations_load(&probe,0,3600));fmo_stations_begin_count(&probe,3600);
    assert(probe.counting && !probe.total_known); // Refresh on reopening the first page.
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
    fmo_stations_list(&s, rows, 2); assert(!s.has_next && s.count == 2 && s.total_known && s.total==8);
    assert(fmo_stations_load(&s, 12, 15000)); s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, rows, 0); assert(s.status == FMO_STATIONS_EMPTY);
    assert(fmo_stations_load(&s, 0, 16000)); s.request = FMO_STATION_LIST_WAIT;
    fmo_stations_list(&s, NULL, 8); assert(s.status == FMO_STATIONS_LOAD_FAILED);
    assert(!fmo_stations_load(&s, UINT32_MAX, 17000));
    puts("FMO station paging, bounded total count and switch lifecycle: PASS");
}
