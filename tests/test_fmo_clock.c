#include "fmo_clock.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    char text[6];
    const int64_t midnight_utc = INT64_C(1767225600); /* 2026-01-01 UTC */
    assert(fmo_clock_format(midnight_utc, text) && !strcmp(text,"08:00"));
    assert(fmo_clock_format(midnight_utc + 59, text) && !strcmp(text,"08:00"));
    assert(fmo_clock_format(midnight_utc + 60, text) && !strcmp(text,"08:01"));
    assert(fmo_clock_format(midnight_utc + 16 * 3600 - 1, text) && !strcmp(text,"23:59"));
    assert(fmo_clock_format(midnight_utc + 16 * 3600, text) && !strcmp(text,"00:00"));
    assert(!fmo_clock_format(0,text) && !strcmp(text,"--:--"));
    assert(!fmo_clock_format(-1,text) && !strcmp(text,"--:--"));
    assert(!fmo_clock_format(INT64_MAX,text) && !strcmp(text,"--:--"));
    assert(!fmo_clock_format(midnight_utc,NULL));
    return 0;
}
