#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Source PCM level, independent of speaker volume. No allocation or floating
 * point: RMS below -54 dBFS is silent; -12 dBFS and above fills the bar. */
uint8_t fmo_audio_meter_measure(const int16_t *samples, size_t count);

typedef struct {
    uint32_t level_q8;
    uint64_t last_ms;
    bool initialized;
} fmo_audio_meter_t;

void fmo_audio_meter_reset(fmo_audio_meter_t *meter);
/* Time-based envelope: 80 ms full-scale rise, 500 ms full-scale fall. */
uint8_t fmo_audio_meter_step(fmo_audio_meter_t *meter, uint8_t target, uint64_t now_ms);
