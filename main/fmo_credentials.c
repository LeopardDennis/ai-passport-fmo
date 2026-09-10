#include "fmo_credentials.h"
#include <string.h>
bool fmo_credentials_valid(const char *ssid, size_t sn, const char *pass, size_t pn)
{
    return ssid && pass && sn >= 1 && sn <= 32 &&
           (pn == 0 || (pn >= 8 && pn <= 63)) &&
           memchr(ssid, 0, sn) == NULL && memchr(pass, 0, pn) == NULL;
}
