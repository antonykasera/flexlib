/**
 * @file test_fd_tone.c
 * @brief Host tests for fd_tone.h and fd_noise.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_noise.h"
#include "fd_tone.h"
#include "fm_trig.h"
#include "ref_dsp.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

__extension__ typedef __int128 i128;

static uint64_t rng = 31337U;
static const double two_pi = 6.283185307179586476925286766559;

/* the state wraps past 2^31 (the contract keeps inputs clear of it) */
static int32_t wrap32(i128 v) { return (int32_t)(uint32_t)(uint64_t)v; }

/* the recursion in 128 bits */
static void ref_goertzel(int32_t c, const int32_t *v, uint32_t n, int32_t *s1,
                         int32_t *s2) {
  for (uint32_t i = 0U; i < n; i++) {
    i128 s0 = (i128)v[i] + (((i128)c * *s1 + ((i128)1 << 29)) >> 30) - *s2;
    *s2 = *s1;
    *s1 = wrap32(s0);
  }
}

void test_fd_goertzel(void) {
  static fm_q15 x15[600];
  static fm_q31 x31[600];
  static int32_t v[600];
  for (int rep = 0; rep < 40; rep++) {
    uint32_t freq = test_rng_u32(&rng);
    uint32_t sh15 = (uint32_t)rep % 5U, sh31 = 6U + (uint32_t)rep % 4U;
    for (uint32_t i = 0U; i < 600U; i++) {
      x15[i] = (fm_q15)(test_rng_u32(&rng) >> 18); /* quarter scale */
      x31[i] = (fm_q31)test_rng_u32(&rng) >> 2;
    }
    FD_Goertzel g15, g31;
    fd_goertzel_init(&g15, freq, sh15);
    fd_goertzel_init(&g31, freq, sh31);
    for (uint32_t i = 0U; i < 600U; i += 100U) { /* in pieces */
      fd_goertzel_q15(&g15, x15 + i, 100U);
      fd_goertzel_q31(&g31, x31 + i, 100U);
    }
    int32_t a1 = 0, a2 = 0, b1 = 0, b2 = 0;
    for (uint32_t i = 0U; i < 600U; i++) {
      v[i] = x15[i] * (1 << sh15);
    }
    ref_goertzel(fm_cos_q31(freq), v, 600U, &a1, &a2);
    for (uint32_t i = 0U; i < 600U; i++) {
      v[i] = (int32_t)(((int64_t)x31[i] + (1LL << (sh31 - 1U))) >> sh31);
    }
    ref_goertzel(fm_cos_q31(freq), v, 600U, &b1, &b2);
    ASSERT_TRUE(g15.s1 == a1 && g15.s2 == a2);
    ASSERT_TRUE(g31.s1 == b1 && g31.s2 == b2);
    /* power from the state, exactly rounded */
    i128 c = g31.c, s1 = g31.s1, s2 = g31.s2;
    i128 p = s1 * s1 + s2 * s2 - ((c * s1 * s2 + ((i128)1 << 29)) >> 30);
    ASSERT_TRUE(fd_goertzel_power(&g31) == (uint64_t)(p < 0 ? 0 : p));
  }
  /* against the DFT: a tone near bin 18 of 205 (DTMF style) */
  const uint32_t n = 205U;
  double xs[205];
  for (uint32_t i = 0U; i < n; i++) {
    double t = 0.5 * sin(two_pi * 18.3 / n * i + 0.3) +
               0.2 * sin(two_pi * 50.0 / n * i);
    x15[i] = (fm_q15)lround(t * 32768.0);
    xs[i] = x15[i];
  }
  uint32_t freq = (uint32_t)llround(18.0 / n * 4294967296.0);
  FD_Goertzel g;
  fd_goertzel_init(&g, freq, 4U);
  fd_goertzel_q15(&g, x15, n);
  int64_t re, im;
  fd_goertzel_dft(&g, &re, &im);
  /* sum x[k] e^(jw(N-1-k)) = e^(jw(N-1)) DFT */
  double w = two_pi * (double)freq / 4294967296.0, dr, di;
  ref_dft_bin(xs, n, (double)freq / 4294967296.0, &dr, &di);
  double rr = (dr * cos(w * (n - 1)) - di * sin(w * (n - 1))) * 16.0;
  double ri = (dr * sin(w * (n - 1)) + di * cos(w * (n - 1))) * 16.0;
  double mag = sqrt(rr * rr + ri * ri);
  ASSERT_TRUE(fabs((double)re - rr) < 1e-4 * mag);
  ASSERT_TRUE(fabs((double)im - ri) < 1e-4 * mag);
  ASSERT_TRUE(fabs((double)fd_goertzel_power(&g) - mag * mag) <
              3e-4 * mag * mag);
}

