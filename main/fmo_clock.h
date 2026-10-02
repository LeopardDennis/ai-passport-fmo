#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Format UTC epoch seconds as Beijing time (UTC+8), HH:MM only.
 * Unsynchronized or implausible dates produce --:--. No allocation or TZ globals. */
bool fmo_clock_format(int64_t unix_seconds, char output[6]);
