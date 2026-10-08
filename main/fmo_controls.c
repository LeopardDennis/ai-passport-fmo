#include "fmo_controls.h"
#include <string.h>

void fmo_controls_observe_wifi(fmo_controls_t *c, const fmo_wifi_list_t *wifi)
{
    if (!wifi || wifi->count > FMO_WIFI_PROFILE_MAX) return;
    unsigned selection = c->wifi.count ? wifi->count : 0;
    if (c->view == FMO_VIEW_WIFI && c->selection < c->wifi.count) {
        for (unsigned i = 0; i < wifi->count; ++i)
            if (!strcmp(c->wifi.names[c->selection], wifi->names[i])) selection = i;
    }
    c->wifi = *wifi;
    if (c->view == FMO_VIEW_WIFI) c->selection = selection;
}

void fmo_controls_observe_setup(fmo_controls_t *c, bool active)
{
    if (active && !c->hotspot_active) {
        // Make all network actions reachable before entering the QR/info view.
        c->view = FMO_VIEW_NETWORK;
        c->selection = 0;
        c->setup_info = false;
    } else if (!active && c->hotspot_active && c->view == FMO_VIEW_SETUP) {
        c->view = FMO_VIEW_MONITOR;
    }
    c->hotspot_active = active;
}
fmo_action_t fmo_controls_key(fmo_controls_t *c, fmo_key_t key)
{
    if (c->view == FMO_VIEW_STATIONS) {
        if (key == FMO_KEY_BACK) { c->view = FMO_VIEW_MONITOR; return FMO_ACTION_NONE; }
        if (c->stations.request != FMO_STATION_NONE) return FMO_ACTION_NONE;
        if (key == FMO_KEY_OK) {
            if (c->stations.status == FMO_STATIONS_LOAD_FAILED ||
                c->stations.status == FMO_STATIONS_EMPTY) return FMO_ACTION_STATIONS;
            if (c->selection < c->stations.count) return FMO_ACTION_STATION_SWITCH;
        }
        if (key == FMO_KEY_UP) {
            if (c->selection) --c->selection;
            else if (c->stations.start) {
                c->stations.start -= FMO_STATION_PAGE_SIZE;
                c->selection = FMO_STATION_PAGE_SIZE - 1;
                return FMO_ACTION_STATIONS;
            }
        }
        if (key == FMO_KEY_DOWN) {
            if (c->selection + 1 < c->stations.count) ++c->selection;
            else if (c->stations.has_next) {
                c->stations.start += FMO_STATION_PAGE_SIZE;
                c->selection = 0;
                return FMO_ACTION_STATIONS;
            }
        }
        return FMO_ACTION_NONE;
    }
    if (c->view == FMO_VIEW_SETUP) {
        if (key == FMO_KEY_OK) c->setup_info = !c->setup_info;
        if (key == FMO_KEY_BACK || key == FMO_KEY_UP || key == FMO_KEY_DOWN) {
            c->view = FMO_VIEW_NETWORK;
            c->selection = 0;
            return FMO_ACTION_CANCEL;
        }
        return FMO_ACTION_NONE;
    }
    if (c->view == FMO_VIEW_WIFI) {
        unsigned count = c->wifi.count;
        if (key == FMO_KEY_BACK) {
            c->view = FMO_VIEW_NETWORK;
            c->selection = 1;
        } else if (count) {
            if (key == FMO_KEY_UP) c->selection = c->selection >= count ? count - 1 : (c->selection + count - 1) % count;
            if (key == FMO_KEY_DOWN) c->selection = c->selection >= count ? 0 : (c->selection + 1) % count;
            if (key == FMO_KEY_OK && c->selection < count) {
                c->view = FMO_VIEW_MONITOR;
                return FMO_ACTION_RETRY;
            }
        }
        return FMO_ACTION_NONE;
    }
    if (c->view == FMO_VIEW_NETWORK) {
        if (key == FMO_KEY_UP) c->selection = (c->selection + 2) % 3;
        if (key == FMO_KEY_DOWN) c->selection = (c->selection + 1) % 3;
        if (key == FMO_KEY_BACK || (key == FMO_KEY_OK && c->selection == 2)) {
            c->view = FMO_VIEW_MONITOR;
            return c->hotspot_active ? FMO_ACTION_CANCEL : FMO_ACTION_NONE;
        }
        if (key == FMO_KEY_OK && c->selection == 0) {
            if (c->hotspot_active) {
                c->view = FMO_VIEW_SETUP;
                c->setup_info = false;
            }
            else return FMO_ACTION_SETUP;
        }
        if (key == FMO_KEY_OK && c->selection == 1) {
            c->view = FMO_VIEW_WIFI;
            c->selection = 0;
        }
        return FMO_ACTION_NONE;
    }
    switch (key) {
    case FMO_KEY_UP:
        c->volume = c->volume >= 90 ? 100 : c->volume + 10;
        return FMO_ACTION_VOLUME;
    case FMO_KEY_DOWN:
        c->volume = c->volume <= 10 ? 0 : c->volume - 10;
        return FMO_ACTION_VOLUME;
    case FMO_KEY_OK:
        c->audio_enabled = !c->audio_enabled;
        return FMO_ACTION_AUDIO;
    case FMO_KEY_STATIONS:
        c->view = FMO_VIEW_STATIONS;
        if (c->stations.request == FMO_STATION_NONE) {
            c->selection = 0;
            c->stations.start = 0;
            return FMO_ACTION_STATIONS;
        }
        for (unsigned i = 0; i < c->stations.count; ++i)
            if (c->stations.rows[i].uid == c->stations.target_uid) c->selection = i;
        break;
    case FMO_KEY_BACK: c->view = FMO_VIEW_NETWORK; c->selection = 0; break;
    }
    return FMO_ACTION_NONE;
}
