/**
 * @file fd_biquad.c
 * @brief Biquad cascades (DF1) and their design
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_biquad.h"
#include "fd_wide.h"
#include "fm_float.h"

/* ---- q15: Q2.14 coefficients, int32 sums (sum |c| < 4) ---- */

/* |acc| < 2^31 - 2^13 */
FX_INLINE fm_q15 out15(int32_t acc) { return fm_sat_q15((acc + 0x2000) >> 14); }

FX_INLINE fm_q15 out15w(int64_t acc) {
  int64_t r = (acc + 0x2000) >> 14;
  return r > 32767 ? 32767 : (r < -32768 ? -32768 : (fm_q15)r);
}

/* one section over the block; xh: x1, x2, yh: y1, y2. Unrolled by two, so the
 * history swaps roles instead of moving */
FX_INLINE void sec15(const fm_q15 *c, uint32_t s, int32_t *xh, fm_q15 *yh,
                     const fm_q15 *x, fm_q15 *y, uint32_t n) {
  const int32_t b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];
  int32_t x1 = xh[0], x2 = xh[1], y1 = yh[0], y2 = yh[1];
  for (; n >= 2U; n -= 2U) {
    int32_t xa = x[0], xb = x[1];
    int32_t acc = (b0 * xa + b1 * x1 + b2 * x2) * (1 << s);
    x2 = xa;
    y2 = out15(acc + a1 * y1 + a2 * y2);
    acc = (b0 * xb + b1 * x2 + b2 * x1) * (1 << s);
    x1 = xb;
    y1 = out15(acc + a1 * y2 + a2 * y1);
    y[0] = (fm_q15)y2;
    y[1] = (fm_q15)y1;
    x += 2;
    y += 2;
  }
  if (n != 0U) {
    int32_t xa = x[0];
    int32_t acc = (b0 * xa + b1 * x1 + b2 * x2) * (1 << s);
    int32_t ya = out15(acc + a1 * y1 + a2 * y2);
    *y = (fm_q15)ya;
    x2 = x1;
    x1 = xa;
    y2 = y1;
    y1 = ya;
  }
  xh[0] = x1;
  xh[1] = x2;
  yh[0] = (fm_q15)y1;
  yh[1] = (fm_q15)y2;
}

/* any coefficients: 64-bit sums */
static void sec15w(const fm_q15 *c, uint32_t s, int32_t *xh, fm_q15 *yh,
                   const fm_q15 *x, fm_q15 *y, uint32_t n) {
  int32_t x1 = xh[0], x2 = xh[1], y1 = yh[0], y2 = yh[1];
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    int64_t acc =
        ((int64_t)(c[0] * x0) + c[1] * x1 + (int64_t)(c[2] * x2)) * (1 << s);
    acc += (int64_t)(c[3] * y1) + c[4] * y2;
    int32_t y0 = out15w(acc);
    y[i] = (fm_q15)y0;
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }
  xh[0] = x1;
  xh[1] = x2;
  yh[0] = (fm_q15)y1;
  yh[1] = (fm_q15)y2;
}

/* ---- q31: Q2.30 coefficients, int64 sums (sum |c| < 4) ---- */

/* round(acc / 2^30) saturated, or with ef: floor, and the residue in *e */
FX_INLINE fm_q31 out31(int64_t acc, bool ef, fm_q31 *e) {
  uint64_t u = (uint64_t)acc + (ef ? (uint64_t)(uint32_t)*e : 0x20000000U);
  int32_t hi = (int32_t)(u >> 32);
  if (hi >= 0x20000000) {
    *e = 0;
    return INT32_MAX;
  }
  if (hi < -0x20000000) {
    *e = 0;
    return INT32_MIN;
  }
  if (ef) {
    *e = (fm_q31)((uint32_t)u & 0x3FFFFFFFU);
  }
  return (fm_q31)(((uint32_t)hi << 2) | ((uint32_t)u >> 30));
}

