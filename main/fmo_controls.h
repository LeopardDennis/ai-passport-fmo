#pragma once
#include <stdbool.h>
#include "fmo_stations.h"
#include <stdint.h>
#include "fmo_wifi_profiles.h"
typedef enum { FMO_VIEW_MONITOR, FMO_VIEW_NETWORK, FMO_VIEW_SETUP, FMO_VIEW_STATIONS, FMO_VIEW_WIFI } fmo_view_t;
typedef enum { FMO_KEY_UP, FMO_KEY_DOWN, FMO_KEY_OK, FMO_KEY_BACK, FMO_KEY_STATIONS } fmo_key_t;
typedef enum { FMO_ACTION_NONE, FMO_ACTION_VOLUME, FMO_ACTION_AUDIO,
    FMO_ACTION_STATIONS, FMO_ACTION_STATION_SWITCH, FMO_ACTION_SETUP, FMO_ACTION_RETRY, FMO_ACTION_CANCEL } fmo_action_t;
typedef struct {
    fmo_view_t view;
    unsigned selection;
    fmo_wifi_list_t wifi;
    char connected_ssid[33];
    bool setup_info;
    bool hotspot_active;
    bool audio_enabled;
    uint8_t volume;
    fmo_stations_t stations;
    bool station_already_current;
} fmo_controls_t;
void fmo_controls_observe_wifi(fmo_controls_t *controls, const fmo_wifi_list_t *wifi);
void fmo_controls_observe_setup(fmo_controls_t *controls, bool active);
fmo_action_t fmo_controls_key(fmo_controls_t *controls, fmo_key_t key);
