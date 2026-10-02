#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* One lifetime worker owns the codec and /audio WebSocket. Public setters are
 * nonblocking, may run in the LVGL timer, and never access hardware or LVGL. */
esp_err_t fmo_audio_start(void);
void fmo_audio_set_online(bool online);
/* 0 silences playback and releases the audio connection; range 0..100. */
void fmo_audio_set_volume(uint8_t percent);
