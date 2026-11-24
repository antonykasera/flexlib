/**
 * @file test_fnn_stream.c
 * @brief Host tests for fnn_stream.h against the batch convolution
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dense.h"
#include "fnn_quant.h"
#include "fnn_stream.h"
#include "fx_arch.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <stddef.h>

#define TMAX 80U
#define CMAX 9U
#define OMAX 17U
#define KMAX 7U
#define LMAX (3U * KMAX + 2U)

static int8_t x[TMAX * CMAX], want[TMAX * OMAX], y[OMAX + 1U], cs[KMAX * CMAX];
static int8_t hist[LMAX * CMAX + 1U];
static int8_t w[OMAX * KMAX * CMAX], wpk[OMAX * KMAX * CMAX] FX_ALIGN(4);
static int32_t b[OMAX], m[OMAX];
static int8_t rsh[OMAX];
static unsigned moved;

static uint32_t pick(uint64_t *r, uint32_t lo, uint32_t hi) {
  return lo + test_rng_u32(r) % (hi - lo + 1U);
}

static unsigned stream(const FNN_Conv *l, uint32_t len, uint32_t t) {
  FNN_Stream s;
  unsigned bad = fnn_stream_init(&s, hist, len, l->kw, l->in.c, l->sw) != FNN_OK;
  bad += fnn_stream_conv1d_check(l, &s) != FNN_OK;
  uint32_t got = 0U, co = l->out.c;
  hist[len * l->in.c] = 55;
  y[co] = 55;
  for (uint32_t i = 0U; i < t; i++) {
    if (fnn_stream_conv1d_s8(l, &s, x + i * l->in.c, y) == 0U) {
      continue;
    }
    for (uint32_t k = 0U; k < co; k++) {
      bad += got < l->out.w && y[k] != want[got * co + k];
    }
    got++;
  }
  moved += t > len;
  return bad + (got != l->out.w) + (hist[len * l->in.c] != 55) + (y[co] != 55);
}

static unsigned run(uint64_t *r) {
  uint32_t c = pick(r, 1U, CMAX), co = pick(r, 1U, OMAX);
  uint32_t kw = pick(r, 1U, KMAX), sw = pick(r, 1U, 3U);
  uint32_t t = pick(r, kw, TMAX), n = kw * c;
  for (uint32_t i = 0U; i < t * c; i++) {
    x[i] = (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < co * n; i++) {
    w[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  fnn_dense_pack_s8(w, n, co, wpk);
  uint32_t q = test_rng_u32(r) & 1U ? co : 1U;
  for (uint32_t k = 0U; k < co; k++) {
    b[k] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
    double sc = (0.5 + (double)(test_rng_u32(r) % 1000U) / 1000.0) /
                (64.0 * sqrt((double)n));
    (void)fnn_quant_from_scale(sc, &m[k], &rsh[k]);
  }
  uint32_t packed = test_rng_u32(r) & 1U;
  int32_t zy = (int32_t)pick(r, 0U, 255U) - 128;
  const FNN_Conv l = {packed ? wpk : w, b, {1U, t, c}, {1U, (t - kw) / sw + 1U, co},
                      1U, kw, 1U, sw, 0U, 0U, packed,
                      (int32_t)pick(r, 0U, 255U) - 128, {m, rsh, q},
                      {zy, test_rng_u32(r) % 3U == 0U ? zy : -128, 127}};
  fnn_conv_s8(&l, x, want, cs);
  const uint32_t lens[4] = {kw, kw + 1U, 2U * kw, 3U * kw + 2U};
  return stream(&l, lens[test_rng_u32(r) % 4U], t);
}

void test_fnn_stream_random(void) {
  uint64_t r = 0x5E7EU;
  unsigned bad = 0U;
  moved = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run(&r);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(moved > 2000U);
}

void test_fnn_stream_push(void) {
  FNN_Stream s;
  int8_t buf[6], col[2];
  ASSERT_EQ(fnn_stream_init(&s, buf, 3U, 2U, 2U, 2U), FNN_OK);
  for (int i = 0; i < 6; i++) {
    col[0] = (int8_t)(10 * i);
    col[1] = (int8_t)(10 * i + 1);
    const int8_t *p = fnn_stream_push(&s, col);
    if (i % 2 == 0) {
      ASSERT_TRUE(p == NULL);
      continue;
    }
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ(p[0], 10 * (i - 1));
    ASSERT_EQ(p[1], 10 * (i - 1) + 1);
    ASSERT_EQ(p[2], 10 * i);
    ASSERT_EQ(p[3], 10 * i + 1);
  }
  fnn_stream_reset(&s);
  ASSERT_TRUE(fnn_stream_push(&s, col) == NULL);
  ASSERT_TRUE(fnn_stream_push(&s, col) != NULL);
  ASSERT_EQ(fnn_stream_init(&s, buf, 1U, 2U, 2U, 1U), FNN_ERR_SHAPE);
  ASSERT_EQ(fnn_stream_init(&s, buf, 3U, 0U, 2U, 1U), FNN_ERR_SHAPE);
  ASSERT_EQ(fnn_stream_init(&s, buf, 3U, 2U, 0U, 1U), FNN_ERR_SHAPE);
  ASSERT_EQ(fnn_stream_init(&s, buf, 3U, 2U, 2U, 0U), FNN_ERR_SHAPE);
}

void test_fnn_stream_conv1d_check(void) {
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  FNN_Stream s;
  const FNN_Conv l = {w, b, {1, 20, 3}, {1, 8, 4}, 1, 5, 1, 2, 0, 0, 0, 0,
                      {&m1, &r1, 1U}, {0, -128, 127}};
  ASSERT_EQ(fnn_stream_init(&s, hist, 5U, 5U, 3U, 2U), FNN_OK);
  ASSERT_EQ(fnn_stream_conv1d_check(&l, &s), FNN_OK);
  FNN_Conv a = l;
  a.zx = 200;
  ASSERT_EQ(fnn_stream_conv1d_check(&a, &s), FNN_ERR_RANGE);
  a = l;
  a.packed = FNN_PACK_S4;
  ASSERT_EQ(fnn_stream_conv1d_check(&a, &s), FNN_ERR_RANGE);
  a = l;
  a.pl = 2U;
  a.out.w = 10U;
  ASSERT_EQ(fnn_stream_conv1d_check(&a, &s), FNN_ERR_SHAPE);
  a = l;
  a.kh = 3U;
  a.pt = 1U;
  ASSERT_EQ(fnn_stream_conv1d_check(&a, &s), FNN_ERR_SHAPE);
  a = l;
  a.in.h = a.out.h = 3U;
  ASSERT_EQ(fnn_stream_conv1d_check(&a, &s), FNN_ERR_SHAPE);
  FNN_Stream t = s;
  t.n = 4U;
  ASSERT_EQ(fnn_stream_conv1d_check(&l, &t), FNN_ERR_SHAPE);
  t = s;
  t.c = 4U;
  ASSERT_EQ(fnn_stream_conv1d_check(&l, &t), FNN_ERR_SHAPE);
  t = s;
  t.stride = 1U;
  ASSERT_EQ(fnn_stream_conv1d_check(&l, &t), FNN_ERR_SHAPE);
}
