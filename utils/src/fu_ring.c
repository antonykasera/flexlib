/**
 * @file fu_ring.c
 * @brief Lock-free single-producer, single-consumer rings
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_ring.h"
#include "fu_bits.h"

/* Word copies (the M3 may load and store unaligned); newlib-nano's memcpy goes
 * a byte at a time. FX_OPAQUE keeps the loops from becoming memcpy calls. */
__attribute__((noinline)) static void copy16(uint8_t *d, const uint8_t *s,
                                             size_t n) {
  for (; n != 0U; n -= 16U, d += 16, s += 16) {
    FX_OPAQUE(d);
    uint32_t a = fu_load_le32(s), b = fu_load_le32(s + 4);
    uint32_t c = fu_load_le32(s + 8), e = fu_load_le32(s + 12);
    fu_store_le32(d, a);
    fu_store_le32(d + 4, b);
    fu_store_le32(d + 8, c);
    fu_store_le32(d + 12, e);
  }
}

FX_INLINE void copy(uint8_t *d, const uint8_t *s, size_t n) {
  if (n >= 16U) {
    size_t k = n & ~(size_t)15U;
    copy16(d, s, k);
    d += k;
    s += k;
    n -= k;
  }
  for (; n >= 4U; n -= 4U, d += 4, s += 4) {
    FX_OPAQUE(d);
    fu_store_le32(d, fu_load_le32(s));
  }
  for (; n != 0U; n--) {
    FX_OPAQUE(d);
    *d++ = *s++;
  }
}

static bool pow2(size_t n) { return n != 0U && (n & (n - 1U)) == 0U; }

void fu_ring_init(FU_Ring *r, void *buf, size_t size) {
  FX_ASSERT(buf != NULL && pow2(size) && size <= 0x80000000U);
  r->buf = buf;
  r->mask = (uint32_t)(size - 1U);
  r->head = 0U;
  r->tail = 0U;
}

size_t fu_ring_write(FU_Ring *r, const void *src, size_t n) {
  FX_ASSERT(src != NULL || n == 0U);
  uint32_t h = r->head;
  size_t room = r->mask + 1U - (h - r->tail);
  n = n < room ? n : room;
  size_t off = h & r->mask;
  size_t first = r->mask + 1U - off;
  first = first < n ? first : n;
  copy(r->buf + off, src, first);
  if (n != first) {
    copy(r->buf, (const uint8_t *)src + first, n - first);
  }
  fx_compiler_barrier();
  r->head = h + (uint32_t)n;
  return n;
}

/* up to n bytes from index t on; the caller owns t */
FX_INLINE size_t take(const FU_Ring *r, uint32_t t, void *dst, size_t n) {
  FX_ASSERT(dst != NULL || n == 0U);
  size_t avail = r->head - t;
  n = n < avail ? n : avail;
  fx_compiler_barrier();
  size_t off = t & r->mask;
  size_t first = r->mask + 1U - off;
  first = first < n ? first : n;
  copy(dst, r->buf + off, first);
  if (n != first) {
    copy((uint8_t *)dst + first, r->buf, n - first);
  }
  return n;
}

size_t fu_ring_read(FU_Ring *r, void *dst, size_t n) {
  uint32_t t = r->tail;
  n = take(r, t, dst, n);
  fx_compiler_barrier();
  r->tail = t + (uint32_t)n;
  return n;
}

size_t fu_ring_peek(const FU_Ring *r, void *dst, size_t n) {
  return take(r, r->tail, dst, n);
}

void fu_ring_skip(FU_Ring *r, size_t n) {
  FX_ASSERT(n <= fu_ring_used(r));
  fx_compiler_barrier();
  r->tail += (uint32_t)n;
}

const uint8_t *fu_ring_span(const FU_Ring *r, size_t *len) {
  uint32_t t = r->tail;
  size_t off = t & r->mask;
  size_t used = r->head - t;
  size_t to_end = r->mask + 1U - off;
  *len = used < to_end ? used : to_end;
  return r->buf + off;
}

size_t fu_ring_from_dma(FU_Ring *r, const void *dma_buf, size_t dma_size,
                        size_t *dma_pos, size_t remaining) {
  FX_ASSERT(remaining <= dma_size && *dma_pos < dma_size);
  const uint8_t *b = dma_buf;
  size_t w = dma_size - remaining; /* where the DMA writes next */
  w = w == dma_size ? 0U : w;
  size_t pos = *dma_pos;
  size_t moved = 0U;
  if (w < pos) { /* wrapped: the end of the buffer first */
    size_t k = fu_ring_write(r, b + pos, dma_size - pos);
    moved = k;
    pos += k;
    if (pos != dma_size) {
      *dma_pos = pos;
      return moved;
    }
    pos = 0U;
  }
  size_t k = fu_ring_write(r, b + pos, w - pos);
  *dma_pos = pos + k;
  return moved + k;
}

void fu_queue_init(FU_Queue *q, void *buf, size_t count, size_t esize) {
  FX_ASSERT(buf != NULL && pow2(count) && count <= 0x80000000U && esize != 0U);
  q->buf = buf;
  q->mask = (uint32_t)(count - 1U);
  q->esize = (uint32_t)esize;
  q->head = 0U;
  q->tail = 0U;
}

bool fu_queue_push(FU_Queue *q, const void *e) {
  uint32_t h = q->head;
  if (h - q->tail > q->mask) {
    return false;
  }
  copy(q->buf + (h & q->mask) * q->esize, e, q->esize);
  fx_compiler_barrier();
  q->head = h + 1U;
  return true;
}

bool fu_queue_pop(FU_Queue *q, void *e) {
  uint32_t t = q->tail;
  if (q->head == t) {
    return false;
  }
  fx_compiler_barrier();
  copy(e, q->buf + (t & q->mask) * q->esize, q->esize);
  fx_compiler_barrier();
  q->tail = t + 1U;
  return true;
}
