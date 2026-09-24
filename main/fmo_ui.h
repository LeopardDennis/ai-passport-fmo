#pragma once
#include "lvgl.h"
#include "fmo_monitor_state.h"

/* LVGL-thread only; application owns this screen for its entire lifetime. */
void fmo_ui_create(void);
void fmo_ui_render(const fmo_monitor_state_t *state, const char *error,
                   const char *setup_ssid, const char *setup_password,
                   int battery, uint64_t now_ms, bool sync_hint);
