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

bool fmo_location_path(fmo_position_t origin, fmo_position_t target,
                       double *distance_km, double *bearing_deg)
{
    if (!distance_km || !bearing_deg) return false;
    *distance_km = *bearing_deg = NAN;
    if (origin.lat_e6 < -90000000 || origin.lat_e6 > 90000000 ||
        target.lat_e6 < -90000000 || target.lat_e6 > 90000000 ||
        origin.lon_e6 < -180000000 || origin.lon_e6 > 180000000 ||
        target.lon_e6 < -180000000 || target.lon_e6 > 180000000) return false;
    const double radians = 0.017453292519943295;
    double lat1 = origin.lat_e6 * radians / 1000000.0;
    double lat2 = target.lat_e6 * radians / 1000000.0;
    double delta_lat = lat2 - lat1;
    double delta_lon = ((double)target.lon_e6 - origin.lon_e6) * radians / 1000000.0;
    double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
        cos(lat1) * cos(lat2) * sin(delta_lon / 2) * sin(delta_lon / 2);
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    *distance_km = 2 * 6371.0088 * atan2(sqrt(a), sqrt(1 - a));
    double y = sin(delta_lon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(delta_lon);
    if (fabs(cos(lat1)) > 1e-12 && hypot(x, y) > 1e-12)
        *bearing_deg = fmod(atan2(y, x) / radians + 360, 360);
    return true;
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
    bool existing = false;
    for (unsigned i = 0; i < FMO_LOCATION_CACHE_SIZE; ++i)
        if (cache->occupied[i] && equal(cache->entries[i].position, p)) {
            slot = i; existing = true; break;
        }
    /* A reboot may restore a sparse cache. Fill holes before evicting a
     * persisted place; a failed new lookup must not discard that cache hit. */
    if (!existing) for (unsigned i = 0; i < FMO_LOCATION_CACHE_SIZE; ++i) {
        unsigned candidate = (cache->next + i) % FMO_LOCATION_CACHE_SIZE;
        if (!cache->occupied[candidate]) { slot = candidate; break; }
    }
    memset(&cache->entries[slot], 0, sizeof(cache->entries[slot]));
    cache->entries[slot].position = p;
    fmo_text_copy_utf8(cache->entries[slot].name, sizeof(cache->entries[slot].name), name);
    cache->retry_at[slot] = now + FMO_LOCATION_RETRY_MS;
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

void fmo_location_short_name(const char *name, char *out, size_t capacity)
{
    if (!out || !capacity) return;
    char compact[FMO_LOCATION_MAX + 1];
    if (!fmo_text_copy_utf8(compact, sizeof(compact), name)) compact[0] = 0;
    static const struct { const char *full, *short_name; } regions[] = {
        {"内蒙古自治区", "内蒙古"}, {"广西壮族自治区", "广西"},
        {"宁夏回族自治区", "宁夏"}, {"新疆维吾尔自治区", "新疆"},
        {"西藏自治区", "西藏"}, {"香港特别行政区", "香港"},
        {"澳门特别行政区", "澳门"},
    };
    for (unsigned i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        size_t full = strlen(regions[i].full), short_len = strlen(regions[i].short_name);
        if (!strncmp(compact, regions[i].full, full)) {
            memmove(compact + short_len, compact + full, strlen(compact + full) + 1);
            memcpy(compact, regions[i].short_name, short_len);
            break;
        }
    }
    bool combined = strchr(compact, ' ') != NULL;
    char *part = compact;
    while (*part) {
        char *end = strchr(part, ' ');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (length > 3 && (!strncmp(part + length - 3, "市", 3) ||
                           !strncmp(part + length - 3, "省", 3) ||
                           (combined && (!strncmp(part + length - 3, "区", 3) ||
                                         !strncmp(part + length - 3, "县", 3))))) {
            memmove(part + length - 3, part + length, strlen(part + length) + 1);
            if (end) end -= 3;
        }
        if (!end) break;
        part = end + 1;
    }
    fmo_text_copy_utf8(out, capacity, compact);
}
