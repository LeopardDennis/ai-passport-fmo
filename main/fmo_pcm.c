#include "fmo_pcm.h"
#include <string.h>

void fmo_pcm_reset(fmo_pcm_t *pcm)
{
    /* Counts are the only authority for readable audio. Leave the 24 KiB
     * backing array untouched: reset runs with interrupts masked on the C3.
     * Uncommitted/stale samples remain inaccessible after every reset. */
    memset((char *)pcm + offsetof(fmo_pcm_t, head), 0,
           sizeof(*pcm) - offsetof(fmo_pcm_t, head));
}

size_t fmo_pcm_message_reserve(uint8_t opcode, bool fin, size_t frame_length, size_t offset)
{
    if (opcode != 2 || offset || frame_length > FMO_PCM_MESSAGE_BYTES) return 0;
    return fin ? (frame_length + 1) / 2 : FMO_PCM_MESSAGE_BYTES / 2;
}

/* PCM is little-endian; memcpy also permits unaligned network payloads.
 * Keep a portable conversion path for hosts with a different byte order. */
static void copy_samples(int16_t *out, const uint8_t *bytes, size_t count)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(out, bytes, count * sizeof(*out));
#else
    for (size_t i = 0; i < count; ++i) {
        uint16_t value = (uint16_t)bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8);
        out[i] = value <= INT16_MAX ? (int16_t)value : (int16_t)((int32_t)value - 65536);
    }
#endif
}

/* Space has already been reserved/dropped for this whole receive fragment.
 * A ring wrap needs at most two contiguous copies, never per-sample modulo. */
static void append_samples(fmo_pcm_t *p, const uint8_t *bytes, size_t count)
{
    if (!count) return;
    size_t tail = (p->head + p->count + p->pending_count) % FMO_PCM_CAPACITY;
    size_t first = FMO_PCM_CAPACITY - tail;
    if (first > count) first = count;
    copy_samples(p->samples + tail, bytes, first);
    if (count > first) copy_samples(p->samples, bytes + first * 2, count - first);
    p->pending_count += count;
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
    size_t incoming = (length + (p->partial_sample ? 1 : 0)) / 2;
    size_t free_samples = FMO_PCM_CAPACITY - p->count - p->pending_count;
    if (incoming > free_samples) {
        size_t dropped = incoming - free_samples;
        if (dropped > p->count) goto invalid; /* Never expose/drop partial messages. */
        p->head = (p->head + dropped) % FMO_PCM_CAPACITY;
        p->count -= dropped;
        p->dropped_samples += (uint32_t)dropped;
    }
    size_t remaining = length;
    if (p->partial_sample && remaining) {
        uint8_t pair[2] = {p->low_byte, *bytes++};
        append_samples(p, pair, 1);
        --remaining;
        p->partial_sample = false;
    }
    size_t pairs = remaining / 2;
    if (pairs) {
        append_samples(p, bytes, pairs);
        bytes += pairs * 2;
    }
    if (remaining & 1) {
        p->low_byte = *bytes;
        p->partial_sample = true;
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
    size_t available = 0;
    if (!p->count) {
        if (now_ms - p->last_ms >= 120) p->started = false;
    } else if (p->started || p->count >= FMO_PCM_START_SAMPLES || now_ms - p->first_ms >= 60) {
        p->started = true;
        available = p->count < samples ? p->count : samples;
        size_t first = FMO_PCM_CAPACITY - p->head;
        if (first > available) first = available;
        memcpy(output, p->samples + p->head, first * sizeof(*output));
        if (available > first)
            memcpy(output + first, p->samples, (available - first) * sizeof(*output));
        p->head = (p->head + available) % FMO_PCM_CAPACITY;
        p->count -= available;
    }
    /* Only pad the missing tail; do not clear samples that were just copied. */
    if (samples > available)
        memset(output + available, 0, (samples - available) * sizeof(*output));
    return available;
}
