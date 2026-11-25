/**
 * @file fnn_softmax.c
 * @brief Softmax and argmax
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_softmax.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include <math.h>

#if FNN_RULE == FNN_ROUND_GEMMLOWP
/* TFLite's softmax, in gemmlowp's fixed point. */

/* Neither argument is ever INT32_MIN, nor x below 0 in rdbpot. */
static int32_t srdhm(int32_t a, int32_t b) {
  int64_t ab = (int64_t)a * b;
  return (int32_t)((ab + (ab >= 0 ? (1LL << 30) : 1 - (1LL << 30))) / (1LL << 31));
}

static int32_t rdbpot(int32_t x, uint32_t e) {
  int32_t mask = (int32_t)((1U << e) - 1U);
  return (x >> e) + ((x & mask) > (mask >> 1) ? 1 : 0);
}

static int32_t sat_shl(int32_t x, uint32_t e) {
  int32_t lim = (int32_t)((1U << (31U - e)) - 1U);
  return x > lim ? INT32_MAX : (x < -lim ? INT32_MIN : (int32_t)((uint32_t)x << e));
}

/* e^a for a in [-1/4, 0), Q0.31 */
static int32_t exp_quarter(int32_t a) {
  const int32_t c = 1895147668, third = 715827883;
  int32_t x = a + (1 << 28);
  int32_t x2 = srdhm(x, x), x3 = srdhm(x2, x), x4 = srdhm(x2, x2);
  int32_t t = rdbpot(srdhm(rdbpot(x4, 2U) + x3, third) + x2, 1U);
  return c + srdhm(c, x + t);
}

/* e^a for a <= 0 in Q5.26, Q0.31 */
static int32_t exp_neg(int32_t a) {
  static const int32_t f[7] = {1672461947, 1302514674, 790015084, 290630308,
                               39332535,   720401,     242};
  if (a == 0) {
    return INT32_MAX;
  }
  int32_t m = (a & ((1 << 24) - 1)) - (1 << 24);
  int32_t r = exp_quarter(m * 32), rem = m - a;
  for (uint32_t k = 0U; k < 7U; k++) {
    r = rem & (1 << (24U + k)) ? srdhm(r, f[k]) : r;
  }
  return r;
}

/* 1 / (1 + a) for a in [0, 1), Q0.31 */
static int32_t recip(int32_t a) {
  int64_t s = (int64_t)a + INT32_MAX;
  int32_t half = (int32_t)((s + (s >= 0 ? 1 : -1)) / 2);
  int32_t x = 1515870810 + srdhm(half, -1010580540);
  for (int i = 0; i < 3; i++) {
    x += sat_shl(srdhm(x, (1 << 29) - srdhm(half, x)), 2U);
  }
  return sat_shl(x, 1U);
}

FNN_Status fnn_softmax_s8_build(double beta, double s_x,
                                uint32_t exp_tab[256]) {
  double k = beta * s_x * 67108864.0;
  if (!(k > 1.0) || !isfinite(k)) {
    return FNN_ERR_QUANT;
  }
  int shift;
  double q = frexp(k, &shift);
  int64_t mult = (int64_t)round(q * 2147483648.0);
  if (mult == 1LL << 31) {
    mult /= 2;
    shift++;
  }
  double radius = floor(31.0 * 67108864.0 / ldexp(1.0, shift));
  for (uint32_t d = 0U; d < 256U; d++) {
    exp_tab[d] = 0U;
    if ((double)d <= radius) {
      int32_t a = (int32_t)-((int64_t)d << shift);
      exp_tab[d] = (uint32_t)exp_neg(srdhm(a, (int32_t)mult));
    }
  }
  return FNN_OK;
}

void fnn_softmax_s8(const int8_t *x, int8_t *y, uint32_t n,
                    const uint32_t exp_tab[256]) {
  FX_ASSERT(n >= 1U && n <= 4095U);
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  uint8_t *d = (uint8_t *)y;
  int32_t s = 0;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t k = (uint32_t)(mx - x[i]);
    d[i] = (uint8_t)k;
    s += rdbpot((int32_t)exp_tab[k], 12U);
  }
  uint32_t h = fx_clz((uint32_t)s), sh = 35U - h;
  int32_t r = recip((int32_t)(((uint32_t)s << h) - (1U << 31)));
  for (uint32_t i = 0U; i < n; i++) {
    int32_t p = srdhm(r, (int32_t)exp_tab[d[i]]);
    int32_t q = sh < 32U ? rdbpot(p, sh) : 0;
    y[i] = (int8_t)(q > 255 ? 127 : q - 128);
  }
}

#else

FNN_Status fnn_softmax_s8_build(double beta, double s_x,
                                uint32_t exp_tab[256]) {
  double k = beta * s_x;
  if (!(k > 0.0) || !isfinite(k)) {
    return FNN_ERR_QUANT;
  }
  for (uint32_t d = 0U; d < 256U; d++) {
    exp_tab[d] = (uint32_t)(ldexp(exp(-k * (double)d), 31) + 0.5);
  }
  return FNN_OK;
}

static FX_COLD uint32_t exact(uint32_t q, uint32_t e, uint64_t s) {
  uint64_t r = ((uint64_t)e << 8) + (s >> 1) - (uint64_t)q * s;
  return q + (r >= s ? 1U : 0U);
}

void fnn_softmax_s8(const int8_t *x, int8_t *y, uint32_t n,
                    const uint32_t exp_tab[256]) {
  FX_ASSERT(n >= 1U);
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  /* Each score's distance from the maximum waits in y for the last pass. */
  uint8_t *d = (uint8_t *)y;
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t k = (uint32_t)(mx - x[i]);
    d[i] = (uint8_t)k;
    s += exp_tab[k];
  }

  /* s is at least 2^31, and shifted right by t it has 23 bits. */
  uint32_t hi = (uint32_t)(s >> 32);
  uint32_t t = hi != 0U ? 41U - fx_clz(hi) : 9U;
  uint32_t b = (uint32_t)(s >> t) + 1U, ht = (uint32_t)((s >> 1) >> t);
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t e = exp_tab[d[i]];
    uint32_t a = (e >> (t - 8U)) + ht;
    /* q can be one low; only a remainder close to b needs the exact check. */
    uint32_t q = a / b;
    if (FX_UNLIKELY(b - (a - q * b) <= 520U)) {
      q = exact(q, e, s);
    }
    y[i] = q > 255U ? 127 : (int8_t)((int32_t)q - 128);
  }
}

#endif

uint32_t fnn_argmax_s8(const int8_t *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  uint32_t k = 0U;
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    if (x[i] > mx) {
      mx = x[i];
      k = i;
    }
  }
  return k;
}
