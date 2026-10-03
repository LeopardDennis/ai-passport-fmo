#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded PCM16-LE streaming: 1536 ms queue, at most 1 s per message. */
#define FMO_PCM_RATE 8000
#define FMO_PCM_CAPACITY 12288
#define FMO_PCM_START_SAMPLES (FMO_PCM_RATE / 10)
#define FMO_PCM_MESSAGE_BYTES 16000
#define FMO_PCM_CHUNK_SAMPLES 160

typedef struct {
    int16_t samples[FMO_PCM_CAPACITY];
    size_t head, count, pending_count, message_length, frame_length, frame_offset;
    uint64_t first_ms, last_ms;
    uint32_t dropped_samples; /* Complete oldest samples discarded on overflow. */
    uint8_t opcode, low_byte;
    bool active, fin, started, partial_sample;
} fmo_pcm_t;

typedef enum { FMO_PCM_MORE, FMO_PCM_COMPLETE, FMO_PCM_IGNORED, FMO_PCM_INVALID } fmo_pcm_result_t;
void fmo_pcm_reset(fmo_pcm_t *pcm);
/* Reserve a whole new message before accepting its first chunk. Continuations
 * already own their space; an unknown fragmented length reserves the maximum. */
size_t fmo_pcm_message_reserve(uint8_t opcode, bool fin, size_t frame_length, size_t offset);
/* Caller serializes feed/read/reset. Invalid binary fragments discard queued
 * audio. Partial message samples stay unreadable inside the same ring until FIN;
 * no second message buffer is allocated. Odd chunk splits preserve alignment. */
fmo_pcm_result_t fmo_pcm_feed(fmo_pcm_t *pcm, uint8_t opcode, bool fin,
                            size_t frame_length, size_t offset,
                            const void *data, size_t length, uint64_t now_ms);
/* Always initializes the whole output to audio or silence. Returns the number
 * of real samples; initial buffering targets 100 ms, short tails at most 60 ms. */
size_t fmo_pcm_read(fmo_pcm_t *pcm, int16_t *output, size_t samples, uint64_t now_ms);
