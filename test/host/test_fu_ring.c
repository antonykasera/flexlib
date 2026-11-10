/**
 * @file test_fu_ring.c
 * @brief Host tests for fu_ring.h, against a reference FIFO
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_ring.h"
#include "test_stats.h"

/* the reference: a plain array, oldest byte first */
typedef struct {
  uint8_t d[4096];
  size_t n;
} Model;

static void model_push(Model *m, const uint8_t *s, size_t n) {
  memcpy(m->d + m->n, s, n);
  m->n += n;
}

static void model_pop(Model *m, size_t n) {
  memmove(m->d, m->d + n, m->n - n);
  m->n -= n;
}

static size_t min_sz(size_t a, size_t b) { return a < b ? a : b; }

static void ring_ops(size_t size, uint32_t start, uint64_t seed) {
  static uint8_t buf[256];
  FU_Ring r;
  Model m = {.n = 0U};
  fu_ring_init(&r, buf, size);
  r.head = start; /* free-running indices near the 2^32 wrap */
  r.tail = start;
  uint8_t src[600];
  uint8_t dst[600];
  uint8_t next = 0U;
  for (int it = 0; it < 20000; it++) {
    uint32_t u = test_rng_u32(&seed);
    size_t n = (u >> 8) % (2U * size + 2U);
    switch (u % 7U) {
    case 0: { /* write */
      for (size_t i = 0U; i < n; i++) {
        src[i] = next++;
      }
      size_t k = fu_ring_write(&r, src, n);
      ASSERT_EQ(k, min_sz(n, size - m.n));
      model_push(&m, src, k);
      next = (uint8_t)(next - (n - k)); /* unwritten bytes come again */
      break;
    }
    case 1: { /* read */
      size_t k = fu_ring_read(&r, dst, n);
      ASSERT_EQ(k, min_sz(n, m.n));
      ASSERT_TRUE(memcmp(dst, m.d, k) == 0);
      model_pop(&m, k);
      break;
    }
    case 2: { /* peek leaves it there */
      size_t k = fu_ring_peek(&r, dst, n);
      ASSERT_EQ(k, min_sz(n, m.n));
      ASSERT_TRUE(memcmp(dst, m.d, k) == 0);
      break;
    }
    case 3: { /* skip */
      size_t k = min_sz(n, m.n);
      fu_ring_skip(&r, k);
      model_pop(&m, k);
      break;
    }
    case 4: { /* put */
      bool ok = fu_ring_put(&r, next);
      ASSERT_EQ(ok, m.n < size);
      if (ok) {
        model_push(&m, &next, 1U);
        next++;
      }
      break;
    }
    case 5: { /* get */
      uint8_t b = 0U;
      bool ok = fu_ring_get(&r, &b);
      ASSERT_EQ(ok, m.n > 0U);
      if (ok) {
        ASSERT_EQ(b, m.d[0]);
        model_pop(&m, 1U);
      }
      break;
    }
    default: { /* span: the front of the data, up to the end of buf */
      size_t len = 0U;
      const uint8_t *p = fu_ring_span(&r, &len);
      ASSERT_EQ(len, min_sz(m.n, size - ((r.tail) & (size - 1U))));
      ASSERT_TRUE(len == 0U || memcmp(p, m.d, len) == 0);
      break;
    }
    }
    ASSERT_EQ(fu_ring_used(&r), m.n);
    ASSERT_EQ(fu_ring_free(&r), size - m.n);
  }
}

void test_fu_ring_vs_model(void) {
  static const size_t sizes[] = {1U, 2U, 16U, 64U, 256U};
  for (size_t i = 0U; i < sizeof sizes / sizeof sizes[0]; i++) {
    ring_ops(sizes[i], 0U, 11U + i);
    ring_ops(sizes[i], 0xFFFFFFFFU - 37U, 21U + i);
  }
}

void test_fu_queue_vs_model(void) {
  static const size_t esizes[] = {1U, 3U, 12U, 33U};
  static const size_t counts[] = {1U, 4U, 8U};
  static uint8_t buf[8 * 33];
  uint64_t seed = 31U;
  for (size_t a = 0U; a < 4U; a++) {
    for (size_t b = 0U; b < 3U; b++) {
      size_t es = esizes[a];
      size_t cnt = counts[b];
      FU_Queue q;
      Model m = {.n = 0U};
      fu_queue_init(&q, buf, cnt, es);
      q.head = 0xFFFFFFFEU;
      q.tail = 0xFFFFFFFEU;
      uint8_t e[33];
      uint8_t next = 0U;
      for (int it = 0; it < 5000; it++) {
        if ((test_rng_u32(&seed) & 1U) != 0U) {
          for (size_t i = 0U; i < es; i++) {
            e[i] = (uint8_t)(next + i);
          }
          bool ok = fu_queue_push(&q, e);
          ASSERT_EQ(ok, m.n / es < cnt);
          if (ok) {
            model_push(&m, e, es);
            next = (uint8_t)(next + es);
          }
        } else {
          bool ok = fu_queue_pop(&q, e);
          ASSERT_EQ(ok, m.n != 0U);
          if (ok) {
            ASSERT_TRUE(memcmp(e, m.d, es) == 0);
            model_pop(&m, es);
          }
        }
        ASSERT_EQ(fu_queue_used(&q), m.n / es);
      }
    }
  }
}

/* A circular DMA writes a byte stream into dbuf */
void test_fu_ring_from_dma(void) {
  static const size_t dsizes[] = {2U, 7U, 16U, 37U};
  uint64_t seed = 41U;
  for (size_t a = 0U; a < 4U; a++) {
    size_t ds = dsizes[a];
    uint8_t dbuf[37];
    uint8_t rbuf[16];
    FU_Ring r;
    fu_ring_init(&r, rbuf, sizeof rbuf);
    size_t w = 0U;      /* DMA write position */
    size_t pos = 0U;    /* from_dma's read position */
    size_t pending = 0U; /* written by the DMA, not yet moved */
    uint8_t in = 0U;
    uint8_t out = 0U;
    size_t total = 0U;
    for (int it = 0; it < 20000; it++) {
      uint32_t u = test_rng_u32(&seed);
      /* the DMA never laps the reader: at most ds - 1 bytes pending */
      size_t burst = min_sz((u >> 4) % 9U, ds - 1U - pending);
      for (size_t i = 0U; i < burst; i++) {
        dbuf[w] = in++;
        w = (w + 1U) % ds;
      }
      pending += burst;
      /* at w = 0 the count register reads ds, or 0 just before the reload */
      size_t remaining = w == 0U && (u & 1U) != 0U ? 0U : ds - w;
      size_t k = fu_ring_from_dma(&r, dbuf, ds, &pos, remaining);
      ASSERT_EQ(k, min_sz(pending, sizeof rbuf - fu_ring_used(&r) + k));
      pending -= k;
      ASSERT_EQ(pos, (w + ds - pending) % ds);
      uint8_t got[16];
      size_t n = fu_ring_read(&r, got, (u >> 12) % 12U);
      for (size_t i = 0U; i < n; i++) {
        ASSERT_EQ(got[i], out);
        out++;
      }
      total += n;
    }
    ASSERT_TRUE(total > 10000U); /* data flowed */
  }
}
