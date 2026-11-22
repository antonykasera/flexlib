/**
 * @file test_fnn_conv.c
 * @brief Host tests for fnn_conv.h against TFLite's reference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_conv.h"
#include "fnn_dense.h"
#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define MODE ((RefRound)FNN_ROUNDING)
#define XMAX 4096U
#define WMAX 12000U
#define CMAX 64U

static int8_t x[XMAX], w[WMAX], y[XMAX + 1U], want[XMAX];
static int8_t wp[WMAX] FX_ALIGN(4);
static int8_t scratch[1024];
static int32_t b[CMAX], bf[CMAX], mult[CMAX], shift[CMAX];
static int8_t rsh[CMAX];

typedef struct {
  uint32_t ih, iw, ic, oc, kh, kw, sh, sw;
  int same;
} Case;

/* Gives TFLite's output size and leading padding for VALID or SAME. */
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

static unsigned run(const Case *t, uint64_t *r, int *ran) {
  uint32_t oh, ow, pt, pl;
  *ran = 0;
  if (!dims(t->ih, t->kh, t->sh, t->same, &oh, &pt) ||
      !dims(t->iw, t->kw, t->sw, t->same, &ow, &pl)) {
    return 0U;
  }
  uint32_t k = t->kh * t->kw * t->ic, nx = t->ih * t->iw * t->ic;
  uint32_t ny = oh * ow * t->oc;
  if (nx > XMAX || ny > XMAX || k * t->oc > WMAX || k > sizeof scratch) {
    return 0U;
  }
  *ran = 1;
  for (uint32_t i = 0U; i < nx; i++) {
    x[i] = (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < k * t->oc; i++) {
    w[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  int per_channel = (int)(test_rng_u32(r) & 1U);
  uint32_t nq = per_channel ? t->oc : 1U;
  for (uint32_t c = 0U; c < t->oc; c++) {
    b[c] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
  }
  for (uint32_t c = 0U; c < nq; c++) {
    double s = (0.5 + (double)(test_rng_u32(r) % 1000U) / 1000.0) /
               (64.0 * sqrt((double)k));
    ref_quantize_multiplier(s, &mult[c], &shift[c]);
    rsh[c] = (int8_t)(-1 - shift[c]);
  }
  int32_t zx = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t zy = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t lo = test_rng_u32(r) % 3U == 0U ? zy : -128, hi = 127;
  ref_conv2d(x, t->ih, t->iw, t->ic, w, t->kh, t->kw, t->sh, t->sw, pt, pl, oh,
             ow, t->oc, b, zx, mult, shift, per_channel, zy, lo, hi, MODE,
             want);
  fnn_fold_bias_s8(w, k, t->oc, b, zx, bf);
  fnn_dense_pack_s8(w, k, t->oc, wp);
  unsigned bad = 0U;
  for (uint32_t packed = 0U; packed < 2U; packed++) {
    const FNN_Conv l = {packed ? wp : w,
                        bf,
                        {t->ih, t->iw, t->ic},
                        {oh, ow, t->oc},
                        t->kh,
                        t->kw,
                        t->sh,
                        t->sw,
                        pt,
                        pl,
                        packed,
                        zx,
                        {mult, rsh, nq},
                        {zy, lo, hi}};
    bad += fnn_conv_check(&l) != FNN_OK;
    y[ny] = 55;
    fnn_conv_s8(&l, x, y, scratch);
    bad += y[ny] != 55;
    for (uint32_t i = 0U; i < ny; i++) {
      bad += y[i] != want[i];
    }
  }
  return bad;
}

void test_fnn_conv_random(void) {
  static const uint32_t chans[] = {1, 2, 3, 4, 5, 8, 9, 16};
  static const uint32_t outs[] = {1, 3, 4, 5, 8, 9, 12, 17};
  uint64_t r = 0xC0417U;
  unsigned bad = 0U, ran = 0U;
  for (unsigned rep = 0U; rep < 3000U; rep++) {
    Case t;
    t.ih = 1U + test_rng_u32(&r) % 7U;
    t.iw = 1U + test_rng_u32(&r) % 9U;
    t.ic = chans[test_rng_u32(&r) % 8U];
    t.oc = outs[test_rng_u32(&r) % 8U];
    t.kh = 1U + test_rng_u32(&r) % 5U;
    t.kw = rep % 7U == 0U ? t.iw : 1U + test_rng_u32(&r) % 5U;
    t.sh = 1U + test_rng_u32(&r) % 2U;
    t.sw = 1U + test_rng_u32(&r) % 2U;
    t.same = (int)(test_rng_u32(&r) & 1U);
    /* Every fifth layer is a conv1d. */
    if (rep % 5U == 0U) {
      t.ih = 1U;
      t.kh = 1U;
    }
    int did;
    bad += run(&t, &r, &did);
    ran += (unsigned)did;
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(ran > 2000U);
}

/* Runs the bench's layers, and filters larger than their input. */
void test_fnn_conv_shapes(void) {
  static const Case cs[] = {
      {5, 5, 64, 64, 1, 1, 1, 1, 0},  {12, 12, 8, 16, 3, 3, 1, 1, 1},
      {1, 68, 16, 32, 1, 5, 1, 1, 0}, {10, 4, 3, 5, 10, 4, 1, 1, 0},
      {3, 3, 2, 3, 5, 5, 1, 1, 1},    {7, 7, 3, 4, 3, 3, 2, 2, 1},
      {2, 9, 5, 7, 2, 3, 2, 3, 1},    {1, 1, 9, 17, 1, 1, 1, 1, 0}};
  uint64_t r = 0x5A9E5U;
  unsigned bad = 0U;
  for (unsigned i = 0U; i < sizeof cs / sizeof cs[0]; i++) {
    for (unsigned rep = 0U; rep < 4U; rep++) {
      int did;
      bad += run(&cs[i], &r, &did);
      bad += did == 0;
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_conv_check(void) {
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  const FNN_Conv ok = {w, bf, {5, 5, 3},      {5, 5, 4},     3, 3, 1, 1, 1, 1,
                       0, 0,  {&m1, &r1, 1U}, {0, -128, 127}};
  ASSERT_EQ(fnn_conv_check(&ok), FNN_OK);
  FNN_Conv l = ok;
  l.pt = 3U; /* as much padding as filter */
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.out.h = 7U; /* the last windows miss the input */
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.out.w = 7U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.sh = 0U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.in.c = 0U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.q.n = 3U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_SHAPE);
  l.q.n = 4U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_OK);
  l = ok;
  l.zx = 128;
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_RANGE);
  /* VALID with stride 2 leaves the last column unread, which is allowed */
  l = ok;
  l.pt = 0U;
  l.pl = 0U;
  l.sh = 2U;
  l.sw = 2U;
  l.in.h = 6U;
  l.in.w = 6U;
  l.out.h = 2U;
  l.out.w = 2U;
  ASSERT_EQ(fnn_conv_check(&l), FNN_OK);
}