/* not unrolled: two live inputs push the coefficients out of registers */
FX_INLINE void sec31(const fm_q31 *c, uint32_t s, int32_t *xh, fm_q31 *yh,
                     fm_q31 *e, const fm_q31 *x, fm_q31 *y, uint32_t n, bool ef,
                     bool shifted) {
  const int32_t b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];
  int32_t x1 = xh[0], x2 = xh[1], y1 = yh[0], y2 = yh[1];
  fm_q31 r = *e;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    int64_t acc = fx_smlal(fx_smlal(fx_smull(b0, x0), b1, x1), b2, x2);
    if (shifted) {
      acc *= (int64_t)1 << s;
    }
    acc = fx_smlal(fx_smlal(acc, a1, y1), a2, y2);
    int32_t y0 = out31(acc, ef, &r);
    y[i] = y0;
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }
  xh[0] = x1;
  xh[1] = x2;
  yh[0] = y1;
  yh[1] = y2;
  *e = r;
}

/* s << k, k < 32 */
FX_INLINE Sum96 sum96_shl(Sum96 s, uint32_t k) {
  if (k != 0U) {
    s.hi = (int32_t)(((uint32_t)s.hi << k) | (uint32_t)(s.lo >> (64U - k)));
    s.lo <<= k;
  }
  return s;
}

/* any coefficients: 96-bit sums */
static void sec31w(const fm_q31 *c, uint32_t s, int32_t *xh, fm_q31 *yh,
                   fm_q31 *e, const fm_q31 *x, fm_q31 *y, uint32_t n, bool ef) {
  int32_t x1 = xh[0], x2 = xh[1], y1 = yh[0], y2 = yh[1];
  fm_q31 r = *e;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t x0 = x[i];
    Sum96 a = {0U, 0};
    sum96_add(&a, fx_smull(c[0], x0));
    sum96_add(&a, fx_smull(c[1], x1));
    sum96_add(&a, fx_smull(c[2], x2));
    a = sum96_shl(a, s);
    sum96_add(&a, fx_smull(c[3], y1));
    sum96_add(&a, fx_smull(c[4], y2));
    sum96_add(&a, ef ? (int64_t)(uint32_t)r : 0x20000000);
    uint32_t top = (uint32_t)(a.lo >> 61); /* fits: bits 61..95 all equal */
    int32_t y0;
    if ((a.hi == 0 && top == 0U) || (a.hi == -1 && top == 7U)) {
      y0 = (int32_t)(uint32_t)(a.lo >> 30);
      r = ef ? (fm_q31)((uint32_t)a.lo & 0x3FFFFFFFU) : 0;
    } else {
      y0 = a.hi < 0 ? INT32_MIN : INT32_MAX;
      r = 0;
    }
    y[i] = y0;
    x2 = x1;
    x1 = x0;
    y2 = y1;
    y1 = y0;
  }
  xh[0] = x1;
  xh[1] = x2;
  yh[0] = y1;
  yh[1] = y2;
  *e = r;
}

/* ---- cascades ---- */

static uint32_t wide_q31(const fm_q31 *c, uint32_t nsec, uint32_t s) {
  for (uint32_t k = 0U; k < nsec; k++) {
    uint64_t g = 0U;
    for (uint32_t i = 0U; i < 5U; i++) {
      int32_t v = c[5U * k + i];
      uint64_t m = v < 0 ? 0U - (uint64_t)(int64_t)v : (uint64_t)v;
      g += i < 3U ? m << s : m;
    }
    if (g >= (4ULL << 30)) {
      return 1U;
    }
  }
  return 0U;
}

