#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Standard Wi-Fi QR payload; failure leaves an empty output. */
bool fmo_wifi_qr_payload(char *out, size_t capacity, const char *ssid, const char *password);
