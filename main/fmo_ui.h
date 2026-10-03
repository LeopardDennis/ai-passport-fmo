#pragma once
#include "lvgl.h"
#include "fmo_controls.h"
#include "fmo_monitor_state.h"

/* LVGL-thread only; application owns this screen for its entire lifetime. */
void fmo_ui_create(void);
void fmo_ui_set_clock(int64_t unix_seconds);
/* Call every 50 ms on the LVGL thread; only updates the narrow audio bar. */
void fmo_ui_set_audio_level(uint8_t level, uint64_t now_ms);
void fmo_ui_render(const fmo_monitor_state_t *state, const char *error,
                   const char *setup_ssid, const char *setup_password,
                   int battery, uint64_t now_ms, bool sync_hint, const fmo_controls_t *controls);
