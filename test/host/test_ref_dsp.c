/**
 * @file test_ref_dsp.c
 * @brief Self-tests for the DSP references and the SNR statistics
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ref_dsp.h"
#include "test_stats.h"
#include <math.h>

#define N 64U

static double rnd(uint64_t *s) {
  return (double)(int32_t)test_rng_u32(s) / 2147483648.0;
}

void test_snr_stats(void) {
  SnrStats s;
  snr_init(&s, "self");
  snr_add(&s, 1.0, 1.0);
  ASSERT_TRUE(snr_db(&s) == HUGE_VAL);
  snr_add(&s, 0.1, 0.0); /* signal power 1, noise 0.01: 20 dB */
  ASSERT_NEAR(snr_db(&s), 20.0, 1e-9);
  ASSERT_NEAR(s.max_abs, 0.1, 1e-12);

  ErrStats e;
  const double got[3] = {1.0, 2.5, 2.0};
  const double ref[3] = {1.0, 2.0, 3.0};
  stats_init(&e, "block", "lsb");
  stats_add_block(&e, got, ref, 3U, 0.5);
  ASSERT_NEAR(e.max_abs, 2.0, 0.0);
  ASSERT_NEAR(e.worst_in, 2.0, 0.0);
}

void test_ref_fir_conv_corr(void) {
  uint64_t s = 11U;
  double h[7], x[N], y[N], c[N + 6U], r[N + 6U];
  for (unsigned i = 0U; i < 7U; i++) {
    h[i] = rnd(&s);
  }
  for (unsigned i = 0U; i < N; i++) {
    x[i] = i == 0U ? 1.0 : 0.0;
  }
  ref_fir(h, 7U, x, N, y); /* impulse response is h */
  for (unsigned i = 0U; i < N; i++) {
    ASSERT_NEAR(y[i], i < 7U ? h[i] : 0.0, 0.0);
  }

  for (unsigned i = 0U; i < N; i++) {
    x[i] = rnd(&s);
  }
  ref_fir(h, 7U, x, N, y);
  ref_conv(x, N, h, 7U, c); /* the FIR is the head of the convolution */
  for (unsigned i = 0U; i < N; i++) {
    ASSERT_NEAR(y[i], c[i], 1e-15);
  }

  /* correlating with the reversed h is convolving with h */
  double hr[7];
  for (unsigned i = 0U; i < 7U; i++) {
    hr[i] = h[6U - i];
  }
  ref_corr(x, N, hr, 7U, r);
  for (unsigned m = 0U; m < N + 6U; m++) {
    ASSERT_NEAR(r[m], c[m], 1e-15);
  }

  /* autocorrelation peaks at lag 0 with the energy */
  double ac[2U * N - 1U], energy = 0.0;
  for (unsigned i = 0U; i < N; i++) {
    energy += x[i] * x[i];
  }
  ref_corr(x, N, x, N, ac);
  ASSERT_NEAR(ac[N - 1U], energy, 1e-14);
  for (unsigned m = 0U; m < 2U * N - 1U; m++) {
    ASSERT_TRUE(fabs(ac[m]) <= ac[N - 1U]);
  }
}

void test_ref_biquad(void) {
  uint64_t s = 12U;
  double x[N], y[N], f[N];
  for (unsigned i = 0U; i < N; i++) {
    x[i] = rnd(&s);
  }
  /* no feedback: a FIR of b */
  const double fir_only[5] = {0.3, -0.2, 0.1, 0.0, 0.0};
  ref_biquad(fir_only, 1U, x, N, y);
  ref_fir(fir_only, 3U, x, N, f);
  for (unsigned i = 0U; i < N; i++) {
    ASSERT_NEAR(y[i], f[i], 1e-15);
  }
  /* one pole at 0.5 (CMSIS sign: +a1 y[-1]): impulse response 0.5^k */
  const double pole[5] = {1.0, 0.0, 0.0, 0.5, 0.0};
  for (unsigned i = 0U; i < N; i++) {
    x[i] = i == 0U ? 1.0 : 0.0;
  }
  ref_biquad(pole, 1U, x, N, y);
  for (unsigned i = 0U; i < 20U; i++) {
    ASSERT_NEAR(y[i], ldexp(1.0, -(int)i), 0.0);
  }
  /* two sections in cascade: 0.5^k convolved with itself = (k + 1) 0.5^k */
  const double two[10] = {1.0, 0.0, 0.0, 0.5, 0.0, 1.0, 0.0, 0.0, 0.5, 0.0};
  ref_biquad(two, 2U, x, N, y);
  for (unsigned i = 0U; i < 20U; i++) {
    ASSERT_NEAR(y[i], (double)(i + 1U) * ldexp(1.0, -(int)i), 1e-15);
  }
}

