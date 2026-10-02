#include "fmo_controls.h"
void fmo_controls_observe_setup(fmo_controls_t *c, bool active)
{
    if (active && !c->hotspot_active) {
        c->view = FMO_VIEW_SETUP;
        c->setup_info = false;
    } else if (!active && c->hotspot_active && c->view == FMO_VIEW_SETUP) {
        c->view = FMO_VIEW_MONITOR;
    }
    c->hotspot_active = active;
}
fmo_action_t fmo_controls_key(fmo_controls_t *c, fmo_key_t key)
{
    if (key == FMO_KEY_REFRESH) return FMO_ACTION_REFRESH;
    if (c->view == FMO_VIEW_SETUP) {
        if (key == FMO_KEY_OK) c->setup_info = !c->setup_info;
        if (key == FMO_KEY_BACK) {
            c->view = FMO_VIEW_NETWORK;
            c->selection = 0;
            return FMO_ACTION_CANCEL;
        }
        return FMO_ACTION_NONE;
    }
    if (c->view == FMO_VIEW_NETWORK) {
        if (key == FMO_KEY_UP) c->selection = (c->selection + 2) % 3;
        if (key == FMO_KEY_DOWN) c->selection = (c->selection + 1) % 3;
        if (key == FMO_KEY_BACK || (key == FMO_KEY_OK && c->selection == 2))
            c->view = FMO_VIEW_MONITOR;
        if (key == FMO_KEY_OK && c->selection == 0) {
            if (c->hotspot_active) c->view = FMO_VIEW_SETUP;
            else return FMO_ACTION_SETUP;
        }
        if (key == FMO_KEY_OK && c->selection == 1) {
            c->view = FMO_VIEW_MONITOR;
            return FMO_ACTION_RETRY;
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
    case FMO_KEY_REFRESH: return FMO_ACTION_REFRESH;
    case FMO_KEY_BACK: c->view = FMO_VIEW_NETWORK; c->selection = 0; break;
    }
    return FMO_ACTION_NONE;
}
