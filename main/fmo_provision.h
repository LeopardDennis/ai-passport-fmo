#pragma once
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>

typedef void (*fmo_setup_display_t)(const char *ssid, const char *password);
bool fmo_provision_load(wifi_config_t *config, bool *force);
esp_err_t fmo_provision_force(void);
/* Network-worker only, outside provisioning. Scans only while disconnected. */
bool fmo_provision_next(wifi_config_t *config, uint8_t *tried);
uint8_t fmo_provision_preferred_mask(void);
void fmo_provision_remember_connected(void);
/* Blocking, network-worker only. Wi-Fi is initialized and started in STA mode.
 * No WebSocket clients may be running. Returns after verified NVS persistence. */
esp_err_t fmo_provision_run(EventGroupHandle_t bits, EventBits_t ready,
                            fmo_setup_display_t display);
