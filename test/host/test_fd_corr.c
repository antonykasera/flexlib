/**
 * @file test_fd_corr.c
 * @brief Host tests for fd_corr.h against exact 128-bit sums
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_corr.h"
#include "fm_div.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define MAXN 300U
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint64_t rng = 1234U;

static i128 rdiv(i128 v, unsigned s) {
  return (v + ((i128)1 << (s - 1U))) >> s;
}

static int32_t clamp(i128 v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* 0 full scale, 1 small (sum |x| < 2 up to 64 samples), 2 +-full scale,
 * 3 all most negative */
static void fill15(fm_q15 *x, uint32_t n, int kind) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = test_rng_u32(&rng);
    int32_t v = kind == 0   ? (int32_t)(u >> 16) - 32768
                : kind == 1 ? (int32_t)(u >> 16) % 1000
                : kind == 2 ? ((u & 1U) != 0U ? 32767 : -32768)
                            : -32768;
    x[i] = (fm_q15)v;
  }
}

static void fill31(fm_q31 *x, uint32_t n, int kind) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = test_rng_u32(&rng);
    x[i] = kind == 0   ? (fm_q31)u
           : kind == 1 ? (fm_q31)((int32_t)u / 70)
           : kind == 2 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN)
                       : INT32_MIN;
  }
}

/* y[k] = sum a[i] b[k - i] */
static i128 conv_at(const int32_t *a, uint32_t na, const int32_t *b,
                    uint32_t nb, uint32_t k) {
  i128 s = 0;
  for (uint32_t i = 0U; i < na; i++) {
    if (k >= i && k - i < nb) {
      s += (i128)a[i] * b[k - i];
    }
  }
  return s;
}

/* sum a[j + lag] b[j] */
static i128 corr_at(const int32_t *a, uint32_t na, const int32_t *b,
                    uint32_t nb, int64_t lag) {
  i128 s = 0;
  for (uint32_t j = 0U; j < nb; j++) {
    int64_t i = (int64_t)j + lag;
    if (i >= 0 && i < (int64_t)na) {
      s += (i128)a[i] * b[j];
    }
  }
  return s;
}

static int32_t wa[MAXN], wb[MAXN];

static void widen15(const fm_q15 *a, uint32_t na, const fm_q15 *b,
                    uint32_t nb) {
  for (uint32_t i = 0U; i < na; i++) {
    wa[i] = a[i];
  }
  for (uint32_t i = 0U; i < nb; i++) {
    wb[i] = b[i];
  }
}

static void check15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                    uint32_t sh) {
  static fm_q15 y[2U * MAXN + 16U], r[2U * MAXN + 16U];
  widen15(a, na, b, nb);
  uint32_t ny = na + nb - 1U;
  for (uint32_t k = 0U; k < ny; k++) {
    r[k] = (fm_q15)clamp(rdiv(conv_at(wa, na, wb, nb, k), 15U + sh), -32768,
                         32767);
  }
  fd_conv_q15(a, na, b, nb, sh, y);
  ASSERT_TRUE(memcmp(y, r, ny * sizeof y[0]) == 0);
  uint32_t first = test_rng_u32(&rng) % ny;
  uint32_t n = test_rng_u32(&rng) % (ny - first + 1U);
  y[n] = 0x5A5A;
  fd_conv_part_q15(a, na, b, nb, sh, first, n, y);
  ASSERT_TRUE(memcmp(y, r + first, n * sizeof y[0]) == 0);
  ASSERT_EQ(y[n], 0x5A5A);
  /* every lag from beyond one end to beyond the other */
  int32_t lag0 = -(int32_t)nb - 2 + (int32_t)(test_rng_u32(&rng) % 3U);
  uint32_t nl = na + nb + 3U - test_rng_u32(&rng) % 3U;
  fd_corr_q15(a, na, b, nb, lag0, nl, sh, y);
  for (uint32_t i = 0U; i < nl; i++) {
    i128 v = corr_at(wa, na, wb, nb, (int64_t)lag0 + i);
    ASSERT_EQ(y[i], clamp(rdiv(v, 15U + sh), -32768, 32767));
  } /* lags with no terms at all, on both sides */
  for (int side = 0; side < 2; side++) {
    int32_t l0 = side == 0 ? -(int32_t)nb - 9 : (int32_t)na + 1;
    memset(y, 0x5A, 9U * sizeof y[0]);
    fd_corr_q15(a, na, b, nb, l0, 8U, sh, y);
    for (uint32_t i = 0U; i < 8U; i++) {
      ASSERT_EQ(y[i], 0);
    }
    ASSERT_EQ(y[8], 0x5A5A);
  }
}

