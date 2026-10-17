#include "fmo_location.h"
#include "fmo_text.h"
#include "cJSON.h"
#include <string.h>

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
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(props, "state");
    /* Chinese municipalities often expose a district in Photon's city field. */
    if (cJSON_IsString(state) &&
        (!strcmp(state->valuestring, "北京市") || !strcmp(state->valuestring, "上海市") ||
         !strcmp(state->valuestring, "天津市") || !strcmp(state->valuestring, "重庆市")))
        fmo_text_copy_utf8(name, capacity, state->valuestring);
    const char *fields[] = {"city", "county", "district", "state", "country"};
    for (unsigned i = 0; !name[0] && i < sizeof(fields) / sizeof(fields[0]); ++i) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(props, fields[i]);
        if (cJSON_IsString(value)) fmo_text_copy_utf8(name, capacity, value->valuestring);
    }
    /* Do not mistake a nearby POI's name for an administrative place. */
    cJSON_Delete(root);
    return name[0] != 0;
}
