#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Format UTC epoch seconds as Beijing time (UTC+8), HH:MM only.
 * Unsynchronized or implausible dates produce --:--. No allocation or TZ globals. */
bool fmo_clock_format(int64_t unix_seconds, char output[6]);

/* Recent contact time in Beijing time: YYYY-MM-DD HH:MM:SS, including pre-2024 dates.
 * Invalid timestamps produce ---------- --:--:--. */
bool fmo_clock_format_history(int64_t unix_seconds, char output[20]);
