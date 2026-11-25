/**
 * @file test_fnn_conv_sub.c
 * @brief Host tests for sub-byte convolution weights against the int8 kernel
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_conv.h"
#include "fnn_quant.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define XMAX 4096U
#define WMAX 16384U
#define CMAX 40U
#define NMAX 600U

static int8_t x[XMAX], w[WMAX], y[XMAX + 1U], want[XMAX];
static int8_t wp[WMAX] FX_ALIGN(4);
static int8_t scratch[FNN_CONV_SUB_SCRATCH(1, 1, NMAX) + 1U] FX_ALIGN(4);
static int8_t patch8[NMAX];
static int32_t b[CMAX], mult[CMAX];
static int8_t rsh[CMAX];
static unsigned odd, flushes;

static uint32_t pick(uint64_t *r, uint32_t lo, uint32_t hi) {
  return lo + test_rng_u32(r) % (hi - lo + 1U);
}

static unsigned run(uint64_t *r, uint32_t fmt, int extreme) {
  uint32_t kh = pick(r, 1U, 4U), kw = pick(r, 1U, 4U), sh = pick(r, 1U, 2U),
           sw = pick(r, 1U, 2U), c = pick(r, 1U, 40U), oc = pick(r, 1U, CMAX);
  uint32_t ih = pick(r, kh, 9U), iw = pick(r, kw, 9U);
  uint32_t pt = pick(r, 0U, kh - 1U), pl = pick(r, 0U, kw - 1U);
  uint32_t oh = (ih + pt - kh) / sh + 1U, ow = (iw + pl - kw) / sw + 1U;
  uint32_t n = kh * kw * c, lim = fmt == FNN_PACK_S4 ? 7U : 1U;
  if (n > NMAX || ih * iw * c > XMAX || oh * ow * oc > XMAX || n * oc > WMAX) {
    return 0U;
  }
  for (uint32_t i = 0U; i < ih * iw * c; i++) {
    x[i] = extreme ? -128 : (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < n * oc; i++) {
    w[i] = extreme ? (int8_t)-(int32_t)lim : (int8_t)((int32_t)pick(r, 0U, 2U * lim) - (int32_t)lim);
  }
  for (uint32_t o = 0U; o < oc; o++) {
    b[o] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
    double sc = (0.5 + (double)(test_rng_u32(r) % 1000U) / 1000.0) /
                (16.0 * sqrt((double)n));
    (void)fnn_quant_from_scale(sc, &mult[o], &rsh[o]);
  }
  unsigned bad = (fmt == FNN_PACK_S4 ? fnn_conv_pack_s4(w, n, oc, wp)
                                     : fnn_conv_pack_s2(w, n, oc, wp)) != FNN_OK;
  uint32_t q = test_rng_u32(r) & 1U ? oc : 1U;
  int32_t zy = (int32_t)pick(r, 0U, 255U) - 128;
  FNN_Conv l = {w, b, {ih, iw, c}, {oh, ow, oc}, kh, kw, sh, sw, pt, pl, 0U,
                (int32_t)pick(r, 0U, 255U) - 128, {mult, rsh, q},
                {zy, test_rng_u32(r) % 3U == 0U ? zy : -128, 127}};
  bad += fnn_conv_check(&l) != FNN_OK;
  fnn_conv_s8(&l, x, want, patch8);
  l.w = wp;
  l.packed = fmt;
  bad += fnn_conv_check(&l) != FNN_OK;
  uint32_t need = FNN_CONV_SUB_SCRATCH(kh, kw, c), ny = oh * ow * oc;
  scratch[need] = 55;
  y[ny] = 55;
  fnn_conv_s8(&l, x, y, scratch);
  bad += scratch[need] != 55 || y[ny] != 55;
  for (uint32_t i = 0U; i < ny; i++) {
    bad += y[i] != want[i];
  }
  odd += (oh * ow) % 2U;
  flushes += n > (fmt == FNN_PACK_S4 ? 36U : 254U);
  return bad;
}

void test_fnn_conv_s4_random(void) {
  uint64_t r = 0x54C0U;
  unsigned bad = 0U;
  odd = flushes = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run(&r, FNN_PACK_S4, k % 50U == 0U);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(odd > 300U && flushes > 1000U);
}

void test_fnn_conv_s2_random(void) {
  uint64_t r = 0x52C0U;
  unsigned bad = 0U;
  odd = flushes = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run(&r, FNN_PACK_S2, k % 50U == 0U);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(odd > 300U && flushes > 50U);
}

void test_fnn_conv_pack_sub(void) {
  const int8_t ok4[3] = {7, -7, 0}, bad4[2] = {8, 0}, ok2[3] = {1, -1, 0},
               bad2[1] = {2};
  int8_t p[16] FX_ALIGN(4);
  ASSERT_EQ(fnn_conv_pack_s4(ok4, 3U, 1U, p), FNN_OK);
  ASSERT_EQ((uint8_t)p[0], 0x07U);
  ASSERT_EQ((uint8_t)p[4], 0x09U);
  ASSERT_EQ((uint8_t)p[8], 0x00U);
  ASSERT_EQ(fnn_conv_pack_s4(bad4, 2U, 1U, p), FNN_ERR_RANGE);
  ASSERT_EQ(fnn_conv_pack_s2(ok2, 3U, 1U, p), FNN_OK);
  ASSERT_EQ((uint8_t)p[0], 0x01U);
  ASSERT_EQ((uint8_t)p[2], 0x03U);
  ASSERT_EQ((uint8_t)p[4], 0x00U);
  ASSERT_EQ(fnn_conv_pack_s2(bad2, 1U, 1U, p), FNN_ERR_RANGE);
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  FNN_Conv l = {p, NULL, {3, 3, 2}, {3, 3, 2}, 1, 1, 1, 1, 0, 0, 4U, 0,
                {&m1, &r1, 1U}, {0, -128, 127}};
  ASSERT_EQ(fnn_conv_check(&l), FNN_ERR_RANGE);
  l.packed = FNN_PACK_S2;
  ASSERT_EQ(fnn_conv_check(&l), FNN_OK);
}
