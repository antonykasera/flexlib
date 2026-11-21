/**
 * @file test_fd_f32.c
 * @brief Host tests for the float sums (fd_vec.h) and FFTs (fd_fft.h)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fft.h"
#include "fd_vec.h"
#include "ref_dsp.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

__extension__ typedef __float128 f128;

#define NMAX 1024U

static float a[NMAX], b[NMAX];

static float unif(uint64_t *r, int k) {
  return ldexpf((float)(int32_t)test_rng_u32(r) * 0x1p-31f, k);
}

static double ulp(float v) {
  float m = fabsf(v);
  return (double)(nextafterf(m, INFINITY) - m);
}

static int within(float got, f128 exact, double scale) {
  double err = fabs((double)((f128)got - exact));
  return err <= 0.5 * ulp(got) + 0x1p-32 * scale;
}

void test_fd_f32_sums(void) {
  uint64_t r = 0x5E3U;
  unsigned long bad = 0U;
  for (unsigned t = 0U; t < 4000U; t++) {
    uint32_t n = 1U + test_rng_u32(&r) % 100U;
    int spread = (int)(test_rng_u32(&r) % 60U);
    for (uint32_t i = 0U; i < n; i++) {
      a[i] = unif(&r, (int)(test_rng_u32(&r) % (uint32_t)(spread + 1)) - 30);
      b[i] = unif(&r, (int)(test_rng_u32(&r) % (uint32_t)(spread + 1)) - 30);
    }
    if (t % 5U == 0U) {
      a[n - 1U] = -a[0];
      b[n - 1U] = b[0];
    }
    f128 d = 0, p = 0, s = 0;
    double sd = 0.0, sp = 0.0, ss = 0.0;
    for (uint32_t i = 0U; i < n; i++) {
      d += (f128)((double)a[i] * (double)b[i]);
      p += (f128)((double)a[i] * (double)a[i]);
      s += (f128)a[i];
      sd += fabs((double)a[i] * (double)b[i]);
      sp += (double)a[i] * (double)a[i];
      ss += fabs((double)a[i]);
    }
    bad += !within(fd_dot_f32(a, b, n), d, sd);
    bad += !within(fd_power_f32(a, n), p, sp);
    bad += !within(fd_sum_f32(a, n), s, ss);
    bad += !within(fd_mean_f32(a, n), s / n, ss / n);
    float rms = fd_rms_f32(a, n);
    double want = sqrt((double)(p / n));
    bad += !(fabs((double)rms - want) <= ulp(rms) + 0x1p-32 * sqrt(sp / n));
  }
  ASSERT_EQ(bad, 0);
}

void test_fd_f32_sums_edges(void) {
  ASSERT_TRUE(fd_sum_f32(a, 0U) == 0.0f);
  a[0] = 1.0f;
  a[1] = 0x1p-40f;
  a[2] = -1.0f;
  ASSERT_TRUE(fd_sum_f32(a, 3U) == 0x1p-40f);
  a[0] = 0x1p-149f;
  a[1] = 0x1p-149f;
  ASSERT_TRUE(fd_sum_f32(a, 2U) == 0x1p-148f);
  a[0] = 3.0e38f;
  a[1] = 3.0e38f;
  ASSERT_TRUE(isinf(fd_sum_f32(a, 2U)));
  ASSERT_TRUE(fd_mean_f32(a, 2U) == 3.0e38f);
  a[1] = NAN;
  ASSERT_TRUE(isnan(fd_dot_f32(a, a, 2U)));
  a[1] = INFINITY;
  ASSERT_TRUE(isnan(fd_rms_f32(a, 2U)));
  for (uint32_t i = 0U; i < 7U; i++) {
    a[i] = 1.0f;
  }
  ASSERT_TRUE(fd_mean_f32(a, 7U) == 1.0f);
  for (uint32_t i = 0U; i < NMAX; i++) { /* past 2^62 */
    b[i] = 1.9999999f;
  }
  f128 p = (f128)NMAX * ((f128)b[0] * (f128)b[0]);
  ASSERT_TRUE(within(fd_power_f32(b, NMAX), p, (double)p));
  ASSERT_TRUE(within(fd_sum_f32(b, NMAX), (f128)NMAX * (f128)b[0], NMAX * 2.0));
  a[0] = 2.0f;
  ASSERT_TRUE(fd_mean_f32(a, 7U) == (float)(8.0 / 7.0));
}

