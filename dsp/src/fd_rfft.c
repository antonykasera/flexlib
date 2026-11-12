/**
 * @file fd_rfft.c
 * @brief Real FFTs as half-size complex ones plus a split pass, spectrum dB
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fft_core.h"

#define QP FD_FFT_QP
#define TAB FD_FFT_TAB
#define NOINLINE __attribute__((noinline))

/* a = Z[k], b = Z[m - k], W = c - i s: 2X[k] = a + conj b + W (a - conj b) / i,
 * 2X[m - k] its conj mirror; |2X| < (2 + 2 sqrt 2) 2^b, rounded once */

/* ---- q15 ---- */

FX_INLINE int16_t out15(int32_t v, uint32_t k) { return (int16_t)(v >> k); }

/* ps 0: exact in int32 (b <= 13); ps 2: each product enters as p >> 2 */
FX_INLINE void split15(int16_t *x, uint32_t n, int32_t sh, uint32_t ps) {
  const uint32_t m = n / 2U, k = (uint32_t)(15 + sh) - ps;
  const int32_t rnd = (1 << (k - 1U)) - 1;
  const int32_t one = 1 << (15U - ps);
  const uint32_t step = 4U * QP >> lg2(n);
  int32_t ar = x[0], ai = x[1];
  x[0] = out15(2 * (ar + ai) * one + rnd, k);
  x[1] = out15(2 * (ar - ai) * one + rnd, k);
  ar = x[m];
  ai = x[m + 1U];
  x[m] = out15(2 * ar * one + rnd, k);
  x[m + 1U] = out15(-2 * ai * one + rnd, k);
  int16_t *lo = x + 2, *hi = x + 2U * (m - 1U);
  for (uint32_t i = 1U; i < m / 2U; i++) {
    int32_t s = fd_fft_tw15(TAB[i * step]);
    int32_t c = fd_fft_tw15(TAB[QP - i * step]);
    int32_t pr = lo[0], pi = lo[1], qr = hi[0], qi = hi[1];
    int32_t er = (pr + qr) * one + rnd, ei = (pi - qi) * one;
    int32_t u = pi + qi, d = pr - qr;
    int32_t br = ((c * u) >> ps) - ((s * d) >> ps);
    int32_t bi = rnd - (((c * d) >> ps) + ((s * u) >> ps));
    lo[0] = out15(er + br, k);
    lo[1] = out15(ei + bi, k);
    hi[0] = out15(er - br, k);
    hi[1] = out15(bi - ei, k);
    lo += 2;
    hi -= 2;
  }
}

/* p = X[k], r = X[m - k]: 2Z[k] = p + conj r + i conj(W) (p - conj r), and
 * 2Z[m - k]; stored in each other's slot, the inverse's index flip */
FX_INLINE void join15(int16_t *x, uint32_t n, int32_t sh, uint32_t ps) {
  const uint32_t m = n / 2U, k = (uint32_t)(15 + sh) - ps;
  const int32_t rnd = (1 << (k - 1U)) - 1;
  const int32_t one = 1 << (15U - ps);
  const uint32_t step = 4U * QP >> lg2(n);
  int32_t ar = x[0], ai = x[1];
  x[0] = out15((ar + ai) * one + rnd, k);
  x[1] = out15((ar - ai) * one + rnd, k);
  ar = x[m];
  ai = x[m + 1U];
  x[m] = out15(2 * ar * one + rnd, k);
  x[m + 1U] = out15(-2 * ai * one + rnd, k);
  int16_t *lo = x + 2, *hi = x + 2U * (m - 1U);
  for (uint32_t i = 1U; i < m / 2U; i++) {
    int32_t s = fd_fft_tw15(TAB[i * step]);
    int32_t c = fd_fft_tw15(TAB[QP - i * step]);
    int32_t pr = lo[0], pi = lo[1], qr = hi[0], qi = hi[1];
    int32_t er = (pr + qr) * one + rnd, ei = (pi - qi) * one;
    int32_t dr = pr - qr, di = pi + qi;
    int32_t or_ = rnd + ((c * dr) >> ps) - ((s * di) >> ps);
    int32_t oi = ((s * dr) >> ps) + ((c * di) >> ps);
    hi[0] = out15(er - oi, k);
    hi[1] = out15(ei + or_, k);
    lo[0] = out15(er + oi, k);
    lo[1] = out15(or_ - ei, k);
    lo += 2;
    hi -= 2;
  }
}

NOINLINE static void split15_n(int16_t *x, uint32_t n, int32_t sh) {
  split15(x, n, sh, 0U);
}

NOINLINE static void split15_w(int16_t *x, uint32_t n, int32_t sh) {
  split15(x, n, sh, 2U);
}

NOINLINE static void join15_n(int16_t *x, uint32_t n, int32_t sh) {
  join15(x, n, sh, 0U);
}

