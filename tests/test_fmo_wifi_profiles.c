#include "fmo_wifi_profiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    fmo_wifi_profiles_t profiles;
    fmo_wifi_profiles_init(&profiles);
    assert(fmo_wifi_profiles_valid(&profiles));
    int8_t rssi[5] = {-80, -40, -60, -128, -50};
    assert(fmo_wifi_profiles_pick(&profiles, 0, rssi, true) == -1);
    fmo_wifi_credential_t old = {.ssid = "legacy", .password = "test-only"};
    assert(fmo_wifi_profiles_put(&profiles, &old));
    assert(profiles.count == 1 && profiles.preferred == 0);
    for (int i = 1; i < 5; ++i) {
        fmo_wifi_credential_t entry = {.password = "test-only"};
        snprintf(entry.ssid, sizeof(entry.ssid), "network-%d", i);
        assert(fmo_wifi_profiles_put(&profiles, &entry));
    }
    fmo_wifi_profiles_t saved = profiles;
    fmo_wifi_credential_t extra = {.ssid = "sixth", .password = "test-only"};
    assert(!fmo_wifi_profiles_put(&profiles, &extra));
    assert(memcmp(&profiles, &saved, sizeof(saved)) == 0);
    strcpy(old.password, "updated-test");
    assert(fmo_wifi_profiles_put(&profiles, &old));
    assert(profiles.count == 5 && profiles.preferred == 0);
    assert(strcmp(profiles.entries[0].password, "updated-test") == 0);
    uint8_t tried = 0;
    const int expected[] = {0, 1, 4, 2, 3};
    for (unsigned i = 0; i < 5; ++i) {
        int picked = fmo_wifi_profiles_pick(&profiles, tried, rssi, i == 0);
        assert(picked == expected[i]);
        tried |= (uint8_t)(1u << picked);
    }
    assert(fmo_wifi_profiles_pick(&profiles, tried, rssi, false) == -1);
    assert(fmo_wifi_profiles_remove(&profiles, "legacy"));
    assert(fmo_wifi_profiles_find(&profiles, "legacy") == -1);
    assert(profiles.count == 4 && fmo_wifi_profiles_valid(&profiles));
    assert(fmo_wifi_profiles_put(&profiles, &extra));
    assert(profiles.preferred == 4);
    assert(fmo_wifi_profiles_remove(&profiles, "network-1"));
    assert(profiles.preferred == 3);
    while (profiles.count) {
        char ssid[33]; strcpy(ssid, profiles.entries[0].ssid);
        assert(fmo_wifi_profiles_remove(&profiles, ssid));
    }
    assert(fmo_wifi_profiles_valid(&profiles));
    assert(profiles.preferred == 0);
    assert(!fmo_wifi_profiles_remove(&profiles, "missing"));
    for (unsigned i = 0; i < sizeof(profiles.entries); ++i)
        assert(((unsigned char *)profiles.entries)[i] == 0);
    profiles.version = 2;
    assert(!fmo_wifi_profiles_valid(&profiles));
    profiles = saved; profiles.count = 6;
    assert(!fmo_wifi_profiles_valid(&profiles));
    profiles = saved; profiles.preferred = 5;
    assert(!fmo_wifi_profiles_valid(&profiles));
    profiles = saved; memset(profiles.entries[0].ssid, 'x', 33);
    assert(!fmo_wifi_profiles_valid(&profiles));
    profiles = saved; profiles.entries[1] = profiles.entries[0];
    assert(!fmo_wifi_profiles_valid(&profiles));
    puts("Wi-Fi profiles: PASS (capacity, update, ordering, deletion, validation)");
    return 0;
}
