/**
 * @file fd_stats.c
 * @brief Block statistics from exact sums
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_vec.h"
#include "fd_wide.h"
#include "fm_sqrt.h"
#include <stddef.h>

/* ---- exact sums ---- */

int64_t fd_sum_q15(const fm_q15 *x, uint32_t n) {
  int64_t s = 0;
  while (n > 0U) {
    uint32_t m = n < 65536U ? n : 65536U; /* |sum| <= 2^31 in int32 */
    int32_t t = 0;
    FX_UNROLL(4)
    for (uint32_t i = 0U; i < m; i++) {
      t += x[i];
    }
    s += t;
    x += m;
    n -= m;
  }
  return s;
}

int64_t fd_sum_q31(const fm_q31 *x, uint32_t n) {
  int64_t s = 0;
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    s += x[i];
  }
  return s;
}

/* round(s / n), ties up */
static int64_t div_round(int64_t s, uint32_t n) {
  if (s >= INT32_MIN && s <= INT32_MAX && n <= (uint32_t)INT32_MAX) {
    int32_t q = (int32_t)s / (int32_t)n;
    int32_t r = (int32_t)s - q * (int32_t)n;
    if (r < 0) {
      r += (int32_t)n;
      q--;
    }
    return q + (2U * (uint32_t)r >= n ? 1 : 0);
  }
  int64_t q = s / (int64_t)n;
  int64_t r = s % (int64_t)n;
  if (r < 0) {
    r += n;
    q--;
  }
  return q + (2 * r >= (int64_t)n ? 1 : 0);
}

fm_q15 fd_mean_q15(const fm_q15 *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  return (fm_q15)div_round(fd_sum_q15(x, n), n);
}

fm_q31 fd_mean_q31(const fm_q31 *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  return (fm_q31)div_round(fd_sum_q31(x, n), n);
}

int64_t fd_dot_q15(const fm_q15 *a, const fm_q15 *b, uint32_t n) {
  int64_t s = 0;
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    s += a[i] * b[i];
  }
  return s;
}

uint64_t fd_power_q15(const fm_q15 *x, uint32_t n) {
  uint64_t s = 0U;
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    s += (uint32_t)(x[i] * x[i]);
  }
  return s;
}

int64_t fd_dot_q31(const fm_q31 *a, const fm_q31 *b, uint32_t n) {
  return sum96_q48(sum96_dot(a, b, n));
}

static FD_U128 power96_q31(const fm_q31 *x, uint32_t n) {
  Sum96 s = sum96_dot(x, x, n);
  return fd_u128((uint32_t)s.hi, s.lo);
}

uint64_t fd_power_q31(const fm_q31 *x, uint32_t n) {
  FD_U128 s = fd_u128_add(power96_q31(x, n), fd_u128(0U, 8192U));
  if (s.hi >= 16384U) {
    return UINT64_MAX;
  }
  return (s.hi << 50) | (s.lo >> 14);
}

/* ---- rms, var, std: correctly rounded from the exact sums ---- */

/* round(sqrt(num / (d1 d2))); the quotient fits 64 bits */
static uint32_t sqrt_round(FD_U128 num, uint32_t d1, uint32_t d2) {
  FD_U128 v = num;
  (void)fd_u128_divmod32(&v, d1);
  (void)fd_u128_divmod32(&v, d2);
  uint32_t r = fm_isqrt64(v.lo);
  uint64_t t = 2U * (uint64_t)r + 1U;
  FD_U128 up = fd_u128_mul32(fd_u128_mul32(fd_u128_mul64(t, t), d1), d2);
  return fd_u128_ge(fd_u128_shl(num, 2U), up) ? r + 1U : r;
}

/* n sum x^2 - (sum x)^2 */
static FD_U128 var_num(int64_t s1, FD_U128 s2, uint32_t n) {
  uint64_t m = s1 < 0 ? 0U - (uint64_t)s1 : (uint64_t)s1;
  return fd_u128_sub(fd_u128_mul32(s2, n), fd_u128_mul64(m, m));
}

/* round(num / (n (n - 1) 2^f)), saturated to 2^(sat) - 1 */
static uint32_t var_round(FD_U128 num, uint32_t n, uint32_t f, uint32_t sat) {
  uint64_t d = (uint64_t)n * (n - 1U);
  FD_U128 q = fd_u128_add(fd_u128_shl(num, 1U), fd_u128_shl(fd_u128(0U, d), f));
  (void)fd_u128_divmod32(&q, n);
  (void)fd_u128_divmod32(&q, n - 1U);
  q = fd_u128_shr(q, f + 1U);
  uint32_t max = (uint32_t)((1ULL << sat) - 1U);
  return q.lo > max ? max : (uint32_t)q.lo;
}

static void moments_q15(const fm_q15 *x, uint32_t n, int64_t *s1, FD_U128 *s2) {
  *s1 = fd_sum_q15(x, n);
  *s2 = fd_u128(0U, fd_power_q15(x, n));
}

static void moments_q31(const fm_q31 *x, uint32_t n, int64_t *s1, FD_U128 *s2) {
  *s1 = fd_sum_q31(x, n);
  *s2 = power96_q31(x, n);
}

fm_q15 fd_rms_q15(const fm_q15 *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  uint32_t r = sqrt_round(fd_u128(0U, fd_power_q15(x, n)), n, 1U);
  return (fm_q15)(r > 32767U ? 32767U : r);
}

fm_q31 fd_rms_q31(const fm_q31 *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  uint32_t r = sqrt_round(power96_q31(x, n), n, 1U);
  return (fm_q31)(r > (uint32_t)INT32_MAX ? (uint32_t)INT32_MAX : r);
}

