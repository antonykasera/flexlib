/**
 * @file test_fd_corr_f32.c
 * @brief Host tests for fd_conv_f32, fd_conv_part_f32 and fd_corr_f32
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_corr.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef __float128 f128;

#define NMAX 80U
#define YMAX (3U * NMAX)

static float a[NMAX], b[NMAX], br[NMAX], y[YMAX], y2[YMAX];
static fm_q31 w[2U * NMAX];
static double worst_of_bound;

static float unif(uint64_t *r, int k) {
  return ldexpf((float)(int32_t)test_rng_u32(r) * 0x1p-31f, k);
}

/* values over 2^-spread .. 1 times 2^k */
static void fill(uint64_t *r, float *x, uint32_t n, uint32_t spread, int k) {
  for (uint32_t i = 0U; i < n; i++) {
    int e = k - (int)(test_rng_u32(r) % (spread + 1U));
    x[i] = unif(r, e);
  }
}

/* own exponent, the shorter raised
 * until sum |q| < 2^32 holds after reconverting */
static int bexp(const float *x, uint32_t n) {
  int e = -125;
  for (uint32_t i = 0U; i < n; i++) {
    while (fabsf(x[i]) >= ldexpf(1.0f, e)) {
      e++;
    }
  }
  return e;
}

static void quant(const float *x, uint32_t n, int e, int64_t *q) {
  for (uint32_t i = 0U; i < n; i++) {
    q[i] = (int64_t)nearbyint(ldexp((double)x[i], 31 - e));
  }
}

static int quant_short(const float *x, uint32_t n, int64_t *q) {
  int e = bexp(x, n);
  quant(x, n, e, q);
  uint64_t g = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    g += (uint64_t)llabs(q[i]);
  }
  if (g < 0x100000000U) {
    return e;
  }
  int k = 1;
  while (((g + n) >> k) + n >= 0x100000000U) {
    k++;
  }
  quant(x, n, e + k, q);
  return e + k;
}

/* pairs of output o: (i, j) with a[i] b[j]; conv i + j = o, corr
 * i = j + lag0 + o */
static uint32_t pairs(int conv, int32_t lag0, uint32_t na, uint32_t nb,
                      uint32_t o, uint32_t *pi, uint32_t *pj) {
  uint32_t c = 0U;
  for (uint32_t j = 0U; j < nb; j++) {
    int64_t i = conv ? (int64_t)o - j : (int64_t)j + lag0 + o;
    if (i >= 0 && i < (int64_t)na) {
      pi[c] = (uint32_t)i;
      pj[c++] = j;
    }
  }
  return c;
}

static int64_t qa[NMAX], qb[NMAX];
static uint32_t pi[NMAX], pj[NMAX];

/* outputs first.. of n against the model (bits) and the header's bound */
static int check(int conv, int32_t lag0, uint32_t na, uint32_t nb,
                 uint32_t first, uint32_t n, const float *got) {
  int ea, eb;
  if (nb <= na) {
    ea = bexp(a, na);
    quant(a, na, ea, qa);
    eb = quant_short(b, nb, qb);
  } else {
    ea = quant_short(a, na, qa);
    eb = bexp(b, nb);
    quant(b, nb, eb, qb);
  }
  const f128 scale = (f128)ldexp(1.0, ea + eb - 62);
  const float *ls = nb <= na ? a : b, *ss = nb <= na ? b : a;
  const uint32_t nl = nb <= na ? na : nb, ns = nb <= na ? nb : na;
  double sum_s = 0.0, max_l = 0.0;
  for (uint32_t i = 0U; i < ns; i++) {
    sum_s += fabs((double)ss[i]);
  }
  for (uint32_t i = 0U; i < nl; i++) {
    max_l = fmax(max_l, fabs((double)ls[i]));
  }
  for (uint32_t k = 0U; k < n; k++) {
    uint32_t c = pairs(conv, lag0, na, nb, first + k, pi, pj);
    f128 mq = 0, ex = 0;
    double wl = 0.0;
    for (uint32_t p = 0U; p < c; p++) {
      mq += (f128)(qa[pi[p]] * (f128)qb[pj[p]]);
      ex += (f128)((double)a[pi[p]] * (double)b[pj[p]]);
      wl += fabs((double)(nb <= na ? a[pi[p]] : b[pj[p]]));
    }
    float want = (float)(mq * scale);
    if (memcmp(&want, &got[k], sizeof want) != 0) {
      printf("    %s na %u nb %u out %u: got %a want %a\n",
             conv ? "conv" : "corr", na, nb, first + k, (double)got[k],
             (double)want);
      return 0;
    }
    float m = fabsf(got[k]);
    double ulp = (double)(nextafterf(m, INFINITY) - m);
    double bound = 0.5 * ulp + 0x1p-31 * sum_s * (max_l + wl);
    double err = fabs((double)((f128)got[k] - ex));
    if (!(err <= bound)) {
      printf("    %s na %u nb %u out %u: err %g bound %g\n",
             conv ? "conv" : "corr", na, nb, first + k, err, bound);
      return 0;
    }
    double r = bound > 0.0 ? err / bound : 0.0;
    worst_of_bound = r > worst_of_bound ? r : worst_of_bound;
  }
  return 1;
}

