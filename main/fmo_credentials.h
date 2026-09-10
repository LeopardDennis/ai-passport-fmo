#pragma once
#include <stdbool.h>
#include <stddef.h>
bool fmo_credentials_valid(const char *ssid, size_t ssid_length,
                           const char *password, size_t password_length);
