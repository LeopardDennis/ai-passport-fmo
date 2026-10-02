#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded PCM16-LE streaming: 512 ms queue, at most 1 s per message. */
#define FMO_PCM_RATE 8000
#define FMO_PCM_CAPACITY 4096
#define FMO_PCM_MESSAGE_BYTES 16000
#define FMO_PCM_CHUNK_SAMPLES 160

typedef struct {
    int16_t samples[FMO_PCM_CAPACITY];
    size_t head, count, message_length, frame_length, frame_offset;
    uint64_t first_ms, last_ms;
    uint8_t opcode, low_byte;
    bool active, fin, started, partial_sample;
} fmo_pcm_t;

typedef enum { FMO_PCM_MORE, FMO_PCM_COMPLETE, FMO_PCM_IGNORED, FMO_PCM_INVALID } fmo_pcm_result_t;
void fmo_pcm_reset(fmo_pcm_t *pcm);
/* Caller serializes feed/read/reset. Invalid binary fragments discard queued
 * audio. Only complete samples enter the queue, including odd chunk splits. */
fmo_pcm_result_t fmo_pcm_feed(fmo_pcm_t *pcm, uint8_t opcode, bool fin,
                            size_t frame_length, size_t offset,
                            const void *data, size_t length, uint64_t now_ms);
/* Always initializes the whole output to audio or silence. Returns the number
 * of real samples; initial buffering lasts 40 ms, short tails at most 60 ms. */
size_t fmo_pcm_read(fmo_pcm_t *pcm, int16_t *output, size_t samples, uint64_t now_ms);
