#pragma once
#include <stdbool.h>
#include <stdint.h>
#define FMO_WIFI_PROFILE_MAX 5
typedef struct { char ssid[33]; char password[64]; } fmo_wifi_credential_t;
typedef struct {
    uint8_t version;
    uint8_t count;
    uint8_t preferred;
    uint8_t reserved;
    fmo_wifi_credential_t entries[FMO_WIFI_PROFILE_MAX];
} fmo_wifi_profiles_t;
bool fmo_wifi_profiles_valid(const fmo_wifi_profiles_t *profiles);
void fmo_wifi_profiles_init(fmo_wifi_profiles_t *profiles);
int fmo_wifi_profiles_find(const fmo_wifi_profiles_t *profiles, const char *ssid);
bool fmo_wifi_profiles_put(fmo_wifi_profiles_t *profiles, const fmo_wifi_credential_t *entry);
bool fmo_wifi_profiles_remove(fmo_wifi_profiles_t *profiles, const char *ssid);
/* RSSI -128 denotes absent/hidden. Try preferred first only when requested;
 * subsequently try strongest untried visible entry, then hidden entries. */
int fmo_wifi_profiles_pick(const fmo_wifi_profiles_t *profiles, uint8_t tried,
                           const int8_t rssi[FMO_WIFI_PROFILE_MAX], bool prefer_last);
