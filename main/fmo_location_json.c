#include "fmo_location.h"
#include "fmo_text.h"
#include "cJSON.h"
#include <string.h>

static bool suffix(const char *name, const char *ending)
{
    size_t n = name ? strlen(name) : 0, e = strlen(ending);
    return n >= e && !strcmp(name + n - e, ending);
}

static const char *place_field(const cJSON *props, const char *key)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(props, key);
    char checked[FMO_LOCATION_MAX + 1];
    if (!cJSON_IsString(value) || !value->valuestring[0] ||
        !fmo_text_copy_utf8(checked, sizeof(checked), value->valuestring) ||
        !strcmp(value->valuestring, "市辖区") || !strcmp(value->valuestring, "市辖县")) return NULL;
    /* Photon field names do not guarantee administrative level. In China,
     * district (and sometimes city/county) can describe a subdistrict/town. */
    static const char *local_suffixes[] = {"街道", "街道办事处", "镇", "乡", "苏木", "村", "社区"};
    for (unsigned i = 0; i < sizeof(local_suffixes) / sizeof(local_suffixes[0]); ++i)
        if (suffix(value->valuestring, local_suffixes[i])) return NULL;
    return value->valuestring;
}

static const char *municipality_name(const char *name)
{
    static const char *names[] = {"北京市", "上海市", "天津市", "重庆市"};
    if (!name) return NULL;
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        size_t short_length = strlen(names[i]) - strlen("市");
        if (!strcmp(name, names[i]) ||
            (strlen(name) == short_length && !strncmp(name, names[i], short_length)))
            return names[i];
    }
    return NULL;
}

static const char *different(const char *name, const char *city)
{
    if (!name || (city && !strcmp(name, city))) return NULL;
    const char *municipality = municipality_name(name);
    const char *other = municipality_name(city);
    if (municipality && other && !strcmp(municipality, other)) return NULL;
    return name;
}

bool fmo_location_parse_response(const char *json, char *name, size_t capacity)
{
    if (!name || !capacity) return false;
    name[0] = 0;
    if (!json || fmo_text_json_has_nul(json)) return false;
    unsigned depth = 0;
    bool quoted = false, escaped = false;
    for (const char *p = json; *p; ++p) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (*p == '\\') escaped = true;
            else if (*p == '"') quoted = false;
        } else if (*p == '"') quoted = true;
        else if (*p == '{' || *p == '[') { if (++depth > 16) return false; }
        else if (*p == '}' || *p == ']') { if (!depth) return false; --depth; }
    }
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    if (!root) return false;
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    const cJSON *feature = cJSON_IsArray(features) ? cJSON_GetArrayItem(features, 0) : NULL;
    const cJSON *props = cJSON_GetObjectItemCaseSensitive(feature, "properties");
    const char *state = place_field(props, "state");
    const char *city = place_field(props, "city");
    const char *county = place_field(props, "county");
    const char *district = place_field(props, "district");
    const char *area = NULL;
    /* Photon may put a municipality's district in city, or a prefecture
     * in county. Keep the city first instead of showing only its district. */
    const char *municipality = municipality_name(state);
    if (!municipality) municipality = municipality_name(county);
    if (!municipality) municipality = municipality_name(city);
    if (municipality) {
        area = different(county, municipality);
        if (!area) area = different(city, municipality);
        city = municipality;
    } else if (county && (suffix(county, "市") || suffix(county, "州") || suffix(county, "盟")) &&
               (!city || suffix(city, "区") || suffix(city, "县") || suffix(city, "旗"))) {
        area = city;
        city = county;
    } else area = different(county, city);
    if (!area) area = different(district, city);
    if (!city) { city = county ? county : district ? district : state; area = NULL; }
    if (!city) city = place_field(props, "country");
    if (city) fmo_text_copy_utf8(name, capacity, city);
    if (name[0] && area && strcmp(area, city)) {
        size_t used = strlen(name);
        if (used + 2 < capacity) {
            name[used] = ' ';
            fmo_text_copy_utf8(name + used + 1, capacity - used - 1, area);
            if (!name[used + 1]) name[used] = 0;
        }
    }
    /* Do not mistake a nearby POI's name for an administrative place. */
    cJSON_Delete(root);
    return name[0] != 0;
}
