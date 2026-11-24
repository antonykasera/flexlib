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
#define SMAX FNN_DWPW_SUB_SCRATCH(5U, 8U, 12U, CMAX)

static int8_t x[XMAX], xy[XMAX + 1U], d[XMAX], y[XMAX + 1U], want[XMAX];
static int8_t wd[25U * CMAX], wp[CMAX * CMAX], s[SMAX + 4U], cs[CMAX];
static int8_t wpk[CMAX * CMAX] FX_ALIGN(4);
static int32_t bd[CMAX], bp[CMAX], md[CMAX], mp[CMAX];
static int8_t rd[CMAX], rp[CMAX];
static unsigned in_place, sub;

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
  uint32_t packed = test_rng_u32(r) % 4U;
  int32_t lim = packed == FNN_PACK_S4 ? 7 : packed == FNN_PACK_S2 ? 1 : 127;
  for (uint32_t i = 0U; i < co * cd; i++) {
    wp[i] = (int8_t)(-lim + (int32_t)(test_rng_u32(r) % (2U * (uint32_t)lim + 1U)));
  }
  if (packed == FNN_PACK_S4) {
    (void)fnn_conv_pack_s4(wp, cd, co, wpk);
  } else if (packed == FNN_PACK_S2) {
    (void)fnn_conv_pack_s2(wp, cd, co, wpk);
  } else {
    fnn_dense_pack_s8(wp, cd, co, wpk);
  }
  for (uint32_t i = 0U; i < CMAX; i++) {
    bd[i] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
    bp[i] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
  }
  uint32_t qd = test_rng_u32(r) & 1U ? cd : 1U, qp = test_rng_u32(r) & 1U ? co : 1U;
  quant(r, qd, kh * kw, md, rd);
  quant(r, qp, cd, mp, rp);
  int32_t zd = (int32_t)pick(r, 0U, 255U) - 128, zy = (int32_t)pick(r, 0U, 255U) - 128;
  const FNN_DwConv dw = {wd, bd, {h, w, c}, {h, w, cd}, kh, kw, 1U, 1U,
                         pick(r, 0U, kh - 1U), pick(r, 0U, kw - 1U),
                         (int32_t)pick(r, 0U, 255U) - 128, {md, rd, qd},
                         {zd, test_rng_u32(r) % 3U == 0U ? zd : -128, 127}};
  const FNN_Conv pw = {packed ? wpk : wp, bp, {h, w, cd}, {h, w, co}, 1U, 1U, 1U,
                       1U, 0U, 0U, packed, zd, {mp, rp, qp},
                       {zy, -128, test_rng_u32(r) % 3U == 0U ? 100 : 127}};
  FNN_Conv rows = pw;
  rows.w = wp;
  rows.packed = 0U;
  unsigned bad = fnn_dwpw_check(&dw, &pw) != FNN_OK;
  fnn_dwconv_s8(&dw, x, d);
  fnn_conv_s8(&rows, d, want, cs);
  uint32_t need = packed >= FNN_PACK_S4 ? FNN_DWPW_SUB_SCRATCH(kh, w, c, cd)
                                        : FNN_DWPW_SCRATCH(kh, w, c, cd);
  int8_t *sc = s + pick(r, 0U, 3U);
  sc[need] = 55;
  y[ny] = 55;
  fnn_dwpw_s8(&dw, &pw, x, y, sc);
  bad += sc[need] != 55 || y[ny] != 55;
  sub += packed >= FNN_PACK_S4;
  for (uint32_t i = 0U; i < ny; i++) {
    bad += y[i] != want[i];
  }
  if (co <= c) {
    memcpy(xy, x, nx);
    xy[nx] = 55;
    fnn_dwpw_s8(&dw, &pw, xy, xy, sc);
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
  in_place = sub = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run(&r);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(in_place > 800U && sub > 1200U);
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
  b = pw;
  b.packed = FNN_PACK_S2;
  ASSERT_EQ(fnn_dwpw_check(&dw, &b), FNN_OK);
}

#define CVMAX (13U * 13U * CMAX)
#define PATCH (4U * 4U * 6U)
static int8_t cx[10U * 10U * 6U], cvo[CVMAX], cy[CVMAX + 1U], cwant[CVMAX];
static int8_t cs2[3U * 13U * CMAX + PATCH + 1U], cpatch[PATCH];
static int8_t cw[CMAX * PATCH], cwpk[CMAX * PATCH] FX_ALIGN(4);
static int32_t cb[CMAX], cm[CMAX];
static int8_t cr[CMAX];
static unsigned overlap, padded;

static uint32_t outs(uint64_t *r, uint32_t in, uint32_t st, uint32_t p) {
  return pick(r, 1U, (in + p - 1U) / st + 1U);
}

