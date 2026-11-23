/**
 * @file fd_mfcc.c
 * @brief Mel bands, their logs and the DCT
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_mfcc.h"
#include "fd_fft.h"
#include "fd_wide.h"
#include "fm_core.h"
#include "fm_sqrt.h"
#include "fm_trig.h"
#include "fx_arch.h"
#include <stdbool.h>

/* The mel scale of fn / n, up to a factor and an offset. */
static int64_t mel(uint32_t n, uint32_t fn) {
  return fm_log2_core(700U * n + fn);
}

FD_Status fd_mel_init(FD_Mel *m, uint16_t *cnt, uint16_t *w, uint32_t n,
                      uint32_t rate, uint32_t bands, uint32_t f_lo,
                      uint32_t f_hi) {
  if (n < 4U || n > FD_FFT_MAX || (n & (n - 1U)) != 0U || rate == 0U ||
      rate > 64000U || bands == 0U || bands > n / 2U || f_lo >= f_hi ||
      2U * f_hi > rate) {
    return FD_ERR_RANGE;
  }
  const int64_t lo = mel(n, f_lo * n);
  const uint64_t span = (uint64_t)(mel(n, f_hi * n) - lo);
  const uint64_t parts = bands + 1U;
  uint32_t k = 0U;
  while (k <= n / 2U && mel(n, k * rate) <= lo) {
    k++;
  }
  m->cnt = cnt;
  m->w = w;
  m->bands = bands;
  m->k0 = k;
  /* Row j is band j's rising edge and band j - 1's falling one. */
  int64_t a = lo;
  for (uint32_t j = 0U; j <= bands; j++) {
    int64_t b = lo + (int64_t)((span * (j + 1U) + parts / 2U) / parts);
    uint64_t d = (uint64_t)(b - a);
    uint32_t c = 0U;
    for (; k <= n / 2U; k++, c++) {
      int64_t v = mel(n, k * rate);
      if (v >= b) {
        break;
      }
      *w++ = (uint16_t)((((uint64_t)(v - a) << 15) + d / 2U) / d);
    }
    cnt[j] = (uint16_t)c;
    a = b;
  }
  return FD_OK;
}

int32_t fd_mel_q15(const FD_Mel *m, const fm_q15 *x, int32_t e, uint64_t *y) {
  const uint16_t *w = m->w;
  uint32_t k = m->k0;
  uint64_t fall = 0U;
  for (uint32_t j = 0U; j <= m->bands; j++) {
    uint64_t rise = 0U;
    for (uint32_t c = m->cnt[j]; c != 0U; c--, k++) {
      int32_t re = x[2U * k], im = x[2U * k + 1U];
      uint32_t p = (uint32_t)(re * re) + (uint32_t)(im * im);
      uint64_t wp = (uint64_t)*w++ * p;
      rise += wp;
      fall += ((uint64_t)p << 15) - wp;
    }
    if (j != 0U) {
      y[j - 1U] = fall;
    }
    fall = rise;
  }
  return 2 * e - 15;
}

#define LN2_Q32 2977044472U

static fm_q31 sat31(int64_t v) {
  return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : (fm_q31)v);
}

void fd_log_q16(const uint64_t *x, int32_t e, fm_q16 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    uint64_t v = x[i] != 0U ? x[i] : 1U;
    uint32_t hi = (uint32_t)(v >> 32);
    uint32_t s = hi != 0U ? 32U - fx_clz(hi) : 0U;
    int64_t l = fm_log2_core((uint32_t)(v >> s));
    int64_t whole = (l >> 32) + 16 + (int64_t)s + e;
    int64_t q = whole * (int64_t)LN2_Q32 + fx_umull_hi((uint32_t)l, LN2_Q32);
    y[i] = sat31((q + 32768) >> 16);
  }
}

static uint32_t root(uint64_t v) {
  uint32_t r = fm_isqrt64(v);
  return (uint64_t)r * r + r < v ? r + 1U : r;
}

FD_Status fd_dct2_init(FD_Dct *d, fm_q31 *c, uint32_t n, uint32_t k) {
  if (n < 2U || n > 65536U || k == 0U || k > n) {
    return FD_ERR_RANGE;
  }
  const uint32_t s0 = root((1ULL << 62) / n), s1 = root((1ULL << 63) / n);
  d->c = c;
  d->n = n;
  d->k = k;
  for (uint32_t j = 0U; j < k; j++) {
    for (uint32_t i = 0U; i < n; i++) {
      uint64_t t = ((uint64_t)j * (2U * i + 1U) << 30) + n / 2U;
      int64_t p = (int64_t)fm_cos_q31((uint32_t)(t / n)) * (j == 0U ? s0 : s1);
      *c++ = (fm_q31)((p + (1LL << 30)) >> 31);
    }
  }
  return FD_OK;
}

static fm_q31 round96(Sum96 s) {
  sum96_add(&s, 1LL << 30);
  if (s.hi > 0 || (s.hi == 0 && s.lo >= (1ULL << 62))) {
    return INT32_MAX;
  }
  if (s.hi < -1 || (s.hi == -1 && s.lo < (3ULL << 62))) {
    return INT32_MIN;
  }
  return (fm_q31)((int64_t)s.lo >> 31);
}

void fd_dct2_q31(const FD_Dct *d, const fm_q31 *x, fm_q31 *y) {
  const uint32_t n = d->n;
  const fm_q31 *c = d->c;
  uint64_t a = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    a += x[i] < 0 ? 0U - (uint32_t)x[i] : (uint32_t)x[i];
  }
  /* sum |x| < 2^32 keeps every partial sum under 2^63 */
  const bool narrow = a < (1ULL << 32);
  for (uint32_t j = 0U; j < d->k; j++, c += n) {
    if (narrow) {
      int64_t s = 0;
      for (uint32_t i = 0U; i < n; i++) {
        s = fx_smlal(s, x[i], c[i]);
      }
      y[j] = sat31((s + (1LL << 30)) >> 31);
    } else {
      y[j] = round96(sum96_dot(x, c, n));
    }
  }
}
