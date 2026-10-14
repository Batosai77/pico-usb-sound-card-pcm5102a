#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 1024  // Jumlah frame audio stereo (1024 * 4 byte = 4096 byte, ~21.3ms pada 48kHz)
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    int16_t left;
    int16_t right;
} audio_frame_t;

typedef struct {
    audio_frame_t buffer[RING_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} audio_ring_buffer_t;

static inline void ring_buffer_init(audio_ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

static inline uint32_t ring_buffer_available_to_read(const audio_ring_buffer_t *rb) {
    return (rb->head - rb->tail);
}

static inline uint32_t ring_buffer_available_to_write(const audio_ring_buffer_t *rb) {
    return RING_BUFFER_SIZE - (rb->head - rb->tail);
}

static inline bool ring_buffer_push(audio_ring_buffer_t *rb, int16_t left, int16_t right) {
    if (ring_buffer_available_to_write(rb) == 0) {
        return false; // Buffer penuh
    }
    uint32_t idx = rb->head & RING_BUFFER_MASK;
    rb->buffer[idx].left = left;
    rb->buffer[idx].right = right;
    rb->head++;
    return true;
}

static inline bool ring_buffer_pop(audio_ring_buffer_t *rb, int16_t *left, int16_t *right) {
    if (ring_buffer_available_to_read(rb) == 0) {
        return false; // Buffer kosong
    }
    uint32_t idx = rb->tail & RING_BUFFER_MASK;
    *left = rb->buffer[idx].left;
    *right = rb->buffer[idx].right;
    rb->tail++;
    return true;
}

#endif // RING_BUFFER_H
