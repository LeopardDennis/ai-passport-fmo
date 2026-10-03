#include "fmo_clock.h"
#include <string.h>
#include <stdio.h>

static bool format_time(int64_t unix_seconds, int64_t earliest, char output[6])
{
    if (!output) return false;
    if (unix_seconds < earliest || unix_seconds >= INT64_C(4102444800)) {
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

bool fmo_clock_format(int64_t unix_seconds, char output[6])
{
    return format_time(unix_seconds, INT64_C(1704067200), output);
}

bool fmo_clock_format_history(int64_t unix_seconds, char output[20])
{
    if (!output) return false;
    if (unix_seconds < INT64_C(946684800) || unix_seconds >= INT64_C(4102444800)) {
        memcpy(output, "---------- --:--:--", 20);
        return false;
    }
    /* Bounded Gregorian conversion, independent of libc TZ and time_t width.
     * Input is 2000..2099 UTC; Beijing may advance into 2100-01-01. */
    int64_t local = unix_seconds + 8 * 3600;
    unsigned days = (unsigned)(local / 86400 - 10957);
    unsigned year = 2000, month = 1;
    for (;;) {
        unsigned year_days = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 366 : 365;
        if (days < year_days) break;
        days -= year_days;
        ++year;
    }
    static const unsigned month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (; month <= 12; ++month) {
        unsigned length = month_days[month - 1];
        if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ++length;
        if (days < length) break;
        days -= length;
    }
    unsigned second_of_day = (unsigned)(local % 86400);
    snprintf(output, 20, "%04u-%02u-%02u %02u:%02u:%02u", year, month, days + 1,
             second_of_day / 3600, second_of_day / 60 % 60, second_of_day % 60);
    return true;
}