void test_fd_corr_f32_model(void) {
  uint64_t r = 0xC0F32U;
  for (unsigned t = 0U; t < 1500U; t++) {
    uint32_t na = 1U + test_rng_u32(&r) % NMAX;
    uint32_t nb = 1U + test_rng_u32(&r) % NMAX;
    uint32_t spread = test_rng_u32(&r) % 4U == 0U ? 40U : test_rng_u32(&r) % 8U;
    fill(&r, a, na, spread, (int)(test_rng_u32(&r) % 60U) - 30);
    fill(&r, b, nb, spread, (int)(test_rng_u32(&r) % 60U) - 30);
    uint32_t no = na + nb - 1U;
    fd_conv_f32(a, na, b, nb, y, w);
    if (!check(1, 0, na, nb, 0U, no, y)) {
      ASSERT_TRUE(0);
      return;
    }
    uint32_t first = test_rng_u32(&r) % no;
    uint32_t n = 1U + test_rng_u32(&r) % (no - first);
    fd_conv_part_f32(a, na, b, nb, first, n, y, w);
    if (!check(1, 0, na, nb, first, n, y)) {
      ASSERT_TRUE(0);
      return;
    }
    int32_t lag0 = (int32_t)(test_rng_u32(&r) % (no + 8U)) - (int32_t)nb - 3;
    n = 1U + test_rng_u32(&r) % (YMAX - 1U);
    fd_corr_f32(a, na, b, nb, lag0, n, y, w);
    if (!check(0, lag0, na, nb, 0U, n, y)) {
      ASSERT_TRUE(0);
      return;
    }
  }
  printf("    worst error / bound %.3f\n", worst_of_bound);
}

/* integers are held exactly: every output is the exact sum rounded once */
void test_fd_corr_f32_integers(void) {
  uint64_t r = 0x1A7U;
  for (unsigned t = 0U; t < 300U; t++) {
    uint32_t na = 1U + test_rng_u32(&r) % NMAX;
    uint32_t nb = 1U + test_rng_u32(&r) % NMAX;
    for (uint32_t i = 0U; i < na; i++) {
      a[i] = (float)((int32_t)(test_rng_u32(&r) % 0x1000001U) - 0x800000);
    }
    for (uint32_t i = 0U; i < nb; i++) {
      b[i] = (float)((int32_t)(test_rng_u32(&r) % 2001U) - 1000);
    }
    fd_conv_f32(a, na, b, nb, y, w);
    for (uint32_t k = 0U; k < na + nb - 1U; k++) {
      uint32_t c = pairs(1, 0, na, nb, k, pi, pj);
      int64_t s = 0;
      for (uint32_t p = 0U; p < c; p++) {
        s += (int64_t)a[pi[p]] * (int64_t)b[pj[p]];
      }
      if (y[k] != (float)s) {
        ASSERT_TRUE(y[k] == (float)s);
        printf("    na %u nb %u out %u: got %.9g want %lld\n", na, nb, k,
               (double)y[k], (long long)s);
        return;
      }
    }
  }
}

