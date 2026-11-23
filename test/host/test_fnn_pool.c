/**
 * @file test_fnn_pool.c
 * @brief Host tests for fnn_pool.h against TFLite's reference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_pool.h"
#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define MODE REF_MODE
#define XMAX 4096U

static int8_t x[XMAX], y[XMAX + 1U], z[XMAX + 1U], want[XMAX];
static unsigned in_place;

typedef struct {
  uint32_t ih, iw, c, kh, kw, sh, sw;
  int same;
} Case;

static int dims(uint32_t in, uint32_t k, uint32_t s, int same, uint32_t *out,
                uint32_t *pad) {
  if (same) {
    *out = (in + s - 1U) / s;
    uint32_t need = (*out - 1U) * s + k;
    *pad = need > in ? (need - in) / 2U : 0U;
    return 1;
  }
  if (in < k) {
    return 0;
  }
  *out = (in - k) / s + 1U;
  *pad = 0U;
  return 1;
}

/* Runs both pools, on a new x when fill is set, and in place over a copy of x
 * when nothing pads the window above or to the left. */
static unsigned run(const Case *t, uint64_t *r, int fill, int *ran) {
  uint32_t oh, ow, pt, pl;
  *ran = 0;
  if (!dims(t->ih, t->kh, t->sh, t->same, &oh, &pt) ||
      !dims(t->iw, t->kw, t->sw, t->same, &ow, &pl)) {
    return 0U;
  }
  uint32_t nx = t->ih * t->iw * t->c, ny = oh * ow * t->c;
  if (nx > XMAX || ny > XMAX) {
    return 0U;
  }
  *ran = 1;
  for (uint32_t i = 0U; fill && i < nx; i++) {
    x[i] = (int8_t)test_rng_u32(r);
  }
  int32_t lo = -128, hi = 127;
  if (test_rng_u32(r) % 3U == 0U) {
    lo = (int32_t)(test_rng_u32(r) % 200U) - 128;
    hi = lo + (int32_t)(test_rng_u32(r) % (uint32_t)(128 - lo));
  }
  const FNN_Pool p = {{t->ih, t->iw, t->c},
                      {oh, ow, t->c},
                      t->kh,
                      t->kw,
                      t->sh,
                      t->sw,
                      pt,
                      pl,
                      lo,
                      hi};
  unsigned bad = fnn_pool_check(&p) != FNN_OK;
  for (int avg = 0; avg < 2; avg++) {
    ref_pool(x, t->ih, t->iw, t->c, t->kh, t->kw, t->sh, t->sw, pt, pl, oh, ow,
             avg, lo, hi, want);
    y[ny] = 55;
    if (avg) {
      fnn_avgpool_s8(&p, x, y);
    } else {
      fnn_maxpool_s8(&p, x, y);
    }
    bad += y[ny] != 55;
    for (uint32_t i = 0U; i < ny; i++) {
      bad += y[i] != want[i];
    }
    if (pt == 0U && pl == 0U) {
      memcpy(z, x, nx);
      z[nx] = 55;
      if (avg) {
        fnn_avgpool_s8(&p, z, z);
      } else {
        fnn_maxpool_s8(&p, z, z);
      }
      bad += z[nx] != 55;
      for (uint32_t i = 0U; i < ny; i++) {
        bad += z[i] != want[i];
      }
      in_place++;
    }
  }
  return bad;
}

