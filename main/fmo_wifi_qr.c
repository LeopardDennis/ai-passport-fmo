#include "fmo_wifi_qr.h"
#include <string.h>
static bool append(char *out, size_t cap, size_t *n, const char *s, bool escape)
{
    for (; *s; ++s) {
        if ((unsigned char)*s < 32 || (unsigned char)*s == 127) return false;
        if (escape && strchr("\\;,:\"", *s)) {
            if (*n + 1 >= cap) return false;
            out[(*n)++] = '\\';
        }
        if (*n + 1 >= cap) return false;
        out[(*n)++] = *s;
    }
    out[*n] = 0;
    return true;
}
bool fmo_wifi_qr_payload(char *out, size_t cap, const char *ssid, const char *password)
{
    if (!out || !cap) return false;
    out[0] = 0;
    if (!ssid || !*ssid || !password || !*password) return false;
    size_t n = 0;
    bool ok = append(out, cap, &n, "WIFI:T:WPA;S:", false) &&
        append(out, cap, &n, ssid, true) && append(out, cap, &n, ";P:", false) &&
        append(out, cap, &n, password, true) && append(out, cap, &n, ";;", false);
    if (!ok) out[0] = 0;
    return ok;
}
