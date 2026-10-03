#include "fmo_audio_meter.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    int16_t samples[160] = {0};
    assert(fmo_audio_meter_measure(NULL, 160) == 0);
    assert(fmo_audio_meter_measure(samples, 0) == 0);
    assert(fmo_audio_meter_measure(samples, 160) == 0);
    uint8_t previous = 0;
    for (int amplitude = 1; amplitude <= 32768; ++amplitude) {
        /* Alternating polarity models PCM without DC, including INT16_MIN. */
        for (unsigned i = 0; i < 160; ++i)
            samples[i] = (int16_t)(i % 2 || amplitude == 32768 ? -amplitude : amplitude);
        uint8_t level = fmo_audio_meter_measure(samples, 160);
        assert(level >= previous && level <= 100);
        if (amplitude <= 64) assert(level == 0);
        if (amplitude == 1024) assert(level == 55);
        if (amplitude >= 8192) assert(level == 100);
        previous = level;
    }
    /* A single full-scale spike has lower RMS than sustained full-scale PCM. */
    for (unsigned i = 0; i < 160; ++i) samples[i] = i ? 0 : INT16_MIN;
    assert(fmo_audio_meter_measure(samples, 160) < 100);
    fmo_audio_meter_t meter = {0};
    assert(fmo_audio_meter_step(&meter, 100, 1000) == 0);
    assert(fmo_audio_meter_step(&meter, 100, 1040) == 50);
    assert(fmo_audio_meter_step(&meter, 100, 1080) == 100);
    assert(fmo_audio_meter_step(&meter, 0, 1130) == 90);
    assert(fmo_audio_meter_step(&meter, 0, 1330) == 50);
    assert(fmo_audio_meter_step(&meter, 0, 1580) == 0);
    assert(fmo_audio_meter_step(&meter, 255, 1660) == 100);
    assert(fmo_audio_meter_step(&meter, 100, 1660) == 100);
    assert(fmo_audio_meter_step(&meter, 0, 50000) == 0);
    assert(fmo_audio_meter_step(&meter, 100, 50080) == 100);
    assert(fmo_audio_meter_step(&meter, 100, 100) == 0); /* Clock rollback. */
    assert(fmo_audio_meter_step(&meter, 100, 180) == 100);
    fmo_audio_meter_reset(&meter);
    assert(!meter.initialized && !meter.level_q8);
    puts("FMO audio meter: PASS (PCM RMS, signed range, gate, monotonic scale, timed envelope)");
}