static void check31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                    uint32_t sh) {
  static fm_q31 y[2U * MAXN + 16U], r[2U * MAXN + 16U];
  uint32_t ny = na + nb - 1U;
  for (uint32_t k = 0U; k < ny; k++) {
    r[k] =
        clamp(rdiv(conv_at(a, na, b, nb, k), 31U + sh), INT32_MIN, INT32_MAX);
  }
  fd_conv_q31(a, na, b, nb, sh, y);
  ASSERT_TRUE(memcmp(y, r, ny * sizeof y[0]) == 0);
  uint32_t first = test_rng_u32(&rng) % ny;
  uint32_t n = test_rng_u32(&rng) % (ny - first + 1U);
  y[n] = 0x5A5A5A5A;
  fd_conv_part_q31(a, na, b, nb, sh, first, n, y);
  ASSERT_TRUE(memcmp(y, r + first, n * sizeof y[0]) == 0);
  ASSERT_EQ(y[n], 0x5A5A5A5A);
  int32_t lag0 = -(int32_t)nb - 2 + (int32_t)(test_rng_u32(&rng) % 3U);
  uint32_t nl = na + nb + 3U - test_rng_u32(&rng) % 3U;
  fd_corr_q31(a, na, b, nb, lag0, nl, sh, y);
  for (uint32_t i = 0U; i < nl; i++) {
    i128 v = corr_at(a, na, b, nb, (int64_t)lag0 + i);
    ASSERT_EQ(y[i], clamp(rdiv(v, 31U + sh), INT32_MIN, INT32_MAX));
  } /* lags with no terms at all, on both sides */
  for (int side = 0; side < 2; side++) {
    int32_t l0 = side == 0 ? -(int32_t)nb - 9 : (int32_t)na + 1;
    memset(y, 0x5A, 9U * sizeof y[0]);
    fd_corr_q31(a, na, b, nb, l0, 8U, sh, y);
    for (uint32_t i = 0U; i < 8U; i++) {
      ASSERT_EQ(y[i], 0);
    }
    ASSERT_EQ(y[8], 0x5A5A5A5A);
  }
}

static const uint32_t big[][2] = {{64U, 64U},  {256U, 32U}, {32U, 256U},
                                  {37U, 5U},   {5U, 37U},   {100U, 17U},
                                  {17U, 100U}, {13U, 14U},  {300U, 300U}};

void test_fd_corr_q15(void) {
  static fm_q15 a[MAXN], b[MAXN];
  static const uint32_t shifts[] = {0U, 1U, 4U, 16U};
  for (uint32_t na = 1U; na <= 12U; na++) {
    for (uint32_t nb = 1U; nb <= 12U; nb++) {
      for (int ka = 0; ka < 4; ka++) {
        for (int kb = 0; kb < 4; kb++) {
          fill15(a, na, ka);
          fill15(b, nb, kb);
          check15(a, na, b, nb, shifts[test_rng_u32(&rng) % 4U]);
        }
      }
    }
  }
  for (unsigned i = 0U; i < COUNT(big); i++) {
    for (int k = 0; k < 4; k++) {
      fill15(a, big[i][0], k == 3 ? 0 : k);
      fill15(b, big[i][1], k == 3 ? 1 : k);
      check15(a, big[i][0], b, big[i][1], k == 0 ? 0U : (uint32_t)k + 2U);
    }
  }
}