void test_fnn_pool_random(void) {
  static const uint32_t chans[] = {1, 2, 3, 4, 5, 7, 8, 9, 16, 17, 64};
  uint64_t r = 0x9001U;
  unsigned bad = 0U, ran = 0U;
  in_place = 0U;
  for (unsigned rep = 0U; rep < 4000U; rep++) {
    Case t;
    t.ih = 1U + test_rng_u32(&r) % 9U;
    t.iw = 1U + test_rng_u32(&r) % 9U;
    t.c = chans[test_rng_u32(&r) % 11U];
    t.kh = 1U + test_rng_u32(&r) % 5U;
    t.kw = 1U + test_rng_u32(&r) % 5U;
    t.sh = 1U + test_rng_u32(&r) % 3U;
    t.sw = 1U + test_rng_u32(&r) % 3U;
    t.same = (int)(test_rng_u32(&r) & 1U);
    int did;
    bad += run(&t, &r, 1, &did);
    ran += (unsigned)did;
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(ran > 3000U);
  ASSERT_TRUE(in_place > 3000U);
}

/* Runs the bench's layers, global pools and the extremes. */
void test_fnn_pool_shapes(void) {
  static const Case cs[] = {{16, 16, 16, 2, 2, 2, 2, 0},
                            {5, 5, 64, 5, 5, 1, 1, 0},
                            {17, 17, 8, 17, 17, 1, 1, 0},
                            {1, 257, 4, 1, 257, 1, 1, 0},
                            {1, 258, 4, 1, 258, 1, 1, 0},
                            {2, 129, 4, 2, 129, 1, 1, 0},
                            {1, 300, 4, 1, 300, 1, 1, 0},
                            {2, 300, 5, 2, 280, 1, 7, 1}};
  uint64_t r = 0x9002U;
  unsigned bad = 0U;
  for (unsigned i = 0U; i < sizeof cs / sizeof cs[0]; i++) {
    uint32_t n = cs[i].ih * cs[i].iw * cs[i].c;
    for (unsigned rep = 0U; rep < 6U; rep++) {
      int did;
      /* All -128, all 127, then random values. */
      for (uint32_t k = 0U; rep < 2U && k < n; k++) {
        x[k] = (int8_t)(rep == 0U ? -128 : 127);
      }
      bad += run(&cs[i], &r, rep >= 2U, &did);
      bad += did == 0;
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_pool_check(void) {
  const FNN_Pool ok = {{8, 8, 4}, {4, 4, 4}, 2, 2, 2, 2, 0, 0, -128, 127};
  ASSERT_EQ(fnn_pool_check(&ok), FNN_OK);
  FNN_Pool p = ok;
  p.out.c = 5U;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_SHAPE);
  p = ok;
  p.in.c = p.out.c = 0U;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_SHAPE);
  p = ok;
  p.pt = 2U;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_SHAPE);
  p = ok;
  p.out.w = 5U;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_SHAPE);
  p = ok;
  p.lo = -129;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_RANGE);
  p = ok;
  p.hi = 128;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_RANGE);
  p = ok;
  p.lo = 10;
  p.hi = 9;
  ASSERT_EQ(fnn_pool_check(&p), FNN_ERR_RANGE);
  p.hi = 10;
  ASSERT_EQ(fnn_pool_check(&p), FNN_OK);

  /* Each pair averages to a tie, which rounds away from zero. */
  static const int8_t t[3][2] = {{1, 2}, {-1, -2}, {0, -1}};
  static const int8_t r[3] = {2, -2, -1};
  const FNN_Pool a = {{1, 2, 1}, {1, 1, 1}, 1, 2, 1, 1, 0, 0, -128, 127};
  for (unsigned i = 0U; i < 3U; i++) {
    int8_t o;
    fnn_avgpool_s8(&a, t[i], &o);
    ASSERT_EQ(o, r[i]);
  }
}

static unsigned run_mean(uint32_t h, uint32_t w, uint32_t c, double scale,
                         uint64_t *r) {
  int32_t zx = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t zy = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t lo = test_rng_u32(r) % 3U == 0U ? zy : -128;
  FNN_Mean m = {{h, w, c}, zx, 0, 0, {zy, lo, 127}};
  unsigned bad = fnn_mean_quant(scale, h * w, &m.mult, &m.rsh) != FNN_OK;
  bad += fnn_mean_check(&m) != FNN_OK;
  ref_mean(x, h, w, c, zx, scale, zy, lo, 127, MODE, want);
  y[c] = 55;
  fnn_mean_s8(&m, x, y);
  bad += y[c] != 55;
  for (uint32_t i = 0U; i < c; i++) {
    bad += y[i] != want[i];
  }
  return bad;
}

