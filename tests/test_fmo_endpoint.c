#include "fmo_endpoint.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    const char *good[] = {"fmo.local", "fmo", "192.168.9.2", "radio-2.example.com", "FMO.local"};
    fmo_endpoint_t endpoint = {.port=80};
    for (unsigned i=0;i<sizeof(good)/sizeof(good[0]);++i) {
        strcpy(endpoint.host,good[i]);assert(fmo_endpoint_valid(&endpoint));
    }
    const char *bad[] = {"", "ws://fmo.local", "fmo.local:80", "fmo.local/events",
        "fmo@local", "-fmo.local", "fmo-.local", ".fmo.local", "fmo..local",
        "fmo.local.", "999.1.1.1", "192.168.1", "[::1]", "fmo local"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        strcpy(endpoint.host,bad[i]);assert(!fmo_endpoint_valid(&endpoint));
    }
    memset(endpoint.host,'a',64);endpoint.host[64]=0;assert(!fmo_endpoint_valid(&endpoint));
    memset(endpoint.host,'a',sizeof(endpoint.host));assert(!fmo_endpoint_valid(&endpoint));
    memset(endpoint.host,0,sizeof(endpoint.host));
    strcpy(endpoint.host,"fmo.local");endpoint.port=0;assert(!fmo_endpoint_valid(&endpoint));
    endpoint.port=65535;assert(fmo_endpoint_valid(&endpoint));
    assert(!fmo_endpoint_valid(NULL));
    return 0;
}