void test_fd_corr_q31(void) {
  static fm_q31 a[MAXN], b[MAXN];
  static const uint32_t shifts[] = {0U, 1U, 5U, 32U};
  for (uint32_t na = 1U; na <= 12U; na++) {
    for (uint32_t nb = 1U; nb <= 12U; nb++) {
      for (int ka = 0; ka < 4; ka++) {
        for (int kb = 0; kb < 4; kb++) {
          fill31(a, na, ka);
          fill31(b, nb, kb);
          check31(a, na, b, nb, shifts[test_rng_u32(&rng) % 4U]);
        }
      }
    }
  }
  for (unsigned i = 0U; i < COUNT(big); i++) {
    for (int k = 0; k < 4; k++) {
      fill31(a, big[i][0], k == 3 ? 0 : k);
      fill31(b, big[i][1], k == 3 ? 1 : k);
      check31(a, big[i][0], b, big[i][1], k == 0 ? 0U : (uint32_t)k + 2U);
    }
  }
}

/* the narrow kernels' bound: sum |b| = 2 - 1 LSB against the most negative
 * input gives the largest sum they hold; 2 takes the wide ones */
void test_fd_corr_sum_rule(void) {
  static fm_q15 a[MAXN], b[MAXN];
  static fm_q31 a31[MAXN], b31[MAXN];
  for (uint32_t extra = 0U; extra < 2U; extra++) {
    for (uint32_t nb = 2U; nb <= 9U; nb++) {
      uint32_t total = 65535U + extra;
      for (uint32_t i = 0U; i < nb; i++) {
        b[i] = (fm_q15)(-(int32_t)(total / nb + (i < total % nb ? 1U : 0U)));
      }
      fill15(a, 40U, 3);
      check15(a, 40U, b, nb, 0U);
      check15(b, nb, a, 40U, 0U);
      check15(a, 40U, b, nb, 1U);
      uint64_t t31 = (1ULL << 32) - 1U + extra;
      for (uint32_t i = 0U; i < nb; i++) {
        b31[i] = (fm_q31)(-(int64_t)(t31 / nb + (i < t31 % nb ? 1U : 0U)));
      }
      fill31(a31, 40U, 3);
      check31(a31, 40U, b31, nb, 0U);
      check31(b31, nb, a31, 40U, 0U);
      check31(a31, 40U, b31, nb, 1U);
    }
  }
  /* products that reach the bound only with integer data: max |a| sum |b| =
   * 16384 x 131071 = 2^31 - 2^14 (q31: 2^30 (2^33 - 1)) must be wide */
  for (uint32_t i = 0U; i < 8U; i++) {
    b[i] = (fm_q15)(i == 0U ? -16383 : -16384);
    a[i] = -16384;
    b31[i] = i == 0U ? -(1 << 30) + 1 : -(1 << 30);
    a31[i] = -(1 << 30);
  }
  check15(a, 8U, b, 8U, 0U);
  check31(a31, 8U, b31, 8U, 0U);
  /* a's largest value after the first chunk the scan reads: sum |b| = 65536 is
   * narrow only while max |a| < 32768 */
  for (uint32_t i = 0U; i < 40U; i++) {
    a[i] = (fm_q15)(i < 20U ? -3 : -32768);
  }
  for (uint32_t i = 0U; i < 4U; i++) {
    b[i] = -16384;
  }
  check15(a, 40U, b, 4U, 0U);
  /* 12-bit data: max |a| sum |b| = 2^30 at 256, 2^31 at 512 */
  for (uint32_t n = 256U; n <= 300U; n += 44U) {
    for (uint32_t i = 0U; i < n; i++) {
      a[i] = (fm_q15)(i % 7U == 0U ? 2047 : -2048);
      b[i] = (fm_q15)(i % 5U == 0U ? 2047 : -2048);
      a31[i] = i % 3U == 0U ? (1 << 27) - 1 : -(1 << 27);
      b31[i] = -(1 << 27);
    }
    check15(a, n, b, n, 0U);
    check15(a, n, a, n, 2U);
    check31(a31, n, b31, n, 0U);
  }
}