fm_q15 fd_var_q15(const fm_q15 *x, uint32_t n) {
  FX_ASSERT(n >= 2U && n < (1UL << 24));
  int64_t s1;
  FD_U128 s2;
  moments_q15(x, n, &s1, &s2);
  return (fm_q15)var_round(var_num(s1, s2, n), n, 15U, 15U);
}

fm_q31 fd_var_q31(const fm_q31 *x, uint32_t n) {
  FX_ASSERT(n >= 2U && n < (1UL << 24));
  int64_t s1;
  FD_U128 s2;
  moments_q31(x, n, &s1, &s2);
  return (fm_q31)var_round(var_num(s1, s2, n), n, 31U, 31U);
}

fm_q15 fd_std_q15(const fm_q15 *x, uint32_t n) {
  FX_ASSERT(n >= 2U && n < (1UL << 24));
  int64_t s1;
  FD_U128 s2;
  moments_q15(x, n, &s1, &s2);
  uint32_t r = sqrt_round(var_num(s1, s2, n), n, n - 1U);
  return (fm_q15)(r > 32767U ? 32767U : r);
}

fm_q31 fd_std_q31(const fm_q31 *x, uint32_t n) {
  FX_ASSERT(n >= 2U && n < (1UL << 24));
  int64_t s1;
  FD_U128 s2;
  moments_q31(x, n, &s1, &s2);
  uint32_t r = sqrt_round(var_num(s1, s2, n), n, n - 1U);
  return (fm_q31)(r > (uint32_t)INT32_MAX ? (uint32_t)INT32_MAX : r);
}

/* ---- min, max ---- */

#define FD_PICK(v, i, better)                                                  \
  if ((v)better best) {                                                        \
    best = (v);                                                                \
    at = (i);                                                                  \
  }

#define FD_EXTREME(T, x, n, index, better)                                     \
  do {                                                                         \
    FX_ASSERT((n) >= 1U);                                                      \
    T best = (x)[0];                                                           \
    uint32_t at = 0U;                                                          \
    uint32_t i_ = 1U;                                                          \
    for (; i_ + 4U <= (n); i_ += 4U) {                                         \
      T v0 = (x)[i_], v1 = (x)[i_ + 1U], v2 = (x)[i_ + 2U], v3 = (x)[i_ + 3U]; \
      FD_PICK(v0, i_, better)                                                  \
      FD_PICK(v1, i_ + 1U, better)                                             \
      FD_PICK(v2, i_ + 2U, better)                                             \
      FD_PICK(v3, i_ + 3U, better)                                             \
    }                                                                          \
    for (; i_ < (n); i_++) {                                                   \
      FD_PICK((x)[i_], i_, better)                                             \
    }                                                                          \
    if ((index) != NULL) {                                                     \
      *(index) = at;                                                           \
    }                                                                          \
    return best;                                                               \
  } while (0)

fm_q15 fd_min_q15(const fm_q15 *x, uint32_t n, uint32_t *index) {
  FD_EXTREME(fm_q15, x, n, index, <);
}

fm_q31 fd_min_q31(const fm_q31 *x, uint32_t n, uint32_t *index) {
  FD_EXTREME(fm_q31, x, n, index, <);
}

fm_q15 fd_max_q15(const fm_q15 *x, uint32_t n, uint32_t *index) {
  FD_EXTREME(fm_q15, x, n, index, >);
}

fm_q31 fd_max_q31(const fm_q31 *x, uint32_t n, uint32_t *index) {
  FD_EXTREME(fm_q31, x, n, index, >);
}

FX_INLINE uint32_t mag32(int32_t v) {
  return v < 0 ? 0U - (uint32_t)v : (uint32_t)v;
}

fm_q15 fd_absmax_q15(const fm_q15 *x, uint32_t n, uint32_t *index) {
  FX_ASSERT(n >= 1U);
  uint32_t best = mag32(x[0]);
  uint32_t at = 0U;
  for (uint32_t i = 1U; i < n; i++) {
    uint32_t m = mag32(x[i]);
    if (m > best) {
      best = m;
      at = i;
    }
  }
  if (index != NULL) {
    *index = at;
  }
  return (fm_q15)(best > 32767U ? 32767U : best);
}

fm_q31 fd_absmax_q31(const fm_q31 *x, uint32_t n, uint32_t *index) {
  FX_ASSERT(n >= 1U);
  uint32_t best = mag32(x[0]);
  uint32_t at = 0U;
  for (uint32_t i = 1U; i < n; i++) {
    uint32_t m = mag32(x[i]);
    if (m > best) {
      best = m;
      at = i;
    }
  }
  if (index != NULL) {
    *index = at;
  }
  return (fm_q31)(best > (uint32_t)INT32_MAX ? (uint32_t)INT32_MAX : best);
}

void fd_minmax_q15(const fm_q15 *x, uint32_t n, fm_q15 *min, fm_q15 *max) {
  FX_ASSERT(n >= 1U);
  fm_q15 lo = x[0], hi = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    lo = x[i] < lo ? x[i] : lo;
    hi = x[i] > hi ? x[i] : hi;
  }
  *min = lo;
  *max = hi;
}

void fd_minmax_q31(const fm_q31 *x, uint32_t n, fm_q31 *min, fm_q31 *max) {
  FX_ASSERT(n >= 1U);
  fm_q31 lo = x[0], hi = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    lo = x[i] < lo ? x[i] : lo;
    hi = x[i] > hi ? x[i] : hi;
  }
  *min = lo;
  *max = hi;
}