void test_fd_nco(void) {
  static fm_q31 s[300], c[300];
  static fm_q15 s15[300], c15[300];
  static fd_cq31 z[300];
  static fd_cq15 z15[300];
  FD_Nco a, b, d, e, f;
  fd_nco_init(&a, 0x12345678U, 0x0ABCDEF1U, -12345);
  b = a;
  d = a;
  e = a;
  f = a;
  fd_nco_q31(&a, s, c, 150U);
  fd_nco_q31(&a, s + 150, c + 150, 150U);
  fd_nco_q15(&b, s15, NULL, 300U);
  fd_nco_q15(&d, NULL, c15, 300U);
  fd_nco_cplx_q31(&e, z, 300U);
  fd_nco_cplx_q15(&f, z15, 300U);
  uint32_t ph = 0x12345678U, inc = 0x0ABCDEF1U;
  for (uint32_t i = 0U; i < 300U; i++) {
    ASSERT_EQ(s[i], fm_sin_q31(ph));
    ASSERT_EQ(c[i], fm_cos_q31(ph));
    ASSERT_TRUE(z[i].re == c[i] && z[i].im == s[i]);
    int64_t r15 = ((int64_t)s[i] + 0x8000) >> 16;
    ASSERT_EQ(s15[i], r15 > 32767 ? 32767 : r15);
    int64_t q15 = ((int64_t)c[i] + 0x8000) >> 16;
    ASSERT_EQ(c15[i], q15 > 32767 ? 32767 : q15);
    ASSERT_TRUE(z15[i].re == c15[i] && z15[i].im == s15[i]);
    ph += inc;
    inc += (uint32_t)-12345;
  }
  ASSERT_EQ(a.phase, ph);
  ASSERT_EQ(a.inc, inc);
}

void test_fd_osc(void) {
  FD_Osc o;
  const uint32_t ph0 = 0x40000000U, inc = 0x01234567U;
  fd_osc_init(&o, ph0, inc);
  ErrStats e;
  stats_init(&e, "fd_osc_q31 vs exact, 10^6 samples", "lsb");
  double amp_lo = 2.0, amp_hi = 0.0;
  static fm_q31 s[1000], c[1000];
  for (uint32_t blk = 0U; blk < 1000U; blk++) {
    fd_osc_q31(&o, s, c, 1000U);
    for (uint32_t i = 0U; i < 1000U; i++) {
      double k = (double)(blk * 1000U + i);
      double t = (ph0 + k * (double)inc) / 4294967296.0 * two_pi;
      stats_add(&e, k, s[i] - sin(t) * 2147483648.0);
      double a = hypot((double)s[i], (double)c[i]) / 2147483648.0;
      amp_lo = a < amp_lo ? a : amp_lo;
      amp_hi = a > amp_hi ? a : amp_hi;
    }
  }
  stats_print(&e);
  printf("  fd_osc amplitude %.9f .. %.9f\n", amp_lo, amp_hi);
  ASSERT_TRUE(amp_lo > 1.0 - 1e-6 && amp_hi < 1.0 + 1e-6);
  ASSERT_TRUE(e.max_abs < 1e-4 * 2147483648.0); /* phase drift */
  fm_q15 s15[4];
  fd_osc_init(&o, 0x40000000U, 0U); /* sin 90 degrees, held */
  fd_osc_q15(&o, s15, NULL, 4U);
  ASSERT_EQ(s15[3], 32767);
}

