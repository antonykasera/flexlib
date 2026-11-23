/**
 * @file test_fnn_fused.c
 * @brief Host tests for fnn_fused.h against the layers run one after the
 * other
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dense.h"
#include "fnn_fused.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define XMAX 1600U
#define CMAX 24U
#define SMAX (5U * 8U * 12U + 8U * CMAX)

static int8_t x[XMAX], xy[XMAX + 1U], d[XMAX], y[XMAX + 1U], want[XMAX];
static int8_t wd[25U * CMAX], wp[CMAX * CMAX], s[SMAX + 1U], cs[CMAX];
static int8_t wpk[CMAX * CMAX] FX_ALIGN(4);
static int32_t bd[CMAX], bp[CMAX], md[CMAX], mp[CMAX];
static int8_t rd[CMAX], rp[CMAX];
static unsigned in_place;

static uint32_t pick(uint64_t *r, uint32_t lo, uint32_t hi) {
  return lo + test_rng_u32(r) % (hi - lo + 1U);
}

static void quant(uint64_t *r, uint32_t n, uint32_t taps, int32_t *m,
                  int8_t *sh) {
  for (uint32_t c = 0U; c < n; c++) {
    double sc = (0.5 + (double)(test_rng_u32(r) % 1000U) / 1000.0) /
                (64.0 * sqrt((double)taps));
    (void)fnn_quant_from_scale(sc, &m[c], &sh[c]);
  }
}

static unsigned run(uint64_t *r) {
  uint32_t h = pick(r, 1U, 8U), w = pick(r, 1U, 8U), c = pick(r, 1U, 12U);
  uint32_t cd = c * pick(r, 1U, 2U), co = pick(r, 1U, CMAX);
  uint32_t kh = pick(r, 1U, 5U), kw = pick(r, 1U, 5U);
  uint32_t nx = h * w * c, nd = h * w * cd, ny = h * w * co;
  for (uint32_t i = 0U; i < nx; i++) {
    x[i] = (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < kh * kw * cd; i++) {
    wd[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  for (uint32_t i = 0U; i < co * cd; i++) {
    wp[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  fnn_dense_pack_s8(wp, cd, co, wpk);
  for (uint32_t i = 0U; i < CMAX; i++) {
    bd[i] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
    bp[i] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
  }
  uint32_t qd = test_rng_u32(r) & 1U ? cd : 1U, qp = test_rng_u32(r) & 1U ? co : 1U;
  quant(r, qd, kh * kw, md, rd);
  quant(r, qp, cd, mp, rp);
  int32_t zd = (int32_t)pick(r, 0U, 255U) - 128, zy = (int32_t)pick(r, 0U, 255U) - 128;
  uint32_t packed = test_rng_u32(r) & 1U;
  const FNN_DwConv dw = {wd, bd, {h, w, c}, {h, w, cd}, kh, kw, 1U, 1U,
                         pick(r, 0U, kh - 1U), pick(r, 0U, kw - 1U),
                         (int32_t)pick(r, 0U, 255U) - 128, {md, rd, qd},
                         {zd, test_rng_u32(r) % 3U == 0U ? zd : -128, 127}};
  const FNN_Conv pw = {packed ? wpk : wp, bp, {h, w, cd}, {h, w, co}, 1U, 1U, 1U,
                       1U, 0U, 0U, packed, zd, {mp, rp, qp},
                       {zy, -128, test_rng_u32(r) % 3U == 0U ? 100 : 127}};
  unsigned bad = fnn_dwpw_check(&dw, &pw) != FNN_OK;
  fnn_dwconv_s8(&dw, x, d);
  fnn_conv_s8(&pw, d, want, cs);
  uint32_t need = FNN_DWPW_SCRATCH(kh, w, c, cd);
  s[need] = 55;
  y[ny] = 55;
  fnn_dwpw_s8(&dw, &pw, x, y, s);
  bad += s[need] != 55 || y[ny] != 55;
  for (uint32_t i = 0U; i < ny; i++) {
    bad += y[i] != want[i];
  }
  if (co <= c) {
    memcpy(xy, x, nx);
    xy[nx] = 55;
    fnn_dwpw_s8(&dw, &pw, xy, xy, s);
    bad += xy[nx] != 55;
    for (uint32_t i = 0U; i < ny; i++) {
      bad += xy[i] != want[i];
    }
    in_place++;
  }
  (void)nd;
  return bad;
}

void test_fnn_dwpw_random(void) {
  uint64_t r = 0xD3E1U;
  unsigned bad = 0U;
  in_place = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run(&r);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(in_place > 800U);
}

void test_fnn_dwpw_check(void) {
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  const FNN_DwConv dw = {wd, bd, {5, 5, 4}, {5, 5, 4}, 3, 3, 1, 1, 1, 1, 0,
                         {&m1, &r1, 1U}, {0, -128, 127}};
  const FNN_Conv pw = {wp, bp, {5, 5, 4}, {5, 5, 8}, 1, 1, 1, 1, 0, 0, 0, 0,
                       {&m1, &r1, 1U}, {0, -128, 127}};
  ASSERT_EQ(fnn_dwpw_check(&dw, &pw), FNN_OK);
  FNN_DwConv a = dw;
  a.sh = 2U;
  a.out.h = 3U;
  ASSERT_EQ(fnn_dwpw_check(&a, &pw), FNN_ERR_SHAPE);
  a = dw;
  a.zx = 200;
  ASSERT_EQ(fnn_dwpw_check(&a, &pw), FNN_ERR_RANGE);
  FNN_Conv b = pw;
  b.kh = b.kw = 3U;
  b.pt = b.pl = 1U;
  ASSERT_EQ(fnn_dwpw_check(&dw, &b), FNN_ERR_SHAPE);
  b = pw;
  b.in.c = 8U;
  ASSERT_EQ(fnn_dwpw_check(&dw, &b), FNN_ERR_SHAPE);
  b = pw;
  b.out.w = 4U;
  ASSERT_EQ(fnn_dwpw_check(&dw, &b), FNN_ERR_SHAPE);
}