static float sx[2U * NMAX];
static fd_cq31 w[NMAX];
static double re[NMAX], im[NMAX];

static double snr_cfft(uint64_t *r, uint32_t n, int k, int inverse) {
  for (uint32_t i = 0U; i < n; i++) {
    sx[2U * i] = unif(r, k);
    sx[2U * i + 1U] = unif(r, k);
    re[i] = (double)sx[2U * i];
    im[i] = (double)sx[2U * i + 1U];
  }
  ref_fft(re, im, n, inverse);
  if (inverse) {
    fd_icfft_f32((fd_cf32 *)(void *)sx, n, w);
  } else {
    fd_cfft_f32((fd_cf32 *)(void *)sx, n, w);
  }
  double sig = 0.0, err = 0.0, scale = inverse ? 1.0 / n : 1.0;
  for (uint32_t i = 0U; i < n; i++) {
    double er = (double)sx[2U * i] - re[i] * scale;
    double ei = (double)sx[2U * i + 1U] - im[i] * scale;
    sig += (re[i] * re[i] + im[i] * im[i]) * scale * scale;
    err += er * er + ei * ei;
  }
  return 10.0 * log10(sig / err);
}

void test_fd_f32_fft(void) {
  uint64_t r = 0xFF7U;
  static const uint32_t sizes[] = {2U, 4U, 64U, 256U, 1024U};
  for (unsigned s = 0U; s < 5U; s++) {
    for (int k = -130; k <= 120; k += 50) {
      ASSERT_TRUE(snr_cfft(&r, sizes[s], k, 0) > 130.0);
      if (k > -130) { /* subnormal outputs */
        ASSERT_TRUE(snr_cfft(&r, sizes[s], k, 1) > 130.0);
      }
    }
  }
  memset(sx, 0, sizeof sx);
  fd_cfft_f32((fd_cf32 *)(void *)sx, 64U, w);
  for (uint32_t i = 0U; i < 128U; i++) {
    ASSERT_TRUE(sx[i] == 0.0f);
  }
  sx[5] = NAN;
  fd_cfft_f32((fd_cf32 *)(void *)sx, 64U, w);
  ASSERT_TRUE(isnan(sx[0]) && isnan(sx[127]));
  for (uint32_t i = 0U; i < 64U; i++) {
    sx[2U * i] = 3.0e37f;
    sx[2U * i + 1U] = 0.0f;
  }
  fd_cfft_f32((fd_cf32 *)(void *)sx, 64U, w);
  ASSERT_TRUE(isinf(sx[0]) && sx[2] == 0.0f);
}

void test_fd_f32_rfft(void) {
  uint64_t r = 0x4FF7U;
  static fm_q31 wr[NMAX];
  for (unsigned rep = 0U; rep < 20U; rep++) {
    uint32_t n = 4U << (rep % 9U);
    int k = (int)(rep * 13U % 200U) - 100;
    for (uint32_t i = 0U; i < n; i++) {
      sx[i] = unif(&r, k);
      re[i] = (double)sx[i];
      im[i] = 0.0;
    }
    memcpy(a, sx, n * sizeof a[0]);
    double xr[NMAX], xi[NMAX], m = 0.0;
    ref_dft(re, im, n, xr, xi, 0);
    fd_rfft_f32(sx, n, wr);
    for (uint32_t q = 0U; q <= n / 2U; q++) {
      m = fmax(m, hypot(xr[q], xi[q]));
    }
    ASSERT_NEAR(sx[0], xr[0], 1e-6 * m);
    ASSERT_NEAR(sx[1], xr[n / 2U], 1e-6 * m);
    for (uint32_t q = 1U; q < n / 2U; q++) {
      ASSERT_NEAR(sx[2U * q], xr[q], 1e-6 * m);
      ASSERT_NEAR(sx[2U * q + 1U], xi[q], 1e-6 * m);
    }
    fd_irfft_f32(sx, n, wr);
    for (uint32_t i = 0U; i < n; i++) {
      ASSERT_NEAR(sx[i], a[i], 1e-6 * ldexp(1.0, k));
    }
  }
}
