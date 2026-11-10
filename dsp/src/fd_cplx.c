/**
 * @file fd_cplx.c
 * @brief Complex vectors
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_vec.h"
#include "fd_wide.h"
#include "fm_sqrt.h"
#include "fm_sum.h"

/* re never overflows int32; im reaches 2^31 (wrapping) only for -1 everywhere
 */
FX_INLINE fd_cq15 cmul15(fd_cq15 a, fd_cq15 b) {
  int32_t re = a.re * b.re - a.im * b.im;
  uint32_t im = (uint32_t)(a.re * b.im) + (uint32_t)(a.im * b.re);
  fd_cq15 r = {fm_sat_q15((re + 0x4000) >> 15),
               im == 0x80000000U ? (fm_q15)32767
                                 : fm_sat_q15(((int32_t)im + 0x4000) >> 15)};
  return r;
}

/* round(u / 2^31) of a sum that fits int64, saturated */
FX_INLINE fm_q31 sat31(uint64_t u) {
  int32_t hi = (int32_t)(u >> 32);
  if (hi >= 0x40000000) {
    return INT32_MAX;
  }
  if (hi < -0x40000000) {
    return INT32_MIN;
  }
  return (fm_q31)(((uint32_t)hi << 1) | ((uint32_t)u >> 31));
}

/* SMLALs from the rounding preload; -bi = ~bi + 1 keeps re free of negation
 * overflow; im wraps to hi = INT32_MIN only for -1 everywhere */
FX_INLINE fd_cq31 cmul31(fd_cq31 a, fd_cq31 b) {
  uint64_t re = 0x40000000U + (uint64_t)(int64_t)a.im;
  re += (uint64_t)fx_smull(a.re, b.re);
  re += (uint64_t)fx_smull(a.im, ~b.im);
  uint64_t im = 0x40000000U + (uint64_t)fx_smull(a.re, b.im);
  im += (uint64_t)fx_smull(a.im, b.re);
  fd_cq31 r = {sat31(re),
               (int32_t)(im >> 32) == INT32_MIN ? INT32_MAX : sat31(im)};
  return r;
}

void fd_cmul_q15(const fd_cq15 *a, const fd_cq15 *b, fd_cq15 *y, uint32_t n) {
  FX_UNROLL(2)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = cmul15(a[i], b[i]);
  }
}

void fd_cmul_q31(const fd_cq31 *a, const fd_cq31 *b, fd_cq31 *y, uint32_t n) {
  FX_UNROLL(2)
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = cmul31(a[i], b[i]);
  }
}

void fd_cmul_real_q15(const fd_cq15 *a, const fm_q15 *b, fd_cq15 *y,
                      uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq15 r = {fm_mul_q15(a[i].re, b[i]), fm_mul_q15(a[i].im, b[i])};
    y[i] = r;
  }
}

void fd_cmul_real_q31(const fd_cq31 *a, const fm_q31 *b, fd_cq31 *y,
                      uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq31 r = {fm_mul_q31(a[i].re, b[i]), fm_mul_q31(a[i].im, b[i])};
    y[i] = r;
  }
}

void fd_conj_q15(const fd_cq15 *x, fd_cq15 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq15 r = {x[i].re, fm_sat_q15(-(int32_t)x[i].im)};
    y[i] = r;
  }
}

void fd_conj_q31(const fd_cq31 *x, fd_cq31 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq31 r = {x[i].re, fm_sub_q31(0, x[i].im)};
    y[i] = r;
  }
}

/* S is an integer, so sqrt(S) >= r + 1/2 exactly when S > r^2 + r */
void fd_cmag_q15(const fd_cq15 *x, uint16_t *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t s = (uint32_t)(x[i].re * x[i].re) + (uint32_t)(x[i].im * x[i].im);
    uint32_t r = fm_isqrt32(s);
    y[i] = (uint16_t)(s > r * r + r ? r + 1U : r);
  }
}

void fd_cmag_q31(const fd_cq31 *x, uint32_t *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t s = (uint64_t)fx_smull(x[i].re, x[i].re) +
                 (uint64_t)fx_smull(x[i].im, x[i].im);
    uint32_t r = fm_isqrt64(s);
    y[i] = s > fx_umull(r, r) + r ? r + 1U : r;
  }
}

void fd_cmag2_q15(const fd_cq15 *x, uint32_t *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = (uint32_t)(x[i].re * x[i].re) + (uint32_t)(x[i].im * x[i].im);
  }
}

void fd_cmag2_q31(const fd_cq31 *x, uint32_t *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t s = (uint64_t)fx_smull(x[i].re, x[i].re) +
                 (uint64_t)fx_smull(x[i].im, x[i].im) + (1ULL << 30);
    uint64_t q = s >> 31;
    y[i] = q > UINT32_MAX ? UINT32_MAX : (uint32_t)q;
  }
}

void fd_cdot_q15(const fd_cq15 *a, const fd_cq15 *b, uint32_t n, int64_t *re,
                 int64_t *im) {
  int64_t sr = 0;
  int64_t si = 0;
  for (uint32_t i = 0U; i < n; i++) {
    sr += a[i].re * b[i].re - a[i].im * b[i].im;
    si += a[i].re * b[i].im;
    si += a[i].im * b[i].re;
  }
  *re = sr;
  *im = si;
}

/* each int64 part holds 2^15 terms (two products of up to 2^47 each) */
void fd_cdot_q31(const fd_cq31 *a, const fd_cq31 *b, uint32_t n, int64_t *re,
                 int64_t *im) {
  Sum96 sr = {0U, 0};
  Sum96 si = {0U, 0};
  while (n > 0U) {
    uint32_t m = n < 32768U ? n : 32768U;
    int64_t rh = 0, rl = 0, ih = 0, il = 0;
    for (uint32_t i = 0U; i < m; i++) {
      int32_t brh = b[i].re >> 16, bih = b[i].im >> 16;
      int32_t brl = (int32_t)((uint32_t)b[i].re & 0xFFFFU);
      int32_t bil = (int32_t)((uint32_t)b[i].im & 0xFFFFU);
      rh = fx_smlal(rh, a[i].re, brh);
      rh = fx_smlal(rh, a[i].im, -bih);
      rl = fx_smlal(rl, a[i].re, brl);
      rl = fx_smlal(rl, a[i].im, -bil);
      ih = fx_smlal(ih, a[i].re, bih);
      ih = fx_smlal(ih, a[i].im, brh);
      il = fx_smlal(il, a[i].re, bil);
      il = fx_smlal(il, a[i].im, brl);
    }
    sum96_add(&sr, rl);
    sum96_add_shl16(&sr, rh);
    sum96_add(&si, il);
    sum96_add_shl16(&si, ih);
    a += m;
    b += m;
    n -= m;
  }
  *re = sum96_q48(sr);
  *im = sum96_q48(si);
}

void fd_interleave_q15(const fm_q15 *re, const fm_q15 *im, fd_cq15 *y,
                       uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq15 r = {re[i], im[i]};
    y[i] = r;
  }
}

void fd_interleave_q31(const fm_q31 *re, const fm_q31 *im, fd_cq31 *y,
                       uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    fd_cq31 r = {re[i], im[i]};
    y[i] = r;
  }
}

void fd_deinterleave_q15(const fd_cq15 *x, fm_q15 *re, fm_q15 *im, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    re[i] = x[i].re;
    im[i] = x[i].im;
  }
}

void fd_deinterleave_q31(const fd_cq31 *x, fm_q31 *re, fm_q31 *im, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    re[i] = x[i].re;
    im[i] = x[i].im;
  }
}
