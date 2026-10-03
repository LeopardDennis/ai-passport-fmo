#include "fmo_audio_meter.h"
#include <string.h>

static uint32_t integer_sqrt(uint32_t value)
{
    uint32_t result = 0, bit = UINT32_C(1) << 30;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else result >>= 1;
        bit >>= 2;
    }
    return result;
}

uint8_t fmo_audio_meter_measure(const int16_t *samples, size_t count)
{
    if (!samples || !count) return 0;
    uint64_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t sample = samples[i];
        sum += (uint64_t)(sample * sample);
    }
    uint32_t rms = integer_sqrt((uint32_t)(sum / count));
    /* Roughly 6 dB steps make quiet speech visible without normalizing each
     * talker to full scale. Piecewise interpolation avoids log/sqrt libraries. */
    static const uint16_t amplitudes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192};
    static const uint8_t levels[] = {0, 10, 25, 40, 55, 70, 85, 100};
    if (rms <= amplitudes[0]) return 0;
    for (size_t i = 1; i < sizeof(levels); ++i) {
        if (rms < amplitudes[i])
            return (uint8_t)(levels[i - 1] +
                (rms - amplitudes[i - 1]) * (levels[i] - levels[i - 1]) /
                (amplitudes[i] - amplitudes[i - 1]));
    }
    return 100;
}

void fmo_audio_meter_reset(fmo_audio_meter_t *meter)
{
    memset(meter, 0, sizeof(*meter));
}

uint8_t fmo_audio_meter_step(fmo_audio_meter_t *meter, uint8_t target, uint64_t now_ms)
{
    if (target > 100) target = 100;
    if (!meter->initialized || now_ms < meter->last_ms) {
        meter->last_ms = now_ms;
        meter->initialized = true;
        meter->level_q8 = 0;
        return 0;
    }
    uint64_t elapsed = now_ms - meter->last_ms;
    meter->last_ms = now_ms;
    if (elapsed > 500) elapsed = 500;
    uint32_t goal = (uint32_t)target * 256;
    bool rising = goal > meter->level_q8;
    uint32_t change = (uint32_t)(elapsed * 100 * 256 / (rising ? 80 : 500));
    if (rising) {
        uint32_t gap = goal - meter->level_q8;
        meter->level_q8 += change < gap ? change : gap;
    } else {
        uint32_t gap = meter->level_q8 - goal;
        meter->level_q8 -= change < gap ? change : gap;
    }
    return (uint8_t)(meter->level_q8 / 256);
}
