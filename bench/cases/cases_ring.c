/**
 * @file cases_ring.c
 * @brief Bench cases for fu_ring.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_bits.h"
#include "fu_ring.h"

/* in bench_scratch: the ring's 256 B, src 64, dst 64, the queue's 96, dma 128 */
static uint8_t *const src = bench_scratch + 256;
static uint8_t *const dst = bench_scratch + 320;
static uint8_t *const dma = bench_scratch + 480;
_Static_assert(608U <= BENCH_SCRATCH_SIZE, "ring data");
static FU_Ring ring = {bench_scratch, 255U, 0U, 0U};

/* ---- candidates: fu_ring_write / fu_ring_read with another copy ---- */

/* one word per step, not unrolled (FX_OPAQUE: GCC made memcpy calls of it) */
FX_INLINE void copy_w(uint8_t *d, const uint8_t *s, size_t n) {
  for (; n >= 4U; n -= 4U, d += 4, s += 4) {
    FX_OPAQUE(d);
    fu_store_le32(d, fu_load_le32(s));
  }
  for (; n != 0U; n--) {
    FX_OPAQUE(d);
    *d++ = *s++;
  }
}

#define RING_RW(pfx, COPY)                                                     \
  BENCH_CALL static size_t pfx##_write(FU_Ring *r, const void *in,             \
                                       size_t n) {                             \
    FX_ASSERT(in != NULL || n == 0U);                                          \
    uint32_t h = r->head;                                                      \
    size_t room = r->mask + 1U - (h - r->tail);                                \
    n = n < room ? n : room;                                                   \
    size_t off = h & r->mask;                                                  \
    size_t first = r->mask + 1U - off;                                         \
    first = first < n ? first : n;                                             \
    COPY(r->buf + off, (const uint8_t *)in, first);                            \
    if (n != first) {                                                          \
      COPY(r->buf, (const uint8_t *)in + first, n - first);                    \
    }                                                                          \
    fx_compiler_barrier();                                                     \
    r->head = h + (uint32_t)n;                                                 \
    return n;                                                                  \
  }                                                                            \
  BENCH_CALL static size_t pfx##_read(FU_Ring *r, void *to, size_t n) {        \
    FX_ASSERT(to != NULL || n == 0U);                                          \
    uint32_t t = r->tail;                                                      \
    size_t avail = r->head - t;                                                \
    n = n < avail ? n : avail;                                                 \
    fx_compiler_barrier();                                                     \
    size_t off = t & r->mask;                                                  \
    size_t first = r->mask + 1U - off;                                         \
    first = first < n ? first : n;                                             \
    COPY((uint8_t *)to, r->buf + off, first);                                  \
    if (n != first) {                                                          \
      COPY((uint8_t *)to + first, r->buf, n - first);                          \
    }                                                                          \
    fx_compiler_barrier();                                                     \
    r->tail = t + (uint32_t)n;                                                 \
    return n;                                                                  \
  }                                                                            \
  BENCH_CALL static uint32_t rt_##pfx(uint32_t off, size_t n) {                \
    ring.head = off;                                                           \
    ring.tail = off;                                                           \
    size_t k = pfx##_write(&ring, src, n);                                     \
    return (uint32_t)(k + pfx##_read(&ring, dst, n));                          \
  }

RING_RW(word, copy_w)
RING_RW(memcpy, memcpy)

/* what a byte-oriented driver does: put and get one at a time */
BENCH_CALL static uint32_t rt_bytes(uint32_t off, size_t n) {
  ring.head = off;
  ring.tail = off;
  size_t k = 0U;
  for (size_t i = 0U; i < n; i++) {
    k += fu_ring_put(&ring, src[i]) ? 1U : 0U;
  }
  for (size_t i = 0U; i < n; i++) {
    k += fu_ring_get(&ring, &dst[i]) ? 1U : 0U;
  }
  return (uint32_t)k;
}

BENCH_CALL static uint32_t rt_lib(uint32_t off, size_t n) {
  ring.head = off;
  ring.tail = off;
  size_t k = fu_ring_write(&ring, src, n);
  return (uint32_t)(k + fu_ring_read(&ring, dst, n));
}

