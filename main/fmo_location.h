#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FMO_LOCATION_MAX 48
#define FMO_LOCATION_CACHE_SIZE 32

typedef struct { int32_t lat_e6, lon_e6; } fmo_position_t;
typedef struct {
    fmo_position_t position;
    char name[FMO_LOCATION_MAX + 1];
} fmo_location_entry_t;
typedef struct {
    fmo_location_entry_t entries[FMO_LOCATION_CACHE_SIZE];
    uint64_t retry_at[FMO_LOCATION_CACHE_SIZE];
    bool occupied[FMO_LOCATION_CACHE_SIZE];
    unsigned next;
} fmo_location_cache_t;

/* WGS84 decimal degrees, or the center of a 4/6/8-character Maidenhead cell. */
bool fmo_location_coordinates(double lat, double lon, fmo_position_t *out);
bool fmo_location_grid(const char *grid, fmo_position_t *out);
/* Great-circle distance and initial true-north bearing from origin to target.
 * An undefined bearing (coincident/antipodal points or polar origin) is NAN. */
bool fmo_location_path(fmo_position_t origin, fmo_position_t target,
                       double *distance_km, double *bearing_deg);
/* NULL = miss; empty name = negative cache hit. */
const fmo_location_entry_t *fmo_location_find(const fmo_location_cache_t *cache,
                                            fmo_position_t position, uint64_t now);
unsigned fmo_location_store(fmo_location_cache_t *cache, fmo_position_t position,
                            const char *name, uint64_t now);
bool fmo_location_entry_valid(const fmo_location_entry_t *entry);
/* Photon JSON parser; no network or platform dependencies. */
bool fmo_location_parse_response(const char *json, char *name, size_t capacity);

/* Compact display name; the original geocoder/cache value is unchanged. */
void fmo_location_short_name(const char *name, char *out, size_t capacity);