NOINLINE static void join15_w(int16_t *x, uint32_t n, int32_t sh) {
  join15(x, n, sh, 2U);
}

int32_t fd_rfft_q15(fm_q15 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 4U) && ((uintptr_t)x & 3U) == 0U);
  int32_t e = fd_fft15_cfft(x, n / 2U);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  uint32_t b = fd_fft15_scan(x, n / 2U);
  int32_t sh = (int32_t)b - 12;
  if (b <= 13U) {
    split15_n(x, n, sh);
  } else {
    split15_w(x, n, sh);
  }
  return e + sh - 1;
}

int32_t fd_irfft_q15(fm_q15 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 4U) && ((uintptr_t)x & 3U) == 0U);
  const uint32_t m = n / 2U;
  uint32_t b = fd_fft15_scan(x, m);
  int32_t sh = (int32_t)b - 12;
  if (b <= 13U) {
    join15_n(x, n, sh);
  } else {
    join15_w(x, n, sh);
  }
  int32_t e = fd_fft15_cfft(x, m);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  return e - (31 - (int32_t)fx_clz(m)) + sh - 1;
}

/* ---- q31: as r4_31, hi (sh >= 1) has twiddles at 2^(32 - sh) and keeps the
 * high word; else twiddles at 2^31, the sum >> (31 + sh) ---- */

FX_INLINE int64_t lsl64(int64_t x, uint32_t s) {
  return (int64_t)((uint64_t)x << s);
}

FX_INLINE uint32_t tws31(uint32_t t, uint32_t ts) {
  return ts == 0U ? t : (t + (1U << (ts - 1U))) >> ts;
}

#define OUT31(v)                                                               \
  (hi ? (int32_t)((v) >> 32)                                                   \
      : (int32_t)(((uint32_t)(v) >> k) |                                       \
                  ((uint32_t)((v) >> 32) << (32U - k))))

#define PASS31_SETUP                                                           \
  const uint32_t m = n / 2U;                                                   \
  const uint32_t ts = hi ? (uint32_t)sh - 1U : 0U;                             \
  const uint32_t xs = 31U - ts;                                                \
  const uint32_t k = hi ? 32U : (uint32_t)(31 + sh);                           \
  const int64_t rnd = (int64_t)((1ULL << (k - 1U)) - 1U);                      \
  const uint32_t step = 4U * QP >> lg2(n)

FX_INLINE void split31(int32_t *x, uint32_t n, int32_t sh, bool hi) {
  PASS31_SETUP;
  int64_t ar = x[0], ai = x[1];
  x[0] = OUT31(lsl64(2 * (ar + ai), xs) + rnd);
  x[1] = OUT31(lsl64(2 * (ar - ai), xs) + rnd);
  ar = x[m];
  ai = x[m + 1U];
  x[m] = OUT31(lsl64(2 * ar, xs) + rnd);
  x[m + 1U] = OUT31(lsl64(-2 * ai, xs) + rnd);
  int32_t *lo = x + 2, *hp = x + 2U * (m - 1U);
  for (uint32_t i = 1U; i < m / 2U; i++) {
    int32_t s = (int32_t)tws31(TAB[i * step], ts);
    int32_t c = (int32_t)tws31(TAB[QP - i * step], ts);
    int32_t pr = lo[0], pi = lo[1], qr = hp[0], qi = hp[1];
    int64_t er = lsl64((int64_t)pr + qr, xs) + rnd;
    int64_t ei = lsl64((int64_t)pi - qi, xs);
    /* 2B = (c u - s d, -(c d + s u)), u = pi + qi, d = pr - qr */
    int64_t br =
        fx_smlal(fx_smlal(fx_smlal(fx_smull(c, pi), c, qi), -s, pr), s, qr);
    int64_t bi = fx_smlal(
        fx_smlal(fx_smlal(fx_smlal(rnd, -c, pr), c, qr), -s, pi), -s, qi);
    lo[0] = OUT31(er + br);
    lo[1] = OUT31(ei + bi);
    hp[0] = OUT31(er - br);
    hp[1] = OUT31(bi - ei);
    lo += 2;
    hp -= 2;
  }
}