void test_ref_dft(void) {
  uint64_t s = 13U;
  double re[N], im[N], fr[N], fi[N], br[N], bi[N];
  for (unsigned i = 0U; i < N; i++) {
    re[i] = i == 0U ? 1.0 : 0.0;
    im[i] = 0.0;
  }
  ref_dft(re, im, N, fr, fi, 0); /* impulse: flat spectrum */
  for (unsigned k = 0U; k < N; k++) {
    ASSERT_NEAR(fr[k], 1.0, 0.0);
    ASSERT_NEAR(fi[k], 0.0, 0.0);
  }
  /* cos at bin 5: N/2 in bins 5 and N - 5 */
  for (unsigned i = 0U; i < N; i++) {
    re[i] = cos(6.283185307179586 * 5.0 * i / N);
  }
  ref_dft(re, NULL, N, fr, fi, 0);
  for (unsigned k = 0U; k < N; k++) {
    double want = (k == 5U || k == N - 5U) ? N / 2.0 : 0.0;
    ASSERT_NEAR(fr[k], want, 1e-12);
    ASSERT_NEAR(fi[k], 0.0, 1e-12);
  }
  double r5, i5;
  ref_dft_bin(re, N, 5.0 / N, &r5, &i5);
  ASSERT_NEAR(r5, fr[5], 1e-12);
  ASSERT_NEAR(i5, fi[5], 1e-12);

  /* forward then inverse gives N x */
  for (unsigned i = 0U; i < N; i++) {
    re[i] = rnd(&s);
    im[i] = rnd(&s);
  }
  ref_dft(re, im, N, fr, fi, 0);
  ref_dft(fr, fi, N, br, bi, 1);
  for (unsigned i = 0U; i < N; i++) {
    ASSERT_NEAR(br[i] / N, re[i], 1e-15);
    ASSERT_NEAR(bi[i] / N, im[i], 1e-15);
  }

  /* the FFT against the DFT, both directions, every size up to N */
  for (unsigned n = 1U; n <= N; n *= 2U) {
    for (int inv = 0; inv <= 1; inv++) {
      for (unsigned i = 0U; i < n; i++) {
        re[i] = br[i] = rnd(&s);
        im[i] = bi[i] = rnd(&s);
      }
      ref_dft(re, im, n, fr, fi, inv);
      ref_fft(br, bi, n, inv);
      for (unsigned k = 0U; k < n; k++) {
        ASSERT_NEAR(br[k], fr[k], 1e-14);
        ASSERT_NEAR(bi[k], fi[k], 1e-14);
      }
    }
  }
}

void test_ref_window_resample(void) {
  const double hann[2] = {0.5, 0.5};
  double w[N + 1U];
  ref_window_cosine(hann, 2U, N + 1U, 1, w); /* symmetric, odd length */
  ASSERT_NEAR(w[0], 0.0, 0.0);
  ASSERT_NEAR(w[N], 0.0, 1e-18);
  ASSERT_NEAR(w[N / 2U], 1.0, 0.0);
  for (unsigned k = 0U; k <= N; k++) {
    ASSERT_NEAR(w[k], w[N - k], 1e-15);
  }
  ref_window_cosine(hann, 2U, N, 0, w); /* periodic: peak at N/2 */
  ASSERT_NEAR(w[N / 2U], 1.0, 0.0);
  ASSERT_NEAR(w[1], w[N - 1U], 1e-15);

  uint64_t s = 14U;
  double h[5], x[N], y[3U * N], f[N];
  for (unsigned i = 0U; i < 5U; i++) {
    h[i] = rnd(&s);
  }
  for (unsigned i = 0U; i < N; i++) {
    x[i] = rnd(&s);
  }
  ref_resample(h, 5U, 1U, 1U, x, N, y); /* 1/1 is the FIR */
  ref_fir(h, 5U, x, N, f);
  for (unsigned i = 0U; i < N; i++) {
    ASSERT_NEAR(y[i], f[i], 0.0);
  }
  ref_resample(h, 5U, 1U, 2U, x, N, y); /* /2 keeps the even outputs */
  for (unsigned i = 0U; i < N / 2U; i++) {
    ASSERT_NEAR(y[i], f[2U * i], 0.0);
  }
  /* x3 with a hold filter {1, 1, 1} repeats each sample */
  const double hold[3] = {1.0, 1.0, 1.0};
  ref_resample(hold, 3U, 3U, 1U, x, N, y);
  for (unsigned i = 0U; i < 3U * N; i++) {
    ASSERT_NEAR(y[i], x[i / 3U], 0.0);
  }
}