static uint32_t wide_q15(const fm_q15 *c, uint32_t nsec, uint32_t s) {
  for (uint32_t k = 0U; k < nsec; k++) {
    uint64_t g = 0U;
    for (uint32_t i = 0U; i < 5U; i++) {
      int32_t v = c[5U * k + i];
      uint64_t m = (uint64_t)(v < 0 ? -v : v);
      g += i < 3U ? m << s : m;
    }
    if (g >= (4U << 14)) {
      return 1U;
    }
  }
  return 0U;
}

void fd_biquad_init_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                        uint32_t bshift, fm_q31 *state) {
  FX_ASSERT(nsec >= 1U && bshift <= 31U);
  f->c = c;
  f->state = state;
  f->nsec = nsec;
  f->bshift = bshift;
  f->wide = wide_q31(c, nsec, bshift);
  f->ef = 0U;
  fd_biquad_reset_q31(f);
}

void fd_biquad_init_ef_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                           uint32_t bshift, fm_q31 *state) {
  fd_biquad_init_q31(f, c, nsec, bshift, state);
  f->ef = 1U;
}

void fd_biquad_init_q15(FD_BiquadQ15 *f, const fm_q15 *c, uint32_t nsec,
                        uint32_t bshift, fm_q15 *state) {
  FX_ASSERT(nsec >= 1U && bshift <= 15U);
  f->c = c;
  f->state = state;
  f->nsec = nsec;
  f->bshift = bshift;
  f->wide = wide_q15(c, nsec, bshift);
  fd_biquad_reset_q15(f);
}

void fd_biquad_reset_q31(FD_BiquadQ31 *f) {
  for (uint32_t i = 0U; i < FD_BIQUAD_STATE_LEN(f->nsec); i++) {
    f->state[i] = 0;
  }
}

void fd_biquad_reset_q15(FD_BiquadQ15 *f) {
  for (uint32_t i = 0U; i < FD_BIQUAD_STATE_LEN(f->nsec); i++) {
    f->state[i] = 0;
  }
}

/* state: section k's input history is section k - 1's output history, which
 * the block overwrites; each section takes the copy saved before it ran */
void fd_biquad_q31(FD_BiquadQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  fm_q31 *res = f->state + 2U * (f->nsec + 1U);
  int32_t xh[2] = {f->state[0], f->state[1]};
  for (uint32_t k = 0U; k < f->nsec; k++) {
    const fm_q31 *c = f->c + 5U * k;
    fm_q31 *yh = f->state + 2U * k + 2U;
    int32_t next[2] = {yh[0], yh[1]};
    const uint32_t sh = f->bshift;
    if (f->wide != 0U) {
      sec31w(c, sh, xh, yh, res + k, x, y, n, f->ef != 0U);
    } else if (f->ef != 0U) {
      sec31(c, sh, xh, yh, res + k, x, y, n, true, sh != 0U);
    } else if (sh != 0U) {
      sec31(c, sh, xh, yh, res + k, x, y, n, false, true);
    } else {
      sec31(c, 0U, xh, yh, res + k, x, y, n, false, false);
    }
    if (k == 0U) {
      f->state[0] = xh[0];
      f->state[1] = xh[1];
    }
    xh[0] = next[0];
    xh[1] = next[1];
    x = y;
  }
}

void fd_biquad_q15(FD_BiquadQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n) {
  int32_t xh[2] = {f->state[0], f->state[1]};
  for (uint32_t k = 0U; k < f->nsec; k++) {
    const fm_q15 *c = f->c + 5U * k;
    fm_q15 *yh = f->state + 2U * k + 2U;
    int32_t next[2] = {yh[0], yh[1]};
    if (f->wide != 0U) {
      sec15w(c, f->bshift, xh, yh, x, y, n);
    } else {
      sec15(c, f->bshift, xh, yh, x, y, n);
    }
    if (k == 0U) {
      f->state[0] = (fm_q15)xh[0];
      f->state[1] = (fm_q15)xh[1];
    }
    xh[0] = next[0];
    xh[1] = next[1];
    x = y;
  }
}

/* ---- design ---- */

