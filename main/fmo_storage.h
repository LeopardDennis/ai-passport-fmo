#pragma once
#include "esp_err.h"

/* Call once in the network worker, before any NVS or Wi-Fi initialization.
 * Fresh complete installs clear app data; only a completed reset commits its
 * image marker. Ordinary reboots retain data. Never touches identity/Recovery. */
esp_err_t fmo_storage_prepare(void);