/* windows past 2^16 terms: the q31 96-bit sums run in chunks */
void test_fd_corr_long(void) {
  enum { N = 70001 };
  static fm_q31 a[N], b[N], y[4];
  for (int kind = 0; kind < 3; kind++) {
    fill31(a, N, kind == 0 ? 3 : 0);
    fill31(b, N, kind == 2 ? 2 : 3);
    for (uint32_t sh = 16U; sh <= 32U; sh += 16U) {
      fd_corr_q31(a, N, b, N, -1, 3U, sh, y);
      for (uint32_t i = 0U; i < 3U; i++) {
        i128 v = corr_at(a, N, b, N, (int64_t)i - 1);
        ASSERT_EQ(y[i], clamp(rdiv(v, 31U + sh), INT32_MIN, INT32_MAX));
      }
    }
  }
}

/* ---- autocorrelation: the exact sums, the exponent rule ---- */

static int32_t ref_rs(i128 r0) {
  int32_t b = 0;
  while (b < 127 && (r0 >> b) != 0) {
    b++;
  }
  int32_t rs = b - 31;
  if (rs > 0 && rdiv(r0, (unsigned)rs) >= ((i128)1 << 31)) {
    rs++;
  }
  return rs;
}

static int32_t ref_out(i128 v, int32_t rs) {
  return rs <= 0 ? (int32_t)(v * ((i128)1 << -rs))
                 : (int32_t)rdiv(v, (unsigned)rs);
}

/* q15 (wide == 0) or q31 data in w, r from the library, e its exponent */
static void check_ac(const int32_t *w, uint32_t n, uint32_t nl, const fm_q31 *r,
                     int32_t e, bool q31) {
  i128 r0 = corr_at(w, n, w, n, 0);
  if (r0 == 0) {
    ASSERT_EQ(e, 0);
    for (uint32_t k = 0U; k < nl; k++) {
      ASSERT_EQ(r[k], 0);
    }
    return;
  }
  int32_t rs = ref_rs(r0);
  ASSERT_EQ(e, q31 ? rs - 31 : rs + 1);
  ASSERT_TRUE(r[0] >= (1 << 30));
  for (uint32_t k = 0U; k < nl; k++) {
    ASSERT_EQ(r[k], ref_out(corr_at(w, n, w, n, k), rs));
  }
}

void test_fd_corr_autocorr(void) {
  static fm_q15 x[MAXN];
  static fm_q31 x31[MAXN], r[MAXN + 8U];
  for (uint32_t n = 1U; n <= 40U; n++) {
    for (int kind = 0; kind < 6; kind++) {
      for (uint32_t i = 0U; i < n; i++) {
        uint32_t u = test_rng_u32(&rng);
        int32_t v = kind == 0   ? (int32_t)(u >> 16) - 32768
                    : kind == 1 ? (int32_t)(u >> 16) % 1000
                    : kind == 2 ? ((u & 1U) != 0U ? 32767 : -32768)
                    : kind == 3 ? (int32_t)(u >> 16) % 2048
                    : kind == 4 ? (i == n / 2U ? -5 : 0)
                                : 0;
        x[i] = (fm_q15)v;
        x31[i] = kind == 0   ? (fm_q31)u
                 : kind == 1 ? (fm_q31)((int32_t)u >> 8)
                 : kind == 2 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN)
                             : (fm_q31)(v * 65536);
      }
      uint32_t nl = 1U + test_rng_u32(&rng) % (n + 3U);
      int32_t e = fd_autocorr_q15(x, n, r, nl);
      widen15(x, n, x, n);
      check_ac(wa, n, nl, r, e, false);
      e = fd_autocorr_q31(x31, n, r, nl);
      check_ac(x31, n, nl, r, e, true);
    }
  }
  /* the block scaled by 2^-3: the same r, e lower by 6 */
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (fm_q15)(((int32_t)(test_rng_u32(&rng) >> 16) - 32768) & ~7);
  }
  int32_t e0 = fd_autocorr_q15(x, 64U, r, 9U);
  fm_q31 r0[9];
  memcpy(r0, r, sizeof r0);
  for (uint32_t i = 0U; i < 64U; i++) {
    x[i] = (fm_q15)(x[i] / 8);
  }
  ASSERT_EQ(fd_autocorr_q15(x, 64U, r, 9U), e0 - 6);
  ASSERT_TRUE(memcmp(r, r0, sizeof r0) == 0);
  /* r[0] = 2^32 - 1 rounds to 2^31 at rs = 1: rs is 2 */
  static const fm_q15 carry[] = {-32768, -32768, -32768, 32767, 255, 22, 5};
  int32_t e = fd_autocorr_q15(carry, 7U, r, 3U);
  ASSERT_EQ(e, 3);
  ASSERT_EQ(r[0], 1 << 30);
  widen15(carry, 7U, carry, 7U);
  check_ac(wa, 7U, 3U, r, e, false);
}

