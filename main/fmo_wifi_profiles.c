#include "fmo_wifi_profiles.h"
#include "fmo_credentials.h"
#include <string.h>

static bool valid_entry(const fmo_wifi_credential_t *entry)
{
    return entry && entry->ssid[32] == 0 && entry->password[63] == 0 &&
        fmo_credentials_valid(entry->ssid, strlen(entry->ssid),
                              entry->password, strlen(entry->password));
}

void fmo_wifi_profiles_init(fmo_wifi_profiles_t *profiles)
{
    memset(profiles, 0, sizeof(*profiles));
    profiles->version = 2;
}

int fmo_wifi_profiles_find(const fmo_wifi_profiles_t *profiles, const char *ssid)
{
    if (!profiles || !ssid || profiles->count > FMO_WIFI_PROFILE_MAX) return -1;
    for (unsigned i = 0; i < profiles->count; ++i)
        if (strcmp(profiles->entries[i].ssid, ssid) == 0) return (int)i;
    return -1;
}

bool fmo_wifi_profiles_valid(const fmo_wifi_profiles_t *profiles)
{
    if (!profiles || profiles->version != 2 || profiles->reserved != 0 ||
        profiles->count > FMO_WIFI_PROFILE_MAX ||
        (profiles->count ? profiles->preferred >= profiles->count : profiles->preferred != 0)) return false;
    for (unsigned i = 0; i < profiles->count; ++i) {
        if (!valid_entry(&profiles->entries[i])) return false;
        const fmo_endpoint_t *endpoint = &profiles->endpoints[i];
        if ((endpoint->host[0] || endpoint->port) && !fmo_endpoint_valid(endpoint)) return false;
        for (unsigned j = 0; j < i; ++j)
            if (strcmp(profiles->entries[i].ssid, profiles->entries[j].ssid) == 0) return false;
    }
    return true;
}

bool fmo_wifi_profiles_put(fmo_wifi_profiles_t *profiles, const fmo_wifi_credential_t *entry)
{
    if (!fmo_wifi_profiles_valid(profiles) || !valid_entry(entry)) return false;
    int i = fmo_wifi_profiles_find(profiles, entry->ssid);
    if (i < 0) {
        if (profiles->count == FMO_WIFI_PROFILE_MAX) return false;
        i = profiles->count++;
    }
    profiles->entries[i] = *entry;
    profiles->preferred = (uint8_t)i;
    return true;
}

bool fmo_wifi_profiles_remove(fmo_wifi_profiles_t *profiles, const char *ssid)
{
    if (!fmo_wifi_profiles_valid(profiles)) return false;
    int i = fmo_wifi_profiles_find(profiles, ssid);
    if (i < 0) return false;
    memmove(&profiles->entries[i], &profiles->entries[i + 1],
            (profiles->count - (unsigned)i - 1) * sizeof(profiles->entries[0]));
    memmove(&profiles->endpoints[i], &profiles->endpoints[i + 1],
            (profiles->count - (unsigned)i - 1) * sizeof(profiles->endpoints[0]));
    memset(&profiles->endpoints[profiles->count - 1], 0, sizeof(profiles->endpoints[0]));
    memset(&profiles->entries[--profiles->count], 0, sizeof(profiles->entries[0]));
    if (profiles->preferred == i) profiles->preferred = 0;
    else if (profiles->preferred > i) --profiles->preferred;
    return true;
}

int fmo_wifi_profiles_pick(const fmo_wifi_profiles_t *profiles, uint8_t tried,
                           const int8_t rssi[FMO_WIFI_PROFILE_MAX], bool prefer_last)
{
    if (!fmo_wifi_profiles_valid(profiles) || !profiles->count || !rssi) return -1;
    if (prefer_last && !(tried & (1u << profiles->preferred))) return profiles->preferred;
    int best = -1;
    for (unsigned i = 0; i < profiles->count; ++i) {
        if (tried & (1u << i)) continue;
        if (best < 0 || rssi[i] > rssi[best]) best = (int)i;
    }
    return best;
}

/* V1 had one global destination. Apply it to every existing network, preserving
 * the behavior of the old firmware, then allow independent edits in V2. */
bool fmo_wifi_profiles_migrate(fmo_wifi_profiles_t *out,
    const fmo_wifi_profiles_v1_t *old, const fmo_endpoint_t *endpoint)
{
    if (!out || !old || old->version != 1 || !fmo_endpoint_valid(endpoint)) return false;
    fmo_wifi_profiles_init(out);
    out->count = old->count;
    out->preferred = old->preferred;
    out->reserved = old->reserved;
    memcpy(out->entries, old->entries, sizeof(out->entries));
    if (!fmo_wifi_profiles_valid(out)) { fmo_wifi_profiles_init(out); return false; }
    for (unsigned i = 0; i < out->count; ++i) out->endpoints[i] = *endpoint;
    return true;
}
