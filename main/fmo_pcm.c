#include "fmo_pcm.h"
#include <string.h>

void fmo_pcm_reset(fmo_pcm_t *pcm)
{
    memset(pcm, 0, sizeof(*pcm));
}

size_t fmo_pcm_message_reserve(uint8_t opcode, bool fin, size_t frame_length, size_t offset)
{
    if (opcode != 2 || offset || frame_length > FMO_PCM_MESSAGE_BYTES) return 0;
    return fin ? (frame_length + 1) / 2 : FMO_PCM_MESSAGE_BYTES / 2;
}

fmo_pcm_result_t fmo_pcm_feed(fmo_pcm_t *p, uint8_t opcode, bool fin,
                            size_t frame_length, size_t offset,
                            const void *data, size_t length, uint64_t now_ms)
{
    if (opcode >= 8) return FMO_PCM_IGNORED; /* Ping/pong may interrupt fragments. */
    if (opcode == 1 && !p->active) return FMO_PCM_IGNORED;
    if ((opcode != 0 && opcode != 2) || (length && !data) ||
        offset > frame_length || length > frame_length - offset) goto invalid;
    if (offset == 0) {
        if (p->frame_offset != p->frame_length) goto invalid;
        if (opcode == 2) {
            if (p->active) goto invalid;
            p->active = true;
            p->message_length = 0;
        } else if (!p->active) goto invalid;
        p->frame_length = frame_length;
        p->frame_offset = 0;
        p->opcode = opcode;
        p->fin = fin;
    }
    if (!p->active || p->frame_offset != offset || p->frame_length != frame_length ||
        p->opcode != opcode || p->fin != fin ||
        frame_length - offset > FMO_PCM_MESSAGE_BYTES - p->message_length) goto invalid;
    const uint8_t *bytes = data;
    /* Decode immediately in bounded chunks; FMO currently sends 5120-byte
     * messages. Do not allocate a second whole-message buffer in internal RAM. */
    for (size_t i = 0; i < length; ++i) {
        if (!p->partial_sample) {
            p->low_byte = bytes[i];
            p->partial_sample = true;
            continue;
        }
        uint16_t value = (uint16_t)p->low_byte | ((uint16_t)bytes[i] << 8);
        int32_t signed_value = value <= INT16_MAX ? (int32_t)value : (int32_t)value - 65536;
        if (p->count + p->pending_count == FMO_PCM_CAPACITY) {
            if (!p->count) goto invalid;
            p->head = (p->head + 1) % FMO_PCM_CAPACITY;
            --p->count;
            ++p->dropped_samples;
        }
        p->samples[(p->head + p->count + p->pending_count++) % FMO_PCM_CAPACITY] = (int16_t)signed_value;
        p->partial_sample = false;
    }
    p->message_length += length;
    p->frame_offset += length;
    if (p->frame_offset != frame_length || !fin) return FMO_PCM_MORE;
    if (p->partial_sample) goto invalid;
    /* Publish a complete message atomically under the caller's buffer lock.
     * Streaming the first TCP fragment plays a few words before a delayed
     * remainder, unlike the FMO browser's complete-message scheduling. */
    if (p->pending_count) {
        if (!p->count) p->first_ms = now_ms;
        if (now_ms - p->last_ms >= 120) p->started = false;
        p->last_ms = now_ms;
        p->count += p->pending_count;
        p->pending_count = 0;
    }
    p->active = false;
    p->message_length = 0;
    return FMO_PCM_COMPLETE;
invalid:
    fmo_pcm_reset(p);
    return FMO_PCM_INVALID;
}

size_t fmo_pcm_read(fmo_pcm_t *p, int16_t *output, size_t samples, uint64_t now_ms)
{
    memset(output, 0, samples * sizeof(*output));
    if (!p->count) {
        if (now_ms - p->last_ms >= 120) p->started = false;
        return 0;
    }
    if (!p->started && p->count < FMO_PCM_START_SAMPLES && now_ms - p->first_ms < 60) return 0;
    p->started = true;
    size_t available = p->count < samples ? p->count : samples;
    for (size_t i = 0; i < available; ++i)
        output[i] = p->samples[(p->head + i) % FMO_PCM_CAPACITY];
    p->head = (p->head + available) % FMO_PCM_CAPACITY;
    p->count -= available;
    return available;
}