/* ---- peak: round((yp - ym) 2^16 / (2 (2 y0 - ym - yp))) ---- */

static int32_t ref_peak(const int32_t *y, uint32_t n) {
  uint32_t i = 0U;
  for (uint32_t k = 1U; k < n; k++) {
    i = y[k] > y[i] ? k : i;
  }
  int64_t d = 0;
  if (i > 0U && i + 1U < n) {
    i128 num = ((i128)y[i + 1U] - y[i - 1U]) * 65536;
    i128 den = 2 * (2 * (i128)y[i] - y[i - 1U] - y[i + 1U]);
    if (den > 0) {
      i128 t = 2 * num + den, q = t / (2 * den);
      d = (int64_t)(q * 2 * den > t ? q - 1 : q); /* floor */
    }
  }
  return (int32_t)((int64_t)i * 65536 + d);
}

void test_fd_corr_peak(void) {
  static fm_q15 y[64];
  static fm_q31 y31[64];
  static int32_t w[64];
  for (uint32_t it = 0U; it < 200000U; it++) {
    uint32_t n = 1U + test_rng_u32(&rng) % 7U;
    int kind = (int)(it % 4U);
    for (uint32_t i = 0U; i < n; i++) {
      uint32_t u = test_rng_u32(&rng);
      w[i] = kind == 0   ? (int32_t)(u >> 16) - 32768
             : kind == 1 ? (int32_t)(u % 5U) - 2
             : kind == 2 ? ((u & 1U) != 0U ? 32767 : -32768)
                         : (int32_t)(u % 3U) + 32765;
      y[i] = (fm_q15)w[i];
    }
    fm_q15 pk;
    ASSERT_EQ(fd_peak_q15(y, n, &pk), ref_peak(w, n));
    for (uint32_t i = 0U; i < n; i++) {
      w[i] = kind == 0   ? (int32_t)test_rng_u32(&rng)
             : kind == 2 ? ((w[i] & 1) != 0 ? INT32_MAX : INT32_MIN)
                         : w[i] * 65536 + (kind == 3 ? 65535 : 0);
      y31[i] = w[i];
    }
    fm_q31 pk31;
    ASSERT_EQ(fd_peak_q31(y31, n, &pk31), ref_peak(w, n));
  }
  y[0] = 7;
  ASSERT_EQ(fd_peak_q15(y, 1U, NULL), 0);
}

/* ---- Levinson against a double recursion ---- */