static unsigned run_convpool(uint64_t *r) {
  uint32_t h = pick(r, 1U, 10U), w = pick(r, 1U, 10U), c = pick(r, 1U, 6U);
  uint32_t kh = pick(r, 1U, 4U), kw = pick(r, 1U, 4U), oc = pick(r, 1U, CMAX);
  uint32_t sh = pick(r, 1U, 2U), sw = pick(r, 1U, 2U);
  uint32_t pt = pick(r, 0U, kh - 1U), pl = pick(r, 0U, kw - 1U);
  uint32_t oh = outs(r, h, sh, pt), ow = outs(r, w, sw, pl);
  uint32_t pkh = pick(r, 1U, 3U), pkw = pick(r, 1U, 3U);
  uint32_t psh = pick(r, 1U, 3U), psw = pick(r, 1U, 3U);
  uint32_t ppt = pick(r, 0U, pkh - 1U), ppl = pick(r, 0U, pkw - 1U);
  uint32_t ph = outs(r, oh, psh, ppt), pw2 = outs(r, ow, psw, ppl);
  uint32_t n = kh * kw * c, ny = ph * pw2 * oc;
  for (uint32_t i = 0U; i < h * w * c; i++) {
    cx[i] = (int8_t)test_rng_u32(r);
  }
  for (uint32_t i = 0U; i < oc * n; i++) {
    cw[i] = (int8_t)(-127 + (int32_t)(test_rng_u32(r) % 255U));
  }
  fnn_dense_pack_s8(cw, n, oc, cwpk);
  for (uint32_t i = 0U; i < oc; i++) {
    cb[i] = (int32_t)(test_rng_u32(r) % 40001U) - 20000;
  }
  uint32_t q = test_rng_u32(r) & 1U ? oc : 1U, packed = test_rng_u32(r) & 1U;
  quant(r, q, n, cm, cr);
  int32_t zy = (int32_t)pick(r, 0U, 255U) - 128;
  int32_t lo = (int32_t)pick(r, 0U, 100U) - 128, hi = 127 - (int32_t)pick(r, 0U, 100U);
  const FNN_Conv cv = {packed ? cwpk : cw, cb, {h, w, c}, {oh, ow, oc}, kh, kw, sh,
                       sw, pt, pl, packed, (int32_t)pick(r, 0U, 255U) - 128,
                       {cm, cr, q}, {zy, test_rng_u32(r) % 3U == 0U ? zy : -128, 127}};
  const FNN_Pool po = {{oh, ow, oc}, {ph, pw2, oc}, pkh, pkw, psh, psw, ppt, ppl, lo, hi};
  unsigned bad = fnn_convpool_check(&cv, &po) != FNN_OK;
  fnn_conv_s8(&cv, cx, cvo, cpatch);
  fnn_maxpool_s8(&po, cvo, cwant);
  uint32_t need = FNN_CONVPOOL_SCRATCH(pkh, ow, oc, kh, kw, c);
  cs2[need] = 55;
  cy[ny] = 55;
  fnn_convpool_s8(&cv, &po, cx, cy, cs2);
  bad += cs2[need] != 55 || cy[ny] != 55;
  for (uint32_t i = 0U; i < ny; i++) {
    bad += cy[i] != cwant[i];
  }
  overlap += psh < pkh && ph > 1U;
  padded += ppt > 0U;
  return bad;
}

void test_fnn_convpool_random(void) {
  uint64_t r = 0xC0F1U;
  unsigned bad = 0U;
  overlap = padded = 0U;
  for (unsigned k = 0U; k < 3000U; k++) {
    bad += run_convpool(&r);
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(overlap > 400U && padded > 1000U);
}

void test_fnn_convpool_check(void) {
  int32_t m1 = 1 << 30;
  int8_t r1 = 10;
  const FNN_Conv cv = {cw, cb, {8, 8, 2}, {8, 8, 4}, 3, 3, 1, 1, 1, 1, 0, 0,
                       {&m1, &r1, 1U}, {0, -128, 127}};
  const FNN_Pool po = {{8, 8, 4}, {4, 4, 4}, 2, 2, 2, 2, 0, 0, -128, 127};
  ASSERT_EQ(fnn_convpool_check(&cv, &po), FNN_OK);
  FNN_Conv a = cv;
  a.zx = 200;
  ASSERT_EQ(fnn_convpool_check(&a, &po), FNN_ERR_RANGE);
  a = cv;
  a.packed = FNN_PACK_S4;
  ASSERT_EQ(fnn_convpool_check(&a, &po), FNN_ERR_RANGE);
  FNN_Pool b = po;
  b.lo = 10;
  b.hi = 5;
  ASSERT_EQ(fnn_convpool_check(&cv, &b), FNN_ERR_RANGE);
  b = po;
  b.in.h = b.in.w = 7U;
  b.out.h = b.out.w = 3U;
  ASSERT_EQ(fnn_convpool_check(&cv, &b), FNN_ERR_SHAPE);
  b = po;
  b.in.h = 7U;
  b.out.h = 3U;
  ASSERT_EQ(fnn_convpool_check(&cv, &b), FNN_ERR_SHAPE);
  b = po;
  b.in.c = b.out.c = 2U;
  ASSERT_EQ(fnn_convpool_check(&cv, &b), FNN_ERR_SHAPE);
}
