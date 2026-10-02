#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Validate printable UTF-8 and copy a whole-character prefix into destination.
 * Invalid input clears the destination; truncation is valid, never splits UTF-8. */
bool fmo_text_copy_utf8(char *destination, size_t capacity, const char *source);

/* Detect a JSON Unicode NUL escape, respecting escaped backslashes. */
bool fmo_text_json_has_nul(const char *text);