#define PI_F 3.14159265358979f

FD_Status fd_biquad_design(FD_BiquadDesign *d, FD_BiquadType type, float f0,
                           float q, float gain_db) {
  if (!(f0 > 0.0f && f0 < 0.5f) || !(q > 0.0f) ||
      (uint32_t)type > (uint32_t)FD_BQ_ALLPASS) {
    return FD_ERR_RANGE;
  }
  const float w = 2.0f * PI_F * f0;
  const float sn = fm_sinf(w), cs = fm_cosf(w);
  const float sh = fm_sinf(0.5f * w);
  const float omc = 2.0f * sh * sh; /* 1 - cos w, without cancellation */
  const float al = sn / (2.0f * q);
  const float A = fm_exp2f(gain_db * (3.32192809f / 40.0f)); /* 10^(g/40) */
  float b0, b1, b2, p0, q1, q2;
  float ap = al; /* alpha in the denominator */
  if (type == FD_BQ_PEAK) {
    ap = al / A;
  }
  p0 = 1.0f + ap;
  q1 = 2.0f * omc + 2.0f * ap;
  q2 = 2.0f * ap;
  switch (type) {
  case FD_BQ_LOWPASS:
    b0 = b2 = 0.5f * omc;
    b1 = omc;
    break;
  case FD_BQ_HIGHPASS:
    b0 = b2 = 0.5f * (1.0f + cs);
    b1 = -(1.0f + cs);
    break;
  case FD_BQ_BANDPASS:
    b0 = al;
    b1 = 0.0f;
    b2 = -al;
    break;
  case FD_BQ_NOTCH:
    b0 = b2 = 1.0f;
    b1 = -2.0f * cs;
    break;
  case FD_BQ_PEAK:
    b0 = 1.0f + al * A;
    b1 = -2.0f * cs;
    b2 = 1.0f - al * A;
    break;
  case FD_BQ_ALLPASS:
    b0 = 1.0f - al;
    b1 = -2.0f * cs;
    b2 = 1.0f + al;
    break;
  default: { /* shelves */
    const float sa = 2.0f * fm_sqrtf(A) * al;
    const float pp = A + 1.0f, mm = A - 1.0f;
    if (type == FD_BQ_LOWSHELF) {
      b0 = A * (pp - mm * cs + sa);
      b1 = 2.0f * A * (mm - pp * cs);
      b2 = A * (pp - mm * cs - sa);
      p0 = pp + mm * cs + sa;
      q1 = 4.0f * omc + 2.0f * sa;
    } else {
      b0 = A * (pp + mm * cs + sa);
      b1 = -2.0f * A * (mm + pp * cs);
      b2 = A * (pp + mm * cs - sa);
      p0 = pp - mm * cs + sa;
      q1 = 4.0f * A * omc + 2.0f * sa;
    }
    q2 = 2.0f * sa;
    break;
  }
  }
  const float r = 1.0f / p0;
  d->b0 = b0 * r;
  d->b1 = b1 * r;
  d->b2 = b2 * r;
  d->d1 = q1 * r;
  d->d2 = q2 * r;
  return FD_OK;
}

FD_Status fd_biquad_butter(FD_BiquadDesign *s, uint32_t order, FD_Band band,
                           float f0) {
  if (order == 0U || (order & 1U) != 0U ||
      (band != FD_LOWPASS && band != FD_HIGHPASS)) {
    return FD_ERR_RANGE;
  }
  const uint32_t m = order / 2U;
  for (uint32_t i = 0U; i < m; i++) { /* lowest Q first */
    uint32_t k = m - 1U - i;
    float th = PI_F * (float)(2U * k + 1U) / (float)(2U * order);
    float q = 0.5f / fm_sinf(th);
    FD_Status st = fd_biquad_design(
        &s[i], band == FD_LOWPASS ? FD_BQ_LOWPASS : FD_BQ_HIGHPASS, f0, q,
        0.0f);
    if (st != FD_OK) {
      return st;
    }
  }
  return FD_OK;
}