static inline uint32_t put_get(uint32_t off) {
  ring.head = off;
  ring.tail = off;
  uint8_t b = 0U;
  (void)fu_ring_put(&ring, (uint8_t)off);
  (void)fu_ring_get(&ring, &b);
  return b;
}

/* 12-byte elements */
static FU_Queue queue = {bench_scratch + 384, 7U, 12U, 0U, 0U};

BENCH_CALL static uint32_t q_lib(uint32_t off) {
  queue.head = off;
  queue.tail = off;
  uint32_t k = fu_queue_push(&queue, src) ? 1U : 0U;
  return k + (fu_queue_pop(&queue, dst) ? 1U : 0U);
}

/* 64 new bytes in a 128-byte circular DMA buffer, moved and read dst */

BENCH_CALL static uint32_t dma_lib(uint32_t off) {
  ring.head = off;
  ring.tail = off;
  size_t pos = off & 127U;
  size_t remaining = 128U - ((pos + 64U) & 127U);
  size_t k = fu_ring_from_dma(&ring, dma, 128U, &pos, remaining);
  return (uint32_t)(k + fu_ring_read(&ring, dst, 64U));
}

/* ---- inputs: the ring position, so 1 in 4 transfers of 64 B wraps ---- */

static inline uint32_t in_off(uint32_t u) {
  static const char id = 0;
  (void)bench_scratch_claim(&id);
  for (size_t i = 0U; i < 64U; i++) {
    src[i] = (uint8_t)(i * 7U + 3U);
  }
  for (size_t i = 0U; i < 128U; i++) {
    dma[i] = (uint8_t)(i * 13U + 5U);
  }
  return u;
}

#define OUT_HASH h = bench_hash(h, dst, 64U)

BENCH_UNARY_POST(b_rt64, uint32_t, in_off, uint32_t, rt_lib(x, 64U), OUT_HASH)
BENCH_UNARY_POST(b_rt64_word, uint32_t, in_off, uint32_t, rt_word(x, 64U),
                 OUT_HASH)
BENCH_UNARY_POST(b_rt64_memcpy, uint32_t, in_off, uint32_t,
                 rt_memcpy(x, 64U), OUT_HASH)
BENCH_UNARY_POST(b_rt64_bytes, uint32_t, in_off, uint32_t, rt_bytes(x, 64U),
                 OUT_HASH)
BENCH_UNARY_POST(b_rt8, uint32_t, in_off, uint32_t, rt_lib(x, 8U), OUT_HASH)
BENCH_UNARY_POST(b_rt8_memcpy, uint32_t, in_off, uint32_t, rt_memcpy(x, 8U),
                 OUT_HASH)
BENCH_UNARY_POST(b_rt8_bytes, uint32_t, in_off, uint32_t, rt_bytes(x, 8U),
                 OUT_HASH)
BENCH_UNARY(b_putget, uint32_t, in_off, uint32_t, put_get(x))
BENCH_UNARY_POST(b_queue, uint32_t, in_off, uint32_t, q_lib(x), OUT_HASH)
BENCH_UNARY_POST(b_dma, uint32_t, in_off, uint32_t, dma_lib(x), OUT_HASH)

/* the copies of one size share a hash */
static const BenchCase cases[] = {
    {"fu_ring write + read, 64 B", "flexlib", b_rt64, true},
    {"ring, word copy not unrolled, 64 B", "candidate", b_rt64_word, true},
    {"ring, newlib memcpy, 64 B", "candidate", b_rt64_memcpy, true},
    {"ring, put + get per byte, 64 B", "candidate", b_rt64_bytes, true},
    {"fu_ring write + read, 8 B", "flexlib", b_rt8, true},
    {"ring, newlib memcpy, 8 B", "candidate", b_rt8_memcpy, true},
    {"ring, put + get per byte, 8 B", "candidate", b_rt8_bytes, true},
    {"fu_ring_put + fu_ring_get (inline)", "flexlib", b_putget, true},
    {"fu_queue push + pop, 12 B", "flexlib", b_queue, true},
    {"fu_ring_from_dma + read, 64 B", "flexlib", b_dma, true},
};

const BenchGroup bench_group_ring = {cases, BENCH_COUNT(cases)};