void test_fnn_mean_random(void) {
  static const uint32_t chans[] = {1, 2, 3, 4, 5, 8, 9, 16, 17, 64};
  uint64_t r = 0x9003U;
  unsigned bad = 0U;
  for (unsigned rep = 0U; rep < 3000U; rep++) {
    uint32_t h = 1U + test_rng_u32(&r) % 12U;
    uint32_t w = 1U + test_rng_u32(&r) % 12U;
    uint32_t c = chans[test_rng_u32(&r) % 10U];
    if (h * w * c > XMAX) {
      continue;
    }
    for (uint32_t i = 0U; i < h * w * c; i++) {
      x[i] = (int8_t)test_rng_u32(&r);
    }
    /* The scales are equal, a power of two, or anything up to 8. */
    double s = 1.0;
    switch (test_rng_u32(&r) % 3U) {
    case 1:
      s = ldexp(1.0, (int)(test_rng_u32(&r) % 7U) - 3);
      break;
    case 2:
      s = 0.05 + (double)(test_rng_u32(&r) % 8000U) / 1000.0;
      break;
    default:
      break;
    }
    bad += run_mean(h, w, c, s, &r);
  }
  ASSERT_EQ(bad, 0U);
}

/* Large inputs and the extremes. */
void test_fnn_mean_shapes(void) {
  static const uint32_t cs[][3] = {
      {5, 5, 64}, {17, 17, 8}, {1, 257, 4}, {1, 258, 4}, {2, 300, 5}, {1, 1, 4}};
  uint64_t r = 0x9004U;
  unsigned bad = 0U;
  for (unsigned i = 0U; i < sizeof cs / sizeof cs[0]; i++) {
    uint32_t n = cs[i][0] * cs[i][1] * cs[i][2];
    for (unsigned rep = 0U; rep < 6U; rep++) {
      for (uint32_t k = 0U; k < n; k++) {
        x[k] = (int8_t)(rep == 0U   ? -128
                        : rep == 1U ? 127
                                    : (int32_t)test_rng_u32(&r));
      }
      bad += run_mean(cs[i][0], cs[i][1], cs[i][2], rep < 4U ? 1.0 : 0.37, &r);
    }
  }
  ASSERT_EQ(bad, 0U);

  int32_t mult;
  int8_t rsh;
  ASSERT_EQ(fnn_mean_quant(1.0, 0U, &mult, &rsh), FNN_ERR_SHAPE);
  ASSERT_EQ(fnn_mean_quant(-1.0, 4U, &mult, &rsh), FNN_ERR_QUANT);
  /* 2^34 / 25 rounded down is the multiplier for 1/25. */
  ASSERT_EQ(fnn_mean_quant(1.0, 25U, &mult, &rsh), FNN_OK);
  ASSERT_EQ(mult, 687194767);
  ASSERT_EQ(rsh, 2);
  /* At a scale of 1.5 2^-32 the shift is already 30 and cannot grow. */
  ASSERT_EQ(fnn_mean_quant(0x1.8p-32, 25U, &mult, &rsh), FNN_OK);
  ASSERT_EQ(mult, 1610612736 / 25);
  ASSERT_EQ(rsh, 30);
  FNN_Mean m = {{1, 1, 1}, -128, mult, rsh, {0, -128, 127}};
  ASSERT_EQ(fnn_mean_check(&m), FNN_OK);
  m.zx = -129;
  ASSERT_EQ(fnn_mean_check(&m), FNN_ERR_RANGE);
  m.zx = 128;
  ASSERT_EQ(fnn_mean_check(&m), FNN_ERR_RANGE);
  m.zx = 0;
  m.in.w = 0U;
  ASSERT_EQ(fnn_mean_check(&m), FNN_ERR_SHAPE);
}