static int ref_lev(const double *r, uint32_t p, double *a, double *k,
                   double *err) {
  double e = r[0];
  for (uint32_t i = 1U; i <= p; i++) {
    double acc = r[i];
    for (uint32_t j = 1U; j < i; j++) {
      acc -= a[j - 1U] * r[i - j];
    }
    double ki = acc / e;
    double t[64];
    for (uint32_t j = 1U; j < i; j++) {
      t[j - 1U] = a[j - 1U] - ki * a[i - j - 1U];
    }
    memcpy(a, t, (i - 1U) * sizeof a[0]);
    a[i - 1U] = ki;
    k[i - 1U] = ki;
    e *= 1.0 - ki * ki;
  }
  *err = e;
  return 0;
}

/* the same recursion in 128 bits, each step's sums rounded once (ties up) */
static FD_Status model_lev(const fm_q31 *r, uint32_t p, uint32_t ash,
                           int32_t *a, int32_t *k, int32_t *err) {
  if (r[0] <= 0) {
    return FD_ERR_UNSTABLE;
  }
  const unsigned as = 31U - ash;
  int32_t e = r[0];
  for (uint32_t i = 1U; i <= p; i++) {
    i128 s = (i128)r[i] * ((i128)1 << as);
    for (uint32_t j = 1U; j < i; j++) {
      s -= (i128)a[j - 1U] * r[i - j];
    }
    i128 acc = rdiv(s, as);
    if (acc >= e || acc <= -(i128)e) {
      return FD_ERR_UNSTABLE;
    }
    int32_t ki = fm_div_q31((fm_q31)acc, e);
    int32_t t[64];
    for (uint32_t j = 1U; j < i; j++) {
      i128 v = rdiv(
          (i128)a[j - 1U] * ((i128)1 << 31) - (i128)ki * a[i - j - 1U], 31U);
      if (v < INT32_MIN || v > INT32_MAX) {
        return FD_ERR_RANGE;
      }
      t[j - 1U] = (int32_t)v;
    }
    memcpy(a, t, (i - 1U) * sizeof a[0]);
    a[i - 1U] = ash == 0U ? ki : (int32_t)rdiv(ki, ash);
    i128 en = rdiv((i128)e * ((i128)1 << 31) - (i128)ki * acc, 31U);
    if (en <= 0) {
      return FD_ERR_UNSTABLE;
    }
    e = (int32_t)en;
    k[i - 1U] = ki;
  }
  *err = e;
  return FD_OK;
}

/* worst a, k, err errors (LSB of a's Q27, of q31) over AR(2) data with pole
 * radius in [r0, r1] */
static void lev_run(double r0, double r1, double worst[3]) {
  static fm_q15 x[512];
  static fm_q31 r[16], a[16], k[16];
  worst[0] = worst[1] = worst[2] = 0.0;
  for (int trial = 0; trial < 300; trial++) {
    double rho = r0 + (r1 - r0) * (double)(test_rng_u32(&rng) % 1001U) / 1000.0;
    double th = 3.14159 * (double)(test_rng_u32(&rng) % 1000U) / 1000.0;
    double c1 = 2.0 * rho * cos(th), c2 = -rho * rho, s1 = 0.0, s2 = 0.0;
    for (uint32_t i = 0U; i < 512U; i++) {
      double v = (double)((int32_t)(test_rng_u32(&rng) >> 20) - 2048) +
                 c1 * s1 + c2 * s2;
      s2 = s1;
      s1 = v;
      double q = v * 4.0 * (1.0 - rho);
      x[i] = (fm_q15)(q > 32767.0 ? 32767.0 : (q < -32768.0 ? -32768.0 : q));
    }
    uint32_t p = 1U + (uint32_t)trial % 12U;
    (void)fd_autocorr_q15(x, 512U, r, p + 1U);
    double rd[16], ad[16], kd[16], ed;
    for (uint32_t i = 0U; i <= p; i++) {
      rd[i] = (double)r[i] / 2147483648.0;
    }
    ref_lev(rd, p, ad, kd, &ed);
    fm_q31 err, ma[16], mk[16], me;
    ASSERT_EQ(fd_levinson_q31(r, p, 4U, a, k, &err), FD_OK);
    ASSERT_EQ(model_lev(r, p, 4U, ma, mk, &me), FD_OK);
    ASSERT_TRUE(memcmp(a, ma, p * sizeof a[0]) == 0 &&
                memcmp(k, mk, p * sizeof k[0]) == 0 && err == me);
    for (uint32_t i = 0U; i < p; i++) {
      double da = fabs((double)a[i] / 134217728.0 - ad[i]) * 134217728.0;
      double dk = fabs((double)k[i] / 2147483648.0 - kd[i]) * 2147483648.0;
      worst[0] = da > worst[0] ? da : worst[0];
      worst[1] = dk > worst[1] ? dk : worst[1];
    }
    double de = fabs((double)err / 2147483648.0 - ed) * 2147483648.0;
    worst[2] = de > worst[2] ? de : worst[2];
  }
}