void test_fd_noise(void) {
  FU_Xoshiro128 r;
  fu_xoshiro128_seed(&r, 2026U);
  static fm_q15 y[200000];
  static fm_q31 Y[200000];
  /* Gaussian, sigma 0.2: mean, deviation, kurtosis, the 3-sigma tail */
  fd_noise_gauss_q15(&r, 6554, y, 200000U);
  fd_noise_gauss_q31(&r, 429496730, Y, 200000U);
  double m = 0, v = 0, k4 = 0, M = 0, V = 0, K4 = 0, tail = 0;
  for (uint32_t i = 0U; i < 200000U; i++) {
    double a = y[i] / 6554.0, b = Y[i] / 429496730.0;
    m += a;
    v += a * a;
    k4 += a * a * a * a;
    M += b;
    V += b * b;
    K4 += b * b * b * b;
    tail += fabs(b) > 3.0 ? 1.0 : 0.0;
  }
  m /= 2e5, v /= 2e5, k4 /= 2e5, M /= 2e5, V /= 2e5, K4 /= 2e5, tail /= 2e5;
  printf("  gauss q15: mean %.4f var %.4f kurt %.3f; q31: mean %.4f var %.4f "
         "kurt %.3f, |x| > 3: %.5f\n",
         m, v, k4 / (v * v), M, V, K4 / (V * V), tail);
  ASSERT_TRUE(fabs(m) < 0.01 && fabs(M) < 0.01);
  ASSERT_TRUE(fabs(v - 1.0) < 0.01 && fabs(V - 1.0) < 0.01);
  ASSERT_TRUE(fabs(k4 / (v * v) - 3.0) < 0.06 &&
              fabs(K4 / (V * V) - 3.0) < 0.06);
  ASSERT_TRUE(fabs(tail - 0.0027) < 0.0006);

  /* uniform: every q15 value about equally often */
  static uint32_t hist[65536];
  memset(hist, 0, sizeof hist);
  for (int rep = 0; rep < 20; rep++) {
    fd_noise_uniform_q15(&r, y, 200000U);
    for (uint32_t i = 0U; i < 200000U; i++) {
      hist[(uint16_t)y[i]]++;
    }
  }
  double chi = 0.0, expect = 4e6 / 65536.0;
  for (uint32_t i = 0U; i < 65536U; i++) {
    chi += (hist[i] - expect) * (hist[i] - expect) / expect;
  }
  ASSERT_TRUE(fabs(chi - 65535.0) < 6.0 * sqrt(2.0 * 65535.0));
  fd_noise_uniform_q31(&r, Y, 7U);
  fd_noise_uniform_q15(&r, y, 7U);

  /* TPDF: range, zero mean, variance 2 (4^b - 1) / 12 */
  for (uint32_t bits = 0U; bits <= 15U; bits += 5U) {
    fd_noise_tpdf_q15(&r, bits, y, 200000U);
    double s = 0, q = 0;
    int32_t lim = (1 << bits) - 1;
    for (uint32_t i = 0U; i < 200000U; i++) {
      ASSERT_TRUE(y[i] >= -lim && y[i] <= lim);
      s += y[i];
      q += (double)y[i] * y[i];
    }
    double want = 2.0 * (ldexp(1.0, (int)(2U * bits)) - 1.0) / 12.0;
    ASSERT_TRUE(fabs(s / 2e5) < 0.01 * (lim + 1));
    ASSERT_TRUE(fabs(q / 2e5 - want) <= 0.02 * want + 1e-9);
  }
}

/* exact ties and saturation in Goertzel, NCO rounding, the odd-n noise tail */
void test_fd_tone_edges(void) {
  /* freq 0: c = 2^31 - 1; s1 = 2^29 makes c s1 / 2^30 a tie (rounds up) */
  FD_Goertzel g;
  fd_goertzel_init(&g, 0U, 15U);
  const fm_q15 x[3] = {16384, 0, 0};
  fd_goertzel_q15(&g, x, 2U);
  ASSERT_EQ(g.s1, 1 << 30);
  fd_goertzel_q15(&g, x + 2, 1U);
  ASSERT_EQ(g.s2, 1 << 30);
  int64_t re, im;
  fd_goertzel_dft(&g, &re, &im); /* s2 cos 0 / 2^31 = 2^30 - 0.5: a tie */
  ASSERT_TRUE(re == (int64_t)g.s1 - (1 << 30));
  /* power: c s1 s2 / 2^30 = 0.5 (a tie), and the 2^64 saturation */
  FD_Goertzel h = g;
  h.c = 1;
  h.s1 = 1 << 29;
  h.s2 = 1;
  ASSERT_TRUE(fd_goertzel_power(&h) == (1ULL << 58));
  h.c = INT32_MIN;
  h.s1 = INT32_MIN;
  h.s2 = INT32_MIN;
  ASSERT_TRUE(fd_goertzel_power(&h) == UINT64_MAX);

  /* NCO q15 at a phase whose sine has a q15 tie */
  uint32_t ph = 1U;
  while (((uint32_t)fm_sin_q31(ph) & 0xFFFFU) != 0x8000U) {
    ph += 0x9E3779B1U;
  }
  FD_Nco o;
  fm_q15 s;
  fd_nco_init(&o, ph, 0U, 0);
  fd_nco_q15(&o, &s, NULL, 1U);
  ASSERT_EQ(s, (int32_t)(((int64_t)fm_sin_q31(ph) + 0x8000) >> 16));

  /* uniform q15, odd and even n: two per output word, the last on its own */
  for (uint32_t n = 5U; n <= 6U; n++) {
    FU_Xoshiro128 r, q;
    fu_xoshiro128_seed(&r, 9U);
    fu_xoshiro128_seed(&q, 9U);
    fm_q15 y[8];
    memset(y, 0x5A, sizeof y);
    fd_noise_uniform_q15(&r, y, n);
    for (uint32_t i = 0U; i + 1U < n; i += 2U) {
      uint32_t u = fu_xoshiro128ss(&q);
      ASSERT_TRUE(y[i] == (int16_t)u && y[i + 1U] == (int16_t)(u >> 16));
    }
    if ((n & 1U) != 0U) {
      ASSERT_EQ(y[n - 1U], (int16_t)fu_xoshiro128ss(&q));
    }
    ASSERT_EQ(y[n], 0x5A5A);
  }
}
