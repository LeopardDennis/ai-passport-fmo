#include "fmo_clock.h"
#include <string.h>

bool fmo_clock_format(int64_t unix_seconds, char output[6])
{
    if (!output) return false;
    if (unix_seconds < INT64_C(1704067200) || unix_seconds >= INT64_C(4102444800)) {
        memcpy(output, "--:--", 6);
        return false;
    }
    unsigned minutes = (unsigned)((unix_seconds / 60 + 8 * 60) % (24 * 60));
    unsigned hour = minutes / 60, minute = minutes % 60;
    output[0] = '0' + hour / 10;
    output[1] = '0' + hour % 10;
    output[2] = ':';
    output[3] = '0' + minute / 10;
    output[4] = '0' + minute % 10;
    output[5] = '\0';
    return true;
}
