/**
 * @file test_fd_fir_f32.c
 * @brief Host tests for fd_fir_f32 against exact sums
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fir.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

__extension__ typedef __float128 f128;

#define TMAX 64U
#define NMAX 2048U
#define CMAX 128U

static float h[TMAX], x[NMAX], y[NMAX];
static fm_q31 hq[TMAX], st[TMAX - 1U + CMAX];
static float xs[TMAX - 1U + CMAX];

static double worst_of_bound, worst_ulp;

static uint32_t rnd(uint64_t *r) { return test_rng_u32(r); }

static float unif(uint64_t *r, int k) {
  return ldexpf((float)(int32_t)rnd(r) * 0x1p-31f, k);
}

/* calls of c, each a whole chunk; every output against the header's bound */
static void check(uint32_t t, uint32_t n, uint32_t c) {
  FD_FirF32 f;
  fd_fir_init_f32(&f, h, hq, t, xs, st, FD_FIR_STATE_LEN(t, c));
  double sh = 0.0;
  for (uint32_t k = 0U; k < t; k++) {
    sh += fabs((double)h[k]);
  }
  for (uint32_t i = 0U; i < n; i += c) {
    uint32_t m = n - i < c ? n - i : c;
    fd_fir_f32(&f, x + i, y + i, m);
    for (uint32_t j = i; j < i + m; j++) {
      double xm = 0x1p-126;
      uint32_t lo = j > 2U * (t + c) ? j - 2U * (t + c) : 0U;
      for (uint32_t q = lo; q < j + c && q < n; q++) {
        xm = fmax(xm, fabs((double)x[q]));
      }
      if (j == i + m - 1U && !(ldexp(1.0, f.es) <= fmax(2.0 * xm, 0x1p-125))) {
        ASSERT_TRUE(ldexp(1.0, f.es) <= fmax(2.0 * xm, 0x1p-125));
        printf("    t %u c %u at %u: E %d, X %g\n", t, c, j, f.es, xm);
        return;
      }
      f128 s = 0;
      double sx = 0.0;
      for (uint32_t k = 0U; k < t && k <= j; k++) {
        s += (f128)((double)h[k] * (double)x[j - k]);
        sx += fabs((double)x[j - k]);
      }
      float want = (float)s;
      double err = fabs((double)((f128)y[j] - s));
      float a = fabsf(y[j]);
      double ulp = (double)(nextafterf(a, INFINITY) - a);
      double bound = 0.5 * ulp + 0x1p-31 * sh * (xm + sx);
      if (!(err <= bound)) {
        ASSERT_TRUE(err <= bound);
        printf("    t %u c %u at %u: got %.9g want %.9g (E %d)\n", t, c, j,
               (double)y[j], (double)want, f.es);
        return;
      }
      double r = bound > 0.0 ? err / bound : 0.0;
      worst_of_bound = r > worst_of_bound ? r : worst_of_bound;
      float wa = fabsf(want);
      double u = want != 0.0f ? err / (double)(nextafterf(wa, INFINITY) - wa)
                              : 0.0;
      worst_ulp = u > worst_ulp ? u : worst_ulp;
    }
  }
}

static void lowpass(uint32_t t) {
  ASSERT_EQ(fd_fir_design(h, t, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING),
            FD_OK);
}

void test_fd_fir_f32_bound(void) {
  uint64_t r = 0xF1F32U;
  static const uint32_t taps[] = {1U, 2U, 5U, 32U, 63U};
  static const uint32_t chunks[] = {1U, 3U, 64U, 128U};
  for (unsigned kind = 0U; kind < 7U; kind++) {
    for (unsigned ti = 0U; ti < 5U; ti++) {
      uint32_t t = taps[ti];
      if (kind % 2U == 0U && t >= 5U) {
        lowpass(t);
      } else {
        for (uint32_t k = 0U; k < t; k++) {
          h[k] = unif(&r, (int)(rnd(&r) % 9U) - 4);
        }
      }
      for (uint32_t i = 0U; i < NMAX; i++) {
        int k = 0;
        switch (kind) {
        case 0: k = 0; break;
        case 1: k = (i / 300U) % 2U == 0U ? 0 : -20; break;
        case 2: k = (int)(rnd(&r) % 40U) - 20; break;
        case 3: k = (i / 50U) % 2U == 0U ? -10 : 5; break;
        case 4: k = -140 + (int)(rnd(&r) % 20U); break;
        case 5: k = 100 + (int)(rnd(&r) % 20U); break;
        default: k = i % 97U == 0U ? 30 : 0; break;
        }
        x[i] = unif(&r, k);
      }
      for (unsigned ci = 0U; ci < 4U; ci++) {
        check(t, NMAX, chunks[ci]);
      }
    }
    printf("  fd_fir_f32, signal %u: worst error %.3f of its bound, %.3g ulp\n",
           kind, worst_of_bound, worst_ulp);
    worst_of_bound = 0.0;
    worst_ulp = 0.0;
  }
}