/* round(v 2^k) for |v 2^k| < 2^31, k <= 31: the q31 conversion of v 2^(k-31) */
static int64_t fix(float v, uint32_t k) {
  return fm_f32_to_q31(v / (float)(1ULL << (31U - k)));
}

/* f fraction bits: 30 (Q2.30) or 14 (Q2.14) */
static FD_Status quantize(const FD_BiquadDesign *s, uint32_t nsec, uint32_t f,
                          int32_t *c, uint32_t *bshift) {
  const int64_t one = (int64_t)1 << f;
  float bmax = 0.0f;
  for (uint32_t k = 0U; k < nsec; k++) {
    const float b[3] = {s[k].b0, s[k].b1, s[k].b2};
    for (int i = 0; i < 3; i++) {
      float m = b[i] < 0.0f ? -b[i] : b[i];
      bmax = m > bmax ? m : bmax;
    }
  }
  uint32_t sh = 0U; /* the least shift that keeps every rounded b under 2 */
  while (bmax / (float)(1UL << sh) >= 2.0f ||
         fix(bmax / (float)(1UL << sh), f) >= 2 * one) {
    if (++sh > 15U) {
      return FD_ERR_RANGE;
    }
  }
  FD_Status st = FD_OK;
  for (uint32_t k = 0U; k < nsec; k++) {
    int32_t *q = c + 5U * k;
    const float d1 = s[k].d1, d2 = s[k].d2;
    if (!(d1 >= 0.0f && d1 < 4.0f && d2 >= 0.0f && d2 < 2.0f)) {
      return FD_ERR_RANGE;
    }
    const float bsc = 1.0f / (float)(1UL << sh);
    q[0] = (int32_t)fix(s[k].b0 * bsc, f);
    q[1] = (int32_t)fix(s[k].b1 * bsc, f);
    q[2] = (int32_t)fix(s[k].b2 * bsc, f);
    int64_t a1 = d1 < 2.0f ? 2 * one - fix(d1, f) : -fix(d1 - 2.0f, f);
    int64_t a2 = fix(d2, f) - one;
    if (a1 >= 2 * one) { /* a double pole at z = 1 */
      a1 = 2 * one - 1;
      st = FD_ERR_UNSTABLE;
    }
    if (!(a2 < one && a2 > -one && a1 + a2 < one && a2 - a1 < one)) {
      st = FD_ERR_UNSTABLE;
    }
    q[3] = (int32_t)a1;
    q[4] = (int32_t)a2;
  }
  *bshift = sh;
  return st;
}

FD_Status fd_biquad_quantize_q31(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q31 *c, uint32_t *bshift) {
  return quantize(s, nsec, 30U, c, bshift);
}

FD_Status fd_biquad_quantize_q15(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q15 *c, uint32_t *bshift) {
  FD_Status st = FD_OK;
  int32_t w[5];
  uint32_t sh = 0U, s1;
  for (uint32_t k = 0U; k < nsec; k++) { /* the cascade's shift first */
    FD_Status r = quantize(&s[k], 1U, 14U, w, &s1);
    if (r == FD_ERR_RANGE) {
      return r;
    }
    sh = s1 > sh ? s1 : sh;
  }
  for (uint32_t k = 0U; k < nsec; k++) {
    FD_BiquadDesign d = s[k];
    const float bsc = 1.0f / (float)(1UL << sh);
    d.b0 *= bsc;
    d.b1 *= bsc;
    d.b2 *= bsc;
    FD_Status r = quantize(&d, 1U, 14U, w, &s1);
    st = r != FD_OK ? r : st;
    for (uint32_t i = 0U; i < 5U; i++) {
      c[5U * k + i] = (fm_q15)w[i];
    }
  }
  *bshift = sh;
  return st;
}
