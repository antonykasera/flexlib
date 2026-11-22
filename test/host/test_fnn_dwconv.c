/**
 * @file test_fnn_dwconv.c
 * @brief Host tests for fnn_dwconv.h against TFLite's reference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dwconv.h"
#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

#define MODE ((RefRound)FNN_ROUNDING)
#define XMAX 4096U
#define CMAX 80U

static int8_t x[XMAX], w[25U * CMAX], y[XMAX + 1U], want[XMAX];
static int32_t b[CMAX], bf[CMAX], mult[CMAX], shift[CMAX];
static int8_t rsh[CMAX];

typedef struct {
  uint32_t ih, iw, ic, m, kh, kw, sh, sw;
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

static unsigned run(const Case *t, uint64_t *r, int *ran) {
  uint32_t oh, ow, pt, pl, oc = t->ic * t->m;
  *ran = 0;
  if (!dims(t->ih, t->kh, t->sh, t->same, &oh, &pt) ||
      !dims(t->iw, t->kw, t->sw, t->same, &ow, &pl)) {
    return 0U;
  }
  uint32_t nx = t->ih * t->iw * t->ic, ny = oh * ow * oc;
  if (nx > XMAX || ny > XMAX || oc > CMAX) {
    return 0U;
  }
  *ran = 1;
  for (uint32_t i = 0U; i < nx; i++) {
    x[i] = (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < t->kh * t->kw * oc; i++) {
    w[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  int per_channel = (int)(test_rng_u32(r) & 1U);
  uint32_t nq = per_channel ? oc : 1U;
  for (uint32_t c = 0U; c < oc; c++) {
    b[c] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
  }
  for (uint32_t c = 0U; c < nq; c++) {
    double s = (0.5 + (double)(test_rng_u32(r) % 1000U) / 1000.0) /
               (64.0 * sqrt((double)(t->kh * t->kw)));
    /* A power of two makes exact ties. */
    s = test_rng_u32(r) % 4U == 0U ? 1.0 / 256.0 : s;
    ref_quantize_multiplier(s, &mult[c], &shift[c]);
    rsh[c] = (int8_t)(-1 - shift[c]);
  }
  int32_t zx = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t zy = (int32_t)(test_rng_u32(r) % 256U) - 128;
  int32_t lo = test_rng_u32(r) % 3U == 0U ? zy : -128;
  ref_dwconv(x, t->ih, t->iw, t->ic, w, t->kh, t->kw, t->sh, t->sw, pt, pl, oh,
             ow, oc, b, zx, mult, shift, per_channel, zy, lo, 127, MODE, want);
  fnn_dwconv_fold_bias_s8(w, t->kh * t->kw, oc, b, zx, bf);
  const FNN_DwConv l = {w,
                        bf,
                        {t->ih, t->iw, t->ic},
                        {oh, ow, oc},
                        t->kh,
                        t->kw,
                        t->sh,
                        t->sw,
                        pt,
                        pl,
                        zx,
                        {mult, rsh, nq},
                        {zy, lo, 127}};
  unsigned bad = fnn_dwconv_check(&l) != FNN_OK;
  y[ny] = 55;
  fnn_dwconv_s8(&l, x, y);
  bad += y[ny] != 55;
  for (uint32_t i = 0U; i < ny; i++) {
    bad += y[i] != want[i];
  }
  return bad;
}

void test_fnn_dwconv_random(void) {
  static const uint32_t chans[] = {1, 2, 3, 4, 5, 7, 8, 9, 16, 17, 64};
  uint64_t r = 0xD3C0417U;
  unsigned bad = 0U, ran = 0U;
  for (unsigned rep = 0U; rep < 3000U; rep++) {
    Case t;
    t.ih = 1U + test_rng_u32(&r) % 7U;
    t.iw = 1U + test_rng_u32(&r) % 9U;
    t.ic = chans[test_rng_u32(&r) % 11U];
    t.m = rep % 4U == 0U ? 1U + test_rng_u32(&r) % 3U : 1U;
    t.kh = 1U + test_rng_u32(&r) % 5U;
    t.kw = 1U + test_rng_u32(&r) % 5U;
    t.sh = 1U + test_rng_u32(&r) % 2U;
    t.sw = 1U + test_rng_u32(&r) % 2U;
    t.same = (int)(test_rng_u32(&r) & 1U);
    int did;
    bad += run(&t, &r, &did);
    ran += (unsigned)did;
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(ran > 2000U);
}

/* Runs the bench's layer, a large input and filters larger than their input. */
void test_fnn_dwconv_shapes(void) {
  static const Case cs[] = {{5, 5, 64, 1, 3, 3, 1, 1, 1},
                            {12, 12, 16, 1, 3, 3, 2, 2, 1},
                            {3, 3, 5, 2, 5, 5, 1, 1, 1},
                            {1, 40, 8, 1, 1, 9, 1, 1, 1},
                            {6, 6, 6, 3, 3, 3, 1, 1, 0}};
  uint64_t r = 0xD5A9E5U;
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

void test_fnn_dwconv_check(void) {
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  const FNN_DwConv ok = {w, bf, {5, 5, 4},      {5, 5, 8},     3, 3, 1, 1, 1,
                         1, 0,  {&m1, &r1, 1U}, {0, -128, 127}};
  ASSERT_EQ(fnn_dwconv_check(&ok), FNN_OK);
  FNN_DwConv l = ok;
  l.out.c = 6U; /* not a multiple of 4 */
  ASSERT_EQ(fnn_dwconv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.pl = 3U;
  ASSERT_EQ(fnn_dwconv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.out.w = 7U;
  ASSERT_EQ(fnn_dwconv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.q.n = 4U;
  ASSERT_EQ(fnn_dwconv_check(&l), FNN_ERR_SHAPE);
  l = ok;
  l.zx = -129;
  ASSERT_EQ(fnn_dwconv_check(&l), FNN_ERR_RANGE);

  /* The fold sums each channel's taps. */
  static const int8_t fw[2 * 3] = {1, 2, 3, 10, 20, 30};
  static const int32_t fb[3] = {100, 200, 300};
  int32_t out[3];
  fnn_dwconv_fold_bias_s8(fw, 2U, 3U, fb, 2, out);
  ASSERT_EQ(out[0], 100 - 2 * 11);
  ASSERT_EQ(out[1], 200 - 2 * 22);
  ASSERT_EQ(out[2], 300 - 2 * 33);
  fnn_dwconv_fold_bias_s8(fw, 2U, 3U, NULL, -1, out);
  ASSERT_EQ(out[2], 33);
}
