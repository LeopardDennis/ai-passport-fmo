#pragma once
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>

typedef void (*fmo_setup_display_t)(const char *ssid, const char *password);
#define FMO_WIFI_READY_BIT BIT0
#define FMO_WIFI_DISCONNECTED_BIT BIT1
#define FMO_WIFI_STOPPED_BIT BIT2
bool fmo_provision_load(wifi_config_t *config, bool *force);
esp_err_t fmo_provision_force(void);
/* Network-worker only, outside provisioning. Scans only while disconnected. */
bool fmo_provision_next(wifi_config_t *config, uint8_t *tried);
uint8_t fmo_provision_preferred_mask(void);
void fmo_provision_remember_connected(void);
/* Event-loop callback: record the reason for a failed setup attempt. */
void fmo_provision_note_disconnect_reason(uint8_t reason);
/* Blocking, network-worker only. Wi-Fi is initialized and started in STA mode.
 * No WebSocket clients may be running. Returns after verified NVS persistence. */
esp_err_t fmo_provision_run(EventGroupHandle_t bits, EventBits_t ready,
                            fmo_setup_display_t display);