FX_INLINE void join31(int32_t *x, uint32_t n, int32_t sh, bool hi) {
  PASS31_SETUP;
  int64_t ar = x[0], ai = x[1];
  x[0] = OUT31(lsl64(ar + ai, xs) + rnd);
  x[1] = OUT31(lsl64(ar - ai, xs) + rnd);
  ar = x[m];
  ai = x[m + 1U];
  x[m] = OUT31(lsl64(2 * ar, xs) + rnd);
  x[m + 1U] = OUT31(lsl64(-2 * ai, xs) + rnd);
  int32_t *lo = x + 2, *hp = x + 2U * (m - 1U);
  for (uint32_t i = 1U; i < m / 2U; i++) {
    int32_t s = (int32_t)tws31(TAB[i * step], ts);
    int32_t c = (int32_t)tws31(TAB[QP - i * step], ts);
    int32_t pr = lo[0], pi = lo[1], qr = hp[0], qi = hp[1];
    int64_t er = lsl64((int64_t)pr + qr, xs) + rnd;
    int64_t ei = lsl64((int64_t)pi - qi, xs);
    /* 2O = (c dr - s di, s dr + c di), dr = pr - qr, di = pi + qi */
    int64_t or_ = fx_smlal(
        fx_smlal(fx_smlal(fx_smlal(rnd, c, pr), -c, qr), -s, pi), -s, qi);
    int64_t oi =
        fx_smlal(fx_smlal(fx_smlal(fx_smull(s, pr), -s, qr), c, pi), c, qi);
    hp[0] = OUT31(er - oi);
    hp[1] = OUT31(ei + or_);
    lo[0] = OUT31(er + oi);
    lo[1] = OUT31(or_ - ei);
    lo += 2;
    hp -= 2;
  }
}

NOINLINE static void split31_h(int32_t *x, uint32_t n, int32_t sh) {
  split31(x, n, sh, true);
}

NOINLINE static void split31_x(int32_t *x, uint32_t n, int32_t sh) {
  split31(x, n, sh, false);
}

NOINLINE static void join31_h(int32_t *x, uint32_t n, int32_t sh) {
  join31(x, n, sh, true);
}

NOINLINE static void join31_x(int32_t *x, uint32_t n, int32_t sh) {
  join31(x, n, sh, false);
}

int32_t fd_rfft_q31(fm_q31 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 4U));
  int32_t e = fd_fft31_cfft(x, n / 2U);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  int32_t sh = (int32_t)fd_fft31_scan(x, n / 2U) - 28;
  if (sh > 0) {
    split31_h(x, n, sh);
  } else {
    split31_x(x, n, sh);
  }
  return e + sh - 1;
}

int32_t fd_irfft_q31(fm_q31 *x, uint32_t n) {
  FX_ASSERT(fd_fft_size_ok(n, 4U));
  const uint32_t m = n / 2U;
  int32_t sh = (int32_t)fd_fft31_scan(x, m) - 28;
  if (sh > 0) {
    join31_h(x, n, sh);
  } else {
    join31_x(x, n, sh);
  }
  int32_t e = fd_fft31_cfft(x, m);
  if (e == FD_FFT_ZERO) {
    return 0;
  }
  return e - (31 - (int32_t)fx_clz(m)) + sh - 1;
}

/* ---- dB ---- */

#define DB10_Q30 3232284966U /* 10 log10 2 */

/* 10 log10 2 (l / 2^32 + c) in Q16.16, l a log2 core value, saturated */
FX_INLINE fm_q16 db_of(int64_t l, int64_t c) {
  int64_t h = (l >> 32) + c;
  if (h > 20000) {
    return INT32_MAX;
  }
  if (h < -20000) {
    return INT32_MIN + 1;
  }
  int64_t v = h * DB10_Q30 + fx_umull_hi((uint32_t)l, DB10_Q30); /* Q30 */
  v = (v + (1 << 13)) >> 14;
  return v > INT32_MAX ? INT32_MAX
                       : (v <= INT32_MIN ? INT32_MIN + 1 : (fm_q16)v);
}

/* the core is log2(p / 2^16): 0 dB at p = 2^30 (q15), 2^62 (q31) */
void fd_fft_db_q15(const fd_cq15 *x, int32_t e, fm_q16 *y, uint32_t n) {
  const int64_t c = 2 * (int64_t)e - 14;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t p = (uint32_t)(x[i].re * x[i].re) + (uint32_t)(x[i].im * x[i].im);
    y[i] = p == 0U ? INT32_MIN : db_of(fm_log2_core(p), c);
  }
}

/* p over 32 bits keeps its top 32: under 2^-31 relative */
void fd_fft_db_q31(const fd_cq31 *x, int32_t e, fm_q16 *y, uint32_t n) {
  const int64_t c = 2 * (int64_t)e - 46;
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t p = (uint64_t)fx_smull(x[i].re, x[i].re) +
                 (uint64_t)fx_smull(x[i].im, x[i].im);
    uint32_t hi = (uint32_t)(p >> 32);
    if (hi == 0U) {
      y[i] = p == 0U ? INT32_MIN : db_of(fm_log2_core((uint32_t)p), c);
    } else {
      uint32_t s = 32U - fx_clz(hi);
      y[i] = db_of(fm_log2_core((uint32_t)(p >> s)), c + (int64_t)s);
    }
  }
}