void test_fd_corr_levinson(void) {
  static fm_q31 r[16], a[16], k[16];
  double w[3];
  lev_run(0.3, 0.8, w);
  printf("  levinson q31, poles 0.3-0.8, order 1-12: a %.1f LSB (Q27), k %.1f, "
         "err %.1f\n",
         w[0], w[1], w[2]);
  ASSERT_TRUE(w[0] < 256.0 && w[1] < 4096.0 && w[2] < 64.0);
  lev_run(0.8, 0.99, w);
  printf("  levinson q31, poles 0.8-0.99: a %.1f LSB, k %.1f, err %.1f\n", w[0],
         w[1], w[2]);
  ASSERT_TRUE(w[0] < 8192.0 && w[1] < 65536.0 && w[2] < 64.0);
  /* random r (mostly not positive definite) and every ashift: the statuses
   * and results the model gives */
  for (int trial = 0; trial < 100000; trial++) {
    uint32_t p = 1U + test_rng_u32(&rng) % 8U, ash = test_rng_u32(&rng) % 31U;
    r[0] = (fm_q31)(test_rng_u32(&rng) >> 1);
    for (uint32_t i = 1U; i <= p; i++) {
      r[i] = (fm_q31)((int32_t)test_rng_u32(&rng) >> (test_rng_u32(&rng) % 8U));
    }
    fm_q31 ma[16], mk[16], me, err;
    FD_Status st = fd_levinson_q31(r, p, ash, a, k, &err);
    ASSERT_EQ(st, model_lev(r, p, ash, ma, mk, &me));
    if (st == FD_OK) {
      ASSERT_TRUE(memcmp(a, ma, p * sizeof a[0]) == 0 &&
                  memcmp(k, mk, p * sizeof k[0]) == 0 && err == me);
    }
  }
  /* AR(1): r = rho^k gives a = (rho, 0, ...), k likewise */
  for (uint32_t i = 0U; i < 6U; i++) {
    r[i] = (fm_q31)(1073741824.0 * pow(0.75, (double)i));
  }
  ASSERT_EQ(fd_levinson_q31(r, 5U, 1U, a, k, NULL), FD_OK);
  ASSERT_NEAR(a[0], 0.75 * 1073741824.0, 2.0);
  for (uint32_t i = 1U; i < 5U; i++) {
    ASSERT_NEAR(a[i], 0.0, 2.0);
    ASSERT_NEAR(k[i], 0.0, 4.0);
  }
  /* a reflection coefficient of 1, r[0] <= 0, a coefficient past 2^ashift */
  r[0] = r[1] = 1 << 30;
  ASSERT_EQ(fd_levinson_q31(r, 1U, 0U, a, NULL, NULL), FD_ERR_UNSTABLE);
  r[0] = 0;
  ASSERT_EQ(fd_levinson_q31(r, 1U, 0U, a, NULL, NULL), FD_ERR_UNSTABLE);
  for (uint32_t i = 0U; i < 4U; i++) {
    r[i] =
        (fm_q31)(1073741824.0 * cos(0.05 * (double)i) * pow(0.99, (double)i));
  }
  ASSERT_EQ(fd_levinson_q31(r, 2U, 0U, a, NULL, NULL), FD_ERR_RANGE);
  ASSERT_EQ(fd_levinson_q31(r, 2U, 2U, a, NULL, NULL), FD_OK);
}

