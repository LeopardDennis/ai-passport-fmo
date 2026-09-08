#pragma once

#include "esp_err.h"
#include "fmo_monitor_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FMO_UPDATE_WIFI,
    FMO_UPDATE_EVENTS_LINK,
    FMO_UPDATE_CONTROL_LINK,
    FMO_UPDATE_CHANNEL,
    FMO_UPDATE_SPEAKER,
    FMO_UPDATE_ERROR,
} fmo_update_type_t;

typedef struct {
    fmo_update_type_t type;
    bool connected;
    bool speaking;
    bool is_host;
    uint32_t uid;
    char callsign[16];
    char grid[12];
    char text[48];
} fmo_update_t;

typedef struct {
    fmo_monitor_state_t state;
    char error[48];
} fmo_snapshot_t;

/* Starts the lifetime-owned worker. Queue must hold ONE fmo_snapshot_t. */
esp_err_t fmo_network_start(QueueHandle_t update_queue);

/* Non-blocking hint from the UI to query the current FMO channel again. */
void fmo_network_request_refresh(void);