/* conv(a, b) == corr(a, reverse(b)) over every lag, bit for bit */
void test_fd_corr_f32_conv_reversed(void) {
  uint64_t r = 0x4EFU;
  for (unsigned t = 0U; t < 300U; t++) {
    uint32_t na = 1U + test_rng_u32(&r) % NMAX;
    uint32_t nb = 1U + test_rng_u32(&r) % NMAX;
    fill(&r, a, na, test_rng_u32(&r) % 30U, 0);
    fill(&r, b, nb, test_rng_u32(&r) % 30U, 0);
    for (uint32_t i = 0U; i < nb; i++) {
      br[i] = b[nb - 1U - i];
    }
    uint32_t no = na + nb - 1U;
    fd_conv_f32(a, na, b, nb, y, w);
    fd_corr_f32(a, na, br, nb, 1 - (int32_t)nb, no, y2, w);
    if (memcmp(y, y2, no * sizeof y[0]) != 0) {
      ASSERT_TRUE(0);
      printf("    na %u nb %u\n", na, nb);
      return;
    }
  }
}

void test_fd_corr_f32_edges(void) {
  /* zeros, subnormals, one value */
  memset(a, 0, sizeof a);
  memset(b, 0, sizeof b);
  fd_conv_f32(a, 5U, b, 3U, y, w);
  for (uint32_t k = 0U; k < 7U; k++) {
    ASSERT_TRUE(y[k] == 0.0f);
  }
  a[0] = 0x1p-149f;
  a[1] = -0x1p-130f;
  b[0] = 0x1p100f;
  fd_conv_f32(a, 2U, b, 1U, y, w);
  ASSERT_TRUE(y[0] == 0x1p-49f && y[1] == -0x1p-30f);
  a[0] = 3.0f;
  b[0] = -0.25f;
  fd_corr_f32(a, 1U, b, 1U, -1, 3U, y, w);
  ASSERT_TRUE(y[0] == 0.0f && y[1] == -0.75f && y[2] == 0.0f);
  /* results past the float range: inf; below it: zero */
  a[0] = 0x1p100f;
  b[0] = 0x1p100f;
  fd_conv_f32(a, 1U, b, 1U, y, w);
  ASSERT_TRUE(isinf(y[0]) && y[0] > 0.0f);
  a[0] = 0x1p-100f;
  b[0] = -0x1p-100f;
  fd_conv_f32(a, 1U, b, 1U, y, w);
  ASSERT_TRUE(y[0] == 0.0f && signbit(y[0]));
  /* a NaN or inf anywhere: every output NaN */
  uint64_t r = 0x7A9U;
  fill(&r, a, 40U, 4U, 0);
  fill(&r, b, 10U, 4U, 0);
  a[17] = NAN;
  fd_conv_f32(a, 40U, b, 10U, y, w);
  for (uint32_t k = 0U; k < 49U; k++) {
    ASSERT_TRUE(isnan(y[k]));
  }
  a[17] = 1.0f;
  b[9] = -INFINITY;
  fd_corr_f32(a, 40U, b, 10U, -9, 49U, y, w);
  for (uint32_t k = 0U; k < 49U; k++) {
    ASSERT_TRUE(isnan(y[k]));
  }
}

/* the shorter's sum of |q| at e + 1 is 2^33 - 40 before and 2^32 + 280 after
 * reconverting (600 values of q = 1.25 round up): only e + 2 keeps
 * (2^31 - 128) sum |q| under 2^63 */
void test_fd_corr_f32_short_exp(void) {
  enum { N = 604 };
  static float la[N], sb[N], yo[1];
  static fm_q31 wk[2 * N];
  for (uint32_t i = 0U; i < N; i++) {
    la[i] = 1.0f - 0x1p-24f;
    sb[i] = i < 3U ? 1.0f - 0x1p-24f : (i == 3U ? 1.0f - 0x1p-23f : 0x1.4p-31f);
  }
  fd_conv_part_f32(la, N, sb, N, N - 1U, 1U, yo, wk);
  f128 ex = 0;
  for (uint32_t i = 0U; i < N; i++) {
    ex += (f128)((double)la[i] * (double)sb[N - 1U - i]);
  }
  ASSERT_TRUE(fabs((double)((f128)yo[0] - ex)) <= 0x1p-20);
}
