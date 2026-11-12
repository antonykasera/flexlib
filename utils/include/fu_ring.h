/**
 * @file fu_ring.h
 * @brief Lock-free SPSC rings: bytes, fixed-size elements, DMA receive
 *
 * One producer and one consumer on one core; either may be an interrupt.
 * Only compiler barriers order the accesses, so two cores need more. The
 * producer calls `put`, `write`, `from_dma` and `push`; the consumer `get`,
 * `read`, `peek`, `skip`, `span` and `pop`; the counts work from either side.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_RING_H
#define FU_RING_H

#include "fx_arch.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Byte ring; opaque. */
typedef struct {
  /** @privatesection */
  uint8_t *buf;
  uint32_t mask;          /* size - 1 */
  volatile uint32_t head; /* producer only */
  volatile uint32_t tail; /* consumer only */
} FU_Ring;

/**
 * Set up an empty ring.
 *
 * @param r    ring
 * @param buf  storage; must outlive r
 * @param size bytes, a power of two up to 2^31
 */
void fu_ring_init(FU_Ring *r, void *buf, size_t size);

/** Bytes waiting to be read. */
FX_INLINE size_t fu_ring_used(const FU_Ring *r) { return r->head - r->tail; }
/** Bytes that can be written. */
FX_INLINE size_t fu_ring_free(const FU_Ring *r) {
  return r->mask + 1U - (r->head - r->tail);
}

/** Add one byte; false when full. */
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

/** Take one byte; false when empty. */
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

/** Copy in as many of the n bytes as fit; returns the count. */
size_t fu_ring_write(FU_Ring *r, const void *src, size_t n);
/** Copy out up to n bytes; returns the count. */
size_t fu_ring_read(FU_Ring *r, void *dst, size_t n);
/** fu_ring_read() that leaves the bytes in the ring. */
size_t fu_ring_peek(const FU_Ring *r, void *dst, size_t n);
/** Drop n bytes, at most fu_ring_used(). */
void fu_ring_skip(FU_Ring *r, size_t n);

/** The readable bytes up to the end of the buffer, `*len` of them, for a DMA
 * transmit; release them with fu_ring_skip() once sent. */
const uint8_t *fu_ring_span(const FU_Ring *r, size_t *len);

/**
 * Move the bytes a circular DMA wrote since the last call into the ring;
 * returns the count. Bytes that do not fit wait for the next call. Call it
 * before the DMA laps `*dma_pos`: a lap goes unnoticed.
 *
 * @param r         ring
 * @param dma_buf   the DMA's buffer
 * @param dma_size  its size in bytes
 * @param dma_pos   where the last call stopped; 0 at the start
 * @param remaining the channel's count register now (CNDTR on STM32)
 */
size_t fu_ring_from_dma(FU_Ring *r, const void *dma_buf, size_t dma_size,
                        size_t *dma_pos, size_t remaining);

/** Queue of fixed-size elements, with the ring's rules; opaque. */
typedef struct {
  /** @privatesection */
  uint8_t *buf;
  uint32_t mask; /* count - 1 */
  uint32_t esize;
  volatile uint32_t head;
  volatile uint32_t tail;
} FU_Queue;

/**
 * Set up an empty queue.
 *
 * @param q     queue
 * @param buf   count * esize bytes; must outlive q
 * @param count elements, a power of two up to 2^31
 * @param esize bytes per element, not 0
 */
void fu_queue_init(FU_Queue *q, void *buf, size_t count, size_t esize);
/** Copy one element in; false when full. */
bool fu_queue_push(FU_Queue *q, const void *e);
/** Copy one element out; false when empty. */
bool fu_queue_pop(FU_Queue *q, void *e);

/** Elements waiting. */
FX_INLINE size_t fu_queue_used(const FU_Queue *q) { return q->head - q->tail; }

#endif /* FU_RING_H */
