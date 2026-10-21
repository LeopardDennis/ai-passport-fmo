#include "fmo_location.h"
#include "fmo_monitor_state.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    fmo_position_t p;
    assert(fmo_location_grid("PM01", &p));
    assert(p.lat_e6 == 31500000 && p.lon_e6 == 121000000);
    assert(fmo_location_grid("pm01aa", &p));
    assert(p.lat_e6 == 31020833 && p.lon_e6 == 120041667);
    assert(fmo_location_grid("PM01aa00", &p));
    assert(p.lat_e6 == 31002083 && p.lon_e6 == 120004167);
    const char *bad[] = {"", "PM", "PM011", "SM01AA", "PM01YY", "PM01A1", "PM01AAx0", "PM01ABC1234"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) assert(!fmo_location_grid(bad[i],&p));
    assert(!fmo_location_coordinates(NAN,0,&p));
    assert(!fmo_location_coordinates(91,0,&p));
    assert(!fmo_location_coordinates(0,INFINITY,&p));
    assert(!fmo_location_coordinates(0,181,&p));
    assert(fmo_location_coordinates(-90,-180,&p));
    assert(fmo_location_coordinates(31.23,121.47,&p));
    assert(p.lat_e6==31230000 && p.lon_e6==121470000);

    double km, bearing;
    assert(fmo_location_path((fmo_position_t){0,0}, (fmo_position_t){0,1000000}, &km, &bearing));
    assert(fabs(km - 111.19508) < 0.001 && fabs(bearing - 90) < 0.000001);
    assert(fmo_location_path((fmo_position_t){0,0}, (fmo_position_t){1000000,0}, &km, &bearing));
    assert(fabs(bearing) < 0.000001);
    assert(fmo_location_path((fmo_position_t){0,0}, (fmo_position_t){-1000000,0}, &km, &bearing));
    assert(fabs(bearing - 180) < 0.000001);
    assert(fmo_location_path((fmo_position_t){0,0}, (fmo_position_t){0,-1000000}, &km, &bearing));
    assert(fabs(bearing - 270) < 0.000001);
    /* Cross the antimeridian on the short eastward path. */
    assert(fmo_location_path((fmo_position_t){0,179000000}, (fmo_position_t){0,-179000000}, &km, &bearing));
    assert(fabs(km - 222.39016) < 0.001 && fabs(bearing - 90) < 0.000001);
    assert(fmo_location_path(p, p, &km, &bearing));
    assert(km == 0 && isnan(bearing));
    assert(fmo_location_path((fmo_position_t){0,0}, (fmo_position_t){0,180000000}, &km, &bearing));
    assert(fabs(km - 20015.11444) < 0.001 && isnan(bearing));
    assert(fmo_location_path((fmo_position_t){90000000,0}, (fmo_position_t){80000000,0}, &km, &bearing));
    assert(isfinite(km) && isnan(bearing));
    assert(!fmo_location_path((fmo_position_t){91000000,0}, p, &km, &bearing));
    assert(!fmo_location_path(p, (fmo_position_t){0,181000000}, &km, &bearing));
    assert(!fmo_location_path(p, p, NULL, &bearing));

    const char *full[] = {"内蒙古自治区", "广西壮族自治区", "宁夏回族自治区",
        "新疆维吾尔自治区", "西藏自治区", "香港特别行政区", "澳门特别行政区",
        "佛山市", "广东省", "呼和浩特市", "内蒙古自治区呼和浩特市", "New York", "--", ""};
    const char *short_names[] = {"内蒙古", "广西", "宁夏", "新疆", "西藏", "香港", "澳门",
        "佛山", "广东", "呼和浩特", "内蒙古呼和浩特", "New York", "--", ""};
    for (unsigned i=0;i<sizeof(full)/sizeof(full[0]);++i) {
        char short_name[FMO_LOCATION_MAX+1];
        fmo_location_short_name(full[i],short_name,sizeof(short_name));
        assert(!strcmp(short_name,short_names[i]));
    }
    char tiny[5];
    fmo_location_short_name("内蒙古自治区",tiny,sizeof(tiny));
    assert(!strcmp(tiny,"内"));
    fmo_location_short_name(NULL,tiny,sizeof(tiny));
    assert(!tiny[0]);
    fmo_location_cache_t sparse={0};
    sparse.entries[0]=(fmo_location_entry_t){.position=p,.name="上海市"};
    sparse.occupied[0]=true;
    fmo_position_t fresh={123,456};
    assert(fmo_location_store(&sparse,fresh,"",100)==1);
    assert(!strcmp(fmo_location_find(&sparse,p,100)->name,"上海市"));
    fmo_location_cache_t cache={0};
    assert(!fmo_location_find(&cache,p,0));
    fmo_location_store(&cache,p,"上海市",0);
    assert(!strcmp(fmo_location_find(&cache,p,999999)->name,"上海市"));
    assert(fmo_location_entry_valid(fmo_location_find(&cache,p,0)));
    fmo_position_t failure={123,456};
    fmo_location_store(&cache,failure,"",100);
    assert(fmo_location_find(&cache,failure,30099));
    assert(!fmo_location_find(&cache,failure,30100));
    for (unsigned i=0;i<FMO_LOCATION_CACHE_SIZE;++i)
        fmo_location_store(&cache,(fmo_position_t){(int)i,0},"佛山市",500);
    assert(!fmo_location_find(&cache,p,0));
    fmo_location_entry_t corrupt={.position={91000000,0},.name="x"};
    assert(!fmo_location_entry_valid(&corrupt));
    corrupt.position.lat_e6=0; memset(corrupt.name,'a',sizeof(corrupt.name));
    assert(!fmo_location_entry_valid(&corrupt));
    strcpy(corrupt.name,"\xff"); assert(!fmo_location_entry_valid(&corrupt));

    char name[49];
    assert(fmo_location_parse_response("{\"features\":[{\"properties\":{\"city\":\"黄浦区\",\"state\":\"上海市\"}}]}",name,sizeof(name)));
    assert(!strcmp(name,"上海市 黄浦区"));
    assert(fmo_location_parse_response("{\"features\":[{\"properties\":{\"city\":\"佛山市\",\"state\":\"广东省\"}}]}",name,sizeof(name)));
    assert(!strcmp(name,"佛山市"));
    const char *places[][2] = {
        {"{\"city\":\"佛山市\",\"county\":\"南海区\",\"district\":\"桂城街道\"}", "佛山市 南海区"},
        {"{\"city\":\"南海区\",\"county\":\"佛山市\"}", "佛山市 南海区"},
        {"{\"city\":\"上海市\",\"county\":\"上海市\",\"district\":\"黄浦区\",\"state\":\"上海市\"}", "上海市 黄浦区"},
        {"{\"city\":\"市辖区\",\"district\":\"黄浦区\",\"state\":\"上海市\"}", "上海市 黄浦区"},
        {"{\"city\":\"佛山市\",\"county\":\"佛山市\"}", "佛山市"},
        {"{\"county\":\"南海区\"}", "南海区"},
        {"{\"state\":\"内蒙古自治区\"}", "内蒙古自治区"},
    };
    for (unsigned i=0;i<sizeof(places)/sizeof(places[0]);++i) {
        char json[512];snprintf(json,sizeof(json),"{\"features\":[{\"properties\":%s}]}",places[i][0]);
        assert(fmo_location_parse_response(json,name,sizeof(name)) && !strcmp(name,places[i][1]));
    }
    fmo_location_short_name("佛山市 南海区",name,sizeof(name));assert(!strcmp(name,"佛山 南海"));
    fmo_location_short_name("上海市 黄浦区",name,sizeof(name));assert(!strcmp(name,"上海 黄浦"));
    fmo_location_short_name("New York Kings County",name,sizeof(name));assert(!strcmp(name,"New York Kings County"));

    assert(!fmo_location_parse_response("{\"features\":[{\"properties\":{\"name\":\"商场\"}}]}",name,sizeof(name)));
    const char *bad_json[]={"{}","[]","null","{","{\"features\":[]}","{}trailing","{\"features\":[{\"properties\":{\"city\":\"A\\u0000B\"}}]}"};
    for(unsigned i=0;i<sizeof(bad_json)/sizeof(bad_json[0]);++i) assert(!fmo_location_parse_response(bad_json[i],name,sizeof(name)));

    char deep[80];
    memset(deep, '[', 20);memset(deep+20, ']', 20);deep[40]=0;
    assert(!fmo_location_parse_response(deep,name,sizeof(name)));

    fmo_monitor_state_t state;
    fmo_monitor_state_init(&state);
    fmo_monitor_set_wifi(&state,true); fmo_monitor_set_events(&state,true);
    fmo_monitor_set_channel(&state,42,"channel");
    fmo_monitor_apply_speaker(&state,"TEST","PM01AA",true,false,false,1);
    fmo_monitor_set_position(&state,true,p);
    fmo_monitor_state_t request=state;
    fmo_monitor_apply_speaker(&state,"TEST","",false,false,false,2);
    assert(fmo_monitor_apply_location(&state,&request,"上海市")); // Short PTT finished.
    assert(!state.speaking && !strcmp(state.location,"上海市"));
    assert(!fmo_monitor_apply_location(&state,&request,"上海市")); // No duplicate publication.
    fmo_monitor_apply_speaker(&state,"OTHER","PM01AA",true,false,false,3);
    assert(!state.location[0]);
    assert(!fmo_monitor_apply_location(&state,&request,"上海市"));
    state=request; fmo_monitor_set_position(&state,true,(fmo_position_t){1,2});
    assert(!fmo_monitor_apply_location(&state,&request,"上海市"));
    state=request; fmo_monitor_set_channel(&state,99,"other");
    assert(!state.location[0] && !state.position_valid);
    assert(!fmo_monitor_apply_location(&state,&request,"上海市"));
    state=request; fmo_monitor_set_events(&state,false);
    assert(!fmo_monitor_apply_location(&state,&request,"上海市"));
    state=request; fmo_monitor_set_wifi(&state,false);
    assert(!fmo_monitor_apply_location(&state,&request,"上海市"));
    puts("FMO location: PASS (grid/coordinate validation, bounded cache, JSON, stale reply isolation)");
}
