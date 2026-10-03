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
    char history[20];
    assert(fmo_clock_format_history(midnight_utc,history) && !strcmp(history,"2026-01-01 08:00:00"));
    assert(fmo_clock_format_history(midnight_utc+59,history) && !strcmp(history,"2026-01-01 08:00:59"));
    assert(fmo_clock_format_history(midnight_utc+60,history) && !strcmp(history,"2026-01-01 08:01:00"));
    assert(fmo_clock_format_history(midnight_utc+16*3600-1,history) && !strcmp(history,"2026-01-01 23:59:59"));
    assert(fmo_clock_format_history(midnight_utc+16*3600,history) && !strcmp(history,"2026-01-02 00:00:00"));
    assert(fmo_clock_format_history(INT64_C(946684800),history) && !strcmp(history,"2000-01-01 08:00:00"));
    assert(fmo_clock_format_history(INT64_C(951868800),history) && !strcmp(history,"2000-03-01 08:00:00"));
    assert(fmo_clock_format_history(INT64_C(951782400),history) && !strcmp(history,"2000-02-29 08:00:00"));
    assert(fmo_clock_format_history(INT64_C(4102444800)-1,history) && !strcmp(history,"2100-01-01 07:59:59"));
    assert(!fmo_clock_format_history(INT64_C(4102444800),history));
    assert(!fmo_clock_format_history(0,history) && !strcmp(history,"---------- --:--:--"));
    assert(!fmo_clock_format_history(-1,history) && !strcmp(history,"---------- --:--:--"));
    assert(!fmo_clock_format_history(INT64_MAX,history) && !strcmp(history,"---------- --:--:--"));
    assert(!fmo_clock_format_history(midnight_utc,NULL));
    return 0;
}
