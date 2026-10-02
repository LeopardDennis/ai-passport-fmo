#include "fmo_endpoint.h"
#include <stddef.h>
#include <string.h>

bool fmo_endpoint_valid(const fmo_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->port || endpoint->host[FMO_HOST_MAX]) return false;
    size_t length = strlen(endpoint->host);
    if (!length) return false;
    size_t label = 0;
    bool numeric = true;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)endpoint->host[i];
        if (c == '.') {
            if (!label || endpoint->host[i - 1] == '-') return false;
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-')) return false;
            if ((!label && c == '-') || ++label > 63) return false;
            if (c < '0' || c > '9') numeric = false;
        }
    }
    if (!label || endpoint->host[length - 1] == '-') return false;
    if (numeric) {
        unsigned octets = 0, value = 0;
        for (size_t i = 0; i <= length; ++i) {
            if (!endpoint->host[i] || endpoint->host[i] == '.') {
                if (value > 255) return false;
                ++octets;
                value = 0;
            } else {
                value = value * 10 + (unsigned)(endpoint->host[i] - '0');
                if (value > 255) return false;
            }
        }
        if (octets != 4) return false;
    }
    return true;
}
