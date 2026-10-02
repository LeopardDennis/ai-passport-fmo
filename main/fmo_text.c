#include "fmo_text.h"
#include <stdint.h>
#include <string.h>

bool fmo_text_copy_utf8(char *destination, size_t capacity, const char *source)
{
    if (!destination || !capacity) return false;
    destination[0] = '\0';
    if (!source) return false;
    size_t used = 0;
    bool fits = true;
    const unsigned char *p = (const unsigned char *)source;
    while (*p) {
        unsigned count;
        uint32_t code;
        if (*p < 0x80) { count = 1; code = *p; }
        else if (*p >= 0xc2 && *p <= 0xdf) { count = 2; code = *p & 0x1f; }
        else if (*p >= 0xe0 && *p <= 0xef) { count = 3; code = *p & 0x0f; }
        else if (*p >= 0xf0 && *p <= 0xf4) { count = 4; code = *p & 0x07; }
        else goto invalid;
        for (unsigned i = 1; i < count; ++i) {
            if ((p[i] & 0xc0) != 0x80) goto invalid;
            code = (code << 6) | (p[i] & 0x3f);
        }
        if ((count == 2 && code < 0x80) || (count == 3 && code < 0x800) ||
            (count == 4 && code < 0x10000) || code > 0x10ffff ||
            (code >= 0xd800 && code <= 0xdfff) || code < 0x20 ||
            (code >= 0x7f && code <= 0x9f)) goto invalid;
        if (fits && count <= capacity - 1 - used) {
            memcpy(destination + used, p, count);
            used += count;
        } else fits = false;
        p += count;
    }
    destination[used] = '\0';
    return true;
invalid:
    destination[0] = '\0';
    return false;
}


bool fmo_text_json_has_nul(const char *text)
{
    if (!text) return false;
    for (const char *p = text; *p; ++p) {
        if (*p != '\\' || !p[1]) continue;
        ++p; // An escaped backslash is consumed together with its prefix.
        if (*p == 'u' && strncmp(p + 1, "0000", 4) == 0) return true;
    }
    return false;
}
