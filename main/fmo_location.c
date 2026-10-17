#include "fmo_location.h"
#include "fmo_text.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

bool fmo_location_coordinates(double lat, double lon, fmo_position_t *out)
{
    if (!out || !isfinite(lat) || !isfinite(lon) || lat < -90 || lat > 90 ||
        lon < -180 || lon > 180) return false;
    out->lat_e6 = (int32_t)llround(lat * 1000000);
    out->lon_e6 = (int32_t)llround(lon * 1000000);
    return true;
}

bool fmo_location_grid(const char *grid, fmo_position_t *out)
{
    if (!grid || !out) return false;
    size_t n = strlen(grid);
    if (n != 4 && n != 6 && n != 8) return false;
    double lon = -180, lat = -90, dx = 20, dy = 10;
    for (size_t i = 0; i < n; i += 2) {
        int base = i == 0 || i == 4 ? 'A' : '0';
        int limit = i == 0 ? 18 : i == 4 ? 24 : 10;
        int x = toupper((unsigned char)grid[i]) - base;
        int y = toupper((unsigned char)grid[i + 1]) - base;
        if (x < 0 || y < 0 || x >= limit || y >= limit) return false;
        if (i) { dx /= limit; dy /= limit; }
        lon += x * dx; lat += y * dy;
    }
    return fmo_location_coordinates(lat + dy / 2, lon + dx / 2, out);
}

static bool equal(fmo_position_t a, fmo_position_t b)
{
    return a.lat_e6 == b.lat_e6 && a.lon_e6 == b.lon_e6;
}

const fmo_location_entry_t *fmo_location_find(const fmo_location_cache_t *cache,
                                            fmo_position_t p, uint64_t now)
{
    for (unsigned i = 0; i < FMO_LOCATION_CACHE_SIZE; ++i)
        if (cache->occupied[i] && equal(cache->entries[i].position, p) &&
            (cache->entries[i].name[0] || now < cache->retry_at[i])) return &cache->entries[i];
    return NULL;
}

unsigned fmo_location_store(fmo_location_cache_t *cache, fmo_position_t p,
                            const char *name, uint64_t now)
{
    unsigned slot = cache->next;
    for (unsigned i = 0; i < FMO_LOCATION_CACHE_SIZE; ++i)
        if (cache->occupied[i] && equal(cache->entries[i].position, p)) { slot = i; break; }
    memset(&cache->entries[slot], 0, sizeof(cache->entries[slot]));
    cache->entries[slot].position = p;
    fmo_text_copy_utf8(cache->entries[slot].name, sizeof(cache->entries[slot].name), name);
    cache->retry_at[slot] = now + 300000;
    cache->occupied[slot] = true;
    cache->next = (slot + 1) % FMO_LOCATION_CACHE_SIZE;
    return slot;
}

bool fmo_location_entry_valid(const fmo_location_entry_t *entry)
{
    char checked[FMO_LOCATION_MAX + 1];
    fmo_position_t p = entry->position;
    return p.lat_e6 >= -90000000 && p.lat_e6 <= 90000000 &&
           p.lon_e6 >= -180000000 && p.lon_e6 <= 180000000 &&
           memchr(entry->name, 0, sizeof(entry->name)) && entry->name[0] &&
           fmo_text_copy_utf8(checked, sizeof(checked), entry->name);
}
