/**
 * @file fnn_elem.c
 * @brief Element-wise layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_elem.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include <math.h>

static int s8(int32_t v) { return v >= -128 && v <= 127; }

static int scale_ok(double s) { return s > 0.0 && isfinite(s); }

#if FNN_RULE != FNN_ROUND_GEMMLOWP
/* Doubles instead of calling ldexp, which brings in newlib's errno data. */
static double pow2(int32_t e) {
  double p = 1.0;
  for (; e > 0; e--) {
    p *= 2.0;
  }
  return p;
}
#endif

FNN_Status fnn_add_init(FNN_Add *a, double s1, int32_t z1, double s2,
                        int32_t z2, double sy, FNN_Out o) {
  if (!scale_ok(s1) || !scale_ok(s2) || !scale_ok(sy)) {
    return FNN_ERR_QUANT;
  }
  double big = s1 > s2 ? s1 : s2;
  if (big / sy >= 524288.0) {
    return FNN_ERR_QUANT;
  }
  if (!s8(z1) || !s8(z2) || !s8(o.zp) || !s8(o.lo) || !s8(o.hi) ||
      o.lo > o.hi) {
    return FNN_ERR_RANGE;
  }
  a->z1 = z1;
  a->z2 = z2;
  a->o = o;
  int8_t r[3];
#if FNN_RULE == FNN_ROUND_GEMMLOWP
  /* TFLite scales both inputs, shifted up 20 bits, to half the larger scale. */
  double twice = 2.0 * big;
  (void)fnn_quant_from_scale(s1 / twice, &a->m[0], &r[0]);
  (void)fnn_quant_from_scale(s2 / twice, &a->m[1], &r[1]);
  (void)fnn_quant_from_scale(twice / (1048576.0 * sy), &a->m[2], &r[2]);
  for (unsigned i = 0U; i < 3U; i++) {
    a->sh[i] = (int8_t)(r[i] + 1);
  }
#else
  /* Both multipliers share the larger one's shift. */
  double m1 = s1 / sy, m2 = s2 / sy;
  int32_t *mbig = s1 > s2 ? &a->m[0] : &a->m[1];
  int32_t *msmall = s1 > s2 ? &a->m[1] : &a->m[0];
  (void)fnn_quant_from_scale(m1 > m2 ? m1 : m2, mbig, &r[0]);
  double small = (m1 > m2 ? m2 : m1) * pow2(32 + r[0]);
  *msmall = (int32_t)(int64_t)(small + 0.5);
  if (r[0] > 22) {
    /* Every sum is then under half a step, and rounds to 0. */
    a->m[0] = a->m[1] = 0;
    r[0] = 0;
  }
  a->sh[0] = (int8_t)(r[0] < 0 ? -r[0] : 0);
  a->sh[1] = (int8_t)(r[0] > 0 ? r[0] : 0);
  a->sh[2] = 0;
  a->m[2] = 0;
#endif
  return FNN_OK;
}

#if FNN_RULE == FNN_ROUND_GEMMLOWP
/* Applies gemmlowp's doubling multiply, then its division by 2^e. */
FX_INLINE int32_t mbq(int32_t x, int32_t m, uint32_t e) {
  int32_t v = (int32_t)(fx_smlal(1LL << 30, x, m) >> 31);
  uint32_t mask = (1U << e) - 1U;
  uint32_t thr = (mask >> 1) + (uint32_t)(v < 0);
  return (v >> e) + (int32_t)(((uint32_t)v & mask) > thr);
}

void fnn_add_s8(const FNN_Add *a, const int8_t *x1, const int8_t *x2,
                int8_t *y, uint32_t n) {
  int32_t z1 = a->z1, z2 = a->z2, m1 = a->m[0], m2 = a->m[1], mo = a->m[2];
  uint32_t e1 = (uint32_t)a->sh[0], e2 = (uint32_t)a->sh[1];
  uint32_t eo = (uint32_t)a->sh[2];
  int32_t zp = a->o.zp, lo = a->o.lo, hi = a->o.hi;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v1 = mbq((x1[i] - z1) * (1 << 20), m1, e1);
    int32_t v2 = mbq((x2[i] - z2) * (1 << 20), m2, e2);
    y[i] = fnn_clamp_s8(mbq(v1 + v2, mo, eo), zp, lo, hi);
  }
}
#else
/* Adds with the shift on the inputs (left) or on the sum. */
FX_INLINE void add_run(const FNN_Add *a, const int8_t *x1, const int8_t *x2,
                       int8_t *y, uint32_t n, int left) {
  int32_t m1 = a->m[0], m2 = a->m[1], lo = a->o.lo, hi = a->o.hi;
  uint32_t t = left ? (uint32_t)a->sh[0] : 0U;
  uint32_t u = left ? 0U : (uint32_t)a->sh[1];
  /* Half a step and the output zero point, at the sum's scale. */
  int64_t c = (int64_t)(((uint64_t)((1U << u) >> 1) << 32) |
                        (u == 0U ? 0x80000000U : 0U)) +
              (int64_t)((uint64_t)(int64_t)a->o.zp << (32U + u));
  /* The inputs' zero points come off once, ahead of the loop. */
  int64_t zs = fx_smlal(fx_smull(m1, a->z1), m2, a->z2);
  int64_t k = c - (int64_t)((uint64_t)zs << t);
  for (uint32_t i = 0U; i < n; i++) {
    int32_t d1 = (int32_t)((uint32_t)x1[i] << t);
    int32_t d2 = (int32_t)((uint32_t)x2[i] << t);
    int64_t p = fx_smlal(fx_smlal(k, d1, m1), d2, m2);
#if FNN_RULE == FNN_ROUND_AWAY
    p -= (int64_t)(p < c);
#endif
    int32_t v = (int32_t)(p >> 32) >> u;
    y[i] = (int8_t)(v < lo ? lo : (v > hi ? hi : v));
  }
}

void fnn_add_s8(const FNN_Add *a, const int8_t *x1, const int8_t *x2,
                int8_t *y, uint32_t n) {
  if (a->sh[0] > 0) {
    add_run(a, x1, x2, y, n, 1);
  } else {
    add_run(a, x1, x2, y, n, 0);
  }
}
#endif
