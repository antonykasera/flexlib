/**
 * @file fu_ring.h
 * @brief Lock-free SPSC rings: bytes, fixed-size elements, DMA receive
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FU_RING_H
#define FU_RING_H

#include "fx_arch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* one producer, one consumer, one core (compiler barriers only) */
typedef struct {
  uint8_t *buf;
  uint32_t mask;          /* size - 1 */
  volatile uint32_t head; /* producer only */
  volatile uint32_t tail; /* consumer only */
} FU_Ring;

/* size: a power of two */
void fu_ring_init(FU_Ring *r, void *buf, size_t size);

FX_INLINE size_t fu_ring_used(const FU_Ring *r) { return r->head - r->tail; }
FX_INLINE size_t fu_ring_free(const FU_Ring *r) {
  return r->mask + 1U - (r->head - r->tail);
}

FX_INLINE bool fu_ring_put(FU_Ring *r, uint8_t b) {
  uint32_t h = r->head;
  if (h - r->tail > r->mask) {
    return false;
  }
  r->buf[h & r->mask] = b;
  fx_compiler_barrier(); /* the byte lands before head publishes it */
  r->head = h + 1U;
  return true;
}

FX_INLINE bool fu_ring_get(FU_Ring *r, uint8_t *b) {
  uint32_t t = r->tail;
  if (r->head == t) {
    return false;
  }
  fx_compiler_barrier();
  *b = r->buf[t & r->mask];
  fx_compiler_barrier(); /* read before the slot is handed back */
  r->tail = t + 1U;
  return true;
}

/* copy what fits / is there; returns the count */
size_t fu_ring_write(FU_Ring *r, const void *src, size_t n);
size_t fu_ring_read(FU_Ring *r, void *dst, size_t n);
size_t fu_ring_peek(const FU_Ring *r, void *dst, size_t n);
void fu_ring_skip(FU_Ring *r, size_t n); /* n <= used */

/* contiguous readable bytes for DMA TX; then fu_ring_skip */
const uint8_t *fu_ring_span(const FU_Ring *r, size_t *len);

/* drain circular-DMA bytes since *dma_pos; remaining: channel count reg */
size_t fu_ring_from_dma(FU_Ring *r, const void *dma_buf, size_t dma_size,
                        size_t *dma_pos, size_t remaining);

/* fixed-size elements */
typedef struct {
  uint8_t *buf;
  uint32_t mask; /* count - 1 */
  uint32_t esize;
  volatile uint32_t head;
  volatile uint32_t tail;
} FU_Queue;

/* buf holds count * esize bytes; count: a power of two */
void fu_queue_init(FU_Queue *q, void *buf, size_t count, size_t esize);
bool fu_queue_push(FU_Queue *q, const void *e);
bool fu_queue_pop(FU_Queue *q, void *e);

FX_INLINE size_t fu_queue_used(const FU_Queue *q) { return q->head - q->tail; }

#endif /* FU_RING_H */