void test_fd_fir_f32_coefs(void) {
  uint64_t r = 0xC0EF5U;
  for (uint32_t i = 0U; i < 512U; i++) {
    x[i] = unif(&r, 0);
  }
  for (uint32_t k = 0U; k < 32U; k++) {
    h[k] = 0.75f;
  }
  check(32U, 512U, 64U);
  memset(h, 0, sizeof h);
  check(32U, 512U, 64U);
  for (uint32_t i = 0U; i < 512U; i++) {
    ASSERT_TRUE(y[i] == 0.0f);
  }
  h[0] = 1.0f;
  check(1U, 512U, 64U);
  ASSERT_TRUE(memcmp(x, y, 512U * sizeof x[0]) == 0);
  h[0] = 0x1p-140f;
  h[1] = -0x1p-149f;
  check(2U, 512U, 64U);
  h[0] = 3.0e38f;
  x[5] = 2.0f;
  FD_FirF32 f;
  fd_fir_init_f32(&f, h, hq, 1U, xs, st, FD_FIR_STATE_LEN(1U, 64U));
  float out = fd_fir_sample_f32(&f, 2.0f);
  ASSERT_TRUE(isinf(out) && out > 0.0f);
}

void test_fd_fir_f32_nonfinite(void) {
  uint64_t r = 0xBAD5U;
  for (uint32_t k = 0U; k < 8U; k++) {
    h[k] = unif(&r, 0);
  }
  for (uint32_t i = 0U; i < 300U; i++) {
    x[i] = unif(&r, 0);
  }
  static float x2[300], y2[300];
  memcpy(x2, x, sizeof x2);
  x2[10] = NAN;
  x2[100] = INFINITY;
  x2[103] = -INFINITY;
  FD_FirF32 f;
  fd_fir_init_f32(&f, h, hq, 8U, xs, st, FD_FIR_STATE_LEN(8U, 64U));
  fd_fir_f32(&f, x2, y2, 300U);
  for (uint32_t i = 0U; i < 300U; i++) {
    bool spoilt = (i >= 10U && i < 18U) || (i >= 100U && i < 111U);
    ASSERT_TRUE((isnan(y2[i]) != 0) == spoilt);
  }
  x[10] = 0.0f;
  x[100] = 0.0f;
  x[103] = 0.0f;
  fd_fir_init_f32(&f, h, hq, 8U, xs, st, FD_FIR_STATE_LEN(8U, 64U));
  fd_fir_f32(&f, x, y, 300U);
  for (uint32_t i = 0U; i < 300U; i++) {
    if (!isnan(y2[i])) {
      ASSERT_NEAR(y2[i], y[i], 1e-6);
    }
  }
}

void test_fd_fir_f32_state(void) {
  uint64_t r = 0x57A7EU;
  lowpass(31U);
  for (uint32_t i = 0U; i < 400U; i++) {
    x[i] = unif(&r, (int)(i / 100U) * 3 - 6);
  }
  FD_FirF32 f;
  fd_fir_init_f32(&f, h, hq, 31U, xs, st, FD_FIR_STATE_LEN(31U, 50U));
  fd_fir_f32(&f, x, y, 400U);
  static float z[400];
  memcpy(z, x, sizeof z);
  fd_fir_reset_f32(&f);
  fd_fir_f32(&f, z, z, 400U);
  ASSERT_TRUE(memcmp(z, y, sizeof z) == 0);
  worst_of_bound = 0.0;
  check(31U, 400U, 1U);
  ASSERT_TRUE(worst_of_bound <= 1.0);
}

void test_fd_fir_f32_coef_exp(void) {
  static const float h1[] = {0.75f}, h3[] = {0.75f, 0.75f, 0.5f};
  static float h32[32];
  for (uint32_t k = 0U; k < 32U; k++) {
    h32[k] = 0.75f;
  }
  FD_FirF32 f;
  fd_fir_init_f32(&f, h1, hq, 1U, xs, st, FD_FIR_STATE_LEN(1U, 8U));
  ASSERT_EQ(f.eh, 0);
  fd_fir_init_f32(&f, h3, hq, 3U, xs, st, FD_FIR_STATE_LEN(3U, 8U));
  ASSERT_EQ(f.eh, 1); /* sum exactly 2 at e = 0 */
  fd_fir_init_f32(&f, h32, hq, 32U, xs, st, FD_FIR_STATE_LEN(32U, 8U));
  ASSERT_EQ(f.eh, 4);
}

void test_fd_fir_f32_exp_schedule(void) {
  static const float h4[] = {0.25f, 0.25f, 0.25f, 0.25f};
  static const float loud[3] = {1.0f, 0.5f, -0.75f};
  static const float quiet[3] = {0x1p-10f, -0x1p-11f, 0x1p-12f};
  float out[3];
  FD_FirF32 f;
  fd_fir_init_f32(&f, h4, hq, 4U, xs, st, FD_FIR_STATE_LEN(4U, 3U));
  static const int32_t want1[] = {1, 1, -9, -9};
  for (unsigned i = 0U; i < 4U; i++) {
    fd_fir_f32(&f, i == 0U ? loud : quiet, out, 3U);
    ASSERT_EQ(f.es, want1[i]);
  }
  /* an equal chunk restarts the count */
  fd_fir_reset_f32(&f);
  static const int32_t want2[] = {1, 1, 1, -9};
  for (unsigned i = 0U; i < 4U; i++) {
    fd_fir_f32(&f, i < 2U ? loud : quiet, out, 3U);
    ASSERT_EQ(f.es, want2[i]);
  }
}