/* ---- correlation through FFTs: SNR against the exact sums, peak lags ---- */

#include "fd_fft.h"

static double corr_fft_snr(const int32_t *wa_, uint32_t na, const int32_t *wb_,
                           uint32_t nb, uint32_t n, int32_t *peak,
                           int32_t *peak_exact) {
  static fm_q15 fa[1024] __attribute__((aligned(4)));
  static fm_q15 fb[1024] __attribute__((aligned(4)));
  memset(fa, 0, sizeof fa);
  memset(fb, 0, sizeof fb);
  for (uint32_t i = 0U; i < na; i++) {
    fa[i] = (fm_q15)wa_[i];
  }
  for (uint32_t i = 0U; i < nb; i++) {
    fb[i] = (fm_q15)wb_[i];
  }
  int32_t e = fd_corr_fft_q15(fa, fb, n);
  double sig = 0.0, err = 0.0, best = -1e300, best_x = -1e300;
  for (int32_t lag = 1 - (int32_t)nb; lag < (int32_t)na; lag++) {
    double x = (double)corr_at(wa_, na, wb_, nb, lag) / 1073741824.0;
    double f = ldexp((double)fa[lag < 0 ? (int32_t)n + lag : lag], e - 15);
    sig += x * x;
    err += (f - x) * (f - x);
    if (f > best) {
      best = f;
      *peak = lag;
    }
    if (x > best_x) {
      best_x = x;
      *peak_exact = lag;
    }
  }
  return err > 0.0 ? 10.0 * log10(sig / err) : 200.0;
}

void test_fd_corr_fft(void) {
  static int32_t s1[512], s2[512];
  double worst = 200.0;
  uint32_t miss = 0U, cases = 0U;
  for (int trial = 0; trial < 600; trial++) {
    uint32_t n = 512U, na = 256U, nb = 256U;
    int kind = trial % 3;
    int32_t delay = (int32_t)(test_rng_u32(&rng) % 201U) - 100;
    double snr_db = (double)(trial % 5) * 10.0; /* 0..40 dB */
    double g = pow(10.0, -snr_db / 20.0);
    double ph = 0.0, f = 0.01;
    for (uint32_t i = 0U; i < 512U; i++) {
      double v;
      if (kind == 2) { /* chirp */
        ph += f;
        f += 0.0007;
        v = 12000.0 * sin(6.2831853 * ph);
      } else {
        v = (double)((int32_t)(test_rng_u32(&rng) >> 16) - 32768) *
            (kind == 1 ? 0.06 : 0.4);
      }
      s1[i] = (int32_t)v;
    }
    /* a = s[128 + i], b = s[128 + j - delay] + noise: the peak at lag -delay,
     * with at least 156 samples in common */
    for (uint32_t j = 0U; j < nb; j++) {
      double v = (double)s1[128 + (int32_t)j - delay];
      v += g * (double)((int32_t)(test_rng_u32(&rng) >> 16) - 32768) * 0.3;
      s2[j] = (int32_t)(v > 32767.0 ? 32767.0 : (v < -32768.0 ? -32768.0 : v));
    }
    memmove(s1, s1 + 128, na * sizeof s1[0]);
    int32_t p, px;
    double snr = corr_fft_snr(s1, na, s2, nb, n, &p, &px);
    worst = snr < worst ? snr : worst;
    miss += p != px ? 1U : 0U;
    cases++;
  }
  printf("  corr fft q15, 256 x 256 in 512: worst SNR %.1f dB, peak lag "
         "differs in %u of %u\n",
         worst, miss, cases);
  ASSERT_TRUE(worst > 40.0);
  ASSERT_EQ(miss, 0U);
}
