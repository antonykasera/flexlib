/**
 * @file test_fd_fir.c
 * @brief Host tests for fd_fir.h against exact 128-bit convolutions
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_fir.h"
#include "ref_dsp.h"
#include "test_stats.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NX 600U
#define MAXT 70U

static uint64_t rng = 77U;

static const uint32_t taps[] = {1U, 2U,  3U,  4U,  5U,  6U,  7U,  8U,
                                9U, 15U, 16U, 17U, 31U, 32U, 33U, 64U};
static const uint32_t chunks[] = {1U, 2U, 3U, 7U, 64U, 200U};
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static i128 rdiv(i128 v, unsigned s) {
  return (v + ((i128)1 << (s - 1U))) >> s;
}

static int32_t clamp(i128 v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* coefficients with sum |h| near a target (in LSBs), mixed signs */
static void coefs15(fm_q15 *h, uint32_t t, uint64_t target) {
  for (uint32_t k = 0U; k < t; k++) {
    uint64_t share = target / t + (k < target % t ? 1U : 0U);
    int32_t v = (int32_t)(share > 32768U ? 32768U : share);
    h[k] = (fm_q15)((test_rng_u32(&rng) & 1U) != 0U ? -v
                                                    : (v > 32767 ? 32767 : v));
  }
}

static void signal15(fm_q15 *x, uint32_t n, int kind) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = test_rng_u32(&rng);
    x[i] = kind == 0   ? (fm_q15)(u >> 16)
           : kind == 1 ? (fm_q15)((u & 1U) != 0U ? 32767 : -32768)
                       : (fm_q15)-32768;
  }
}

static void signal31(fm_q31 *x, uint32_t n, int kind) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = test_rng_u32(&rng);
    x[i] = kind == 0   ? (fm_q31)u
           : kind == 1 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN)
                       : INT32_MIN;
  }
}

static void ref15(const fm_q15 *h, uint32_t t, const fm_q15 *x, uint32_t n,
                  fm_q15 *y) {
  for (uint32_t i = 0U; i < n; i++) {
    i128 a = 0;
    for (uint32_t k = 0U; k < t && k <= i; k++) {
      a += (i128)h[k] * x[i - k];
    }
    y[i] = (fm_q15)clamp(rdiv(a, 15U), -32768, 32767);
  }
}

static void ref31(const fm_q31 *h, uint32_t t, const fm_q31 *x, uint32_t n,
                  fm_q31 *y) {
  for (uint32_t i = 0U; i < n; i++) {
    i128 a = 0;
    for (uint32_t k = 0U; k < t && k <= i; k++) {
      a += (i128)h[k] * x[i - k];
    }
    y[i] = clamp(rdiv(a, 31U), INT32_MIN, INT32_MAX);
  }
}

/* feeds n samples in random-length calls */
static uint32_t piece(uint32_t left) {
  uint32_t r = test_rng_u32(&rng) % 5U;
  uint32_t p = r == 0U ? 1U : (r == 1U ? 70U : test_rng_u32(&rng) % 23U + 1U);
  return p < left ? p : left;
}

void test_fd_fir_q15(void) {
  static fm_q15 h[MAXT], x[NX], y[NX], r[NX], st[MAXT + 256U];
  static const uint64_t gains[] = {100U, 30000U, 65535U, 65536U, 200000U};
  for (unsigned ti = 0U; ti < COUNT(taps); ti++) {
    uint32_t t = taps[ti];
    for (unsigned gi = 0U; gi < COUNT(gains); gi++) {
      coefs15(h, t, gains[gi] < 32768U * t ? gains[gi] : 32768U * t - 1U);
      for (int kind = 0; kind < 3; kind++) {
        signal15(x, NX, kind);
        ref15(h, t, x, NX, r);
        for (unsigned ci = 0U; ci < COUNT(chunks); ci++) {
          FD_FirQ15 f;
          fd_fir_init_q15(&f, h, t, st, FD_FIR_STATE_LEN(t, chunks[ci]));
          for (uint32_t i = 0U, p; i < NX; i += p) {
            p = piece(NX - i);
            fd_fir_q15(&f, x + i, y + i, p);
          }
          ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
        }
        FD_FirQ15 f; /* in place, then one sample at a time after a reset */
        fd_fir_init_q15(&f, h, t, st, FD_FIR_STATE_LEN(t, 64U));
        memcpy(y, x, sizeof y);
        fd_fir_q15(&f, y, y, NX);
        ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
        fd_fir_reset_q15(&f);
        for (uint32_t i = 0U; i < NX; i++) {
          ASSERT_EQ(fd_fir_sample_q15(&f, x[i]), r[i]);
        }
      }
    }
  }
}

void test_fd_fir_q31(void) {
  static fm_q31 h[MAXT], x[NX], y[NX], r[NX], st[MAXT + 256U];
  for (unsigned ti = 0U; ti < COUNT(taps); ti++) {
    uint32_t t = taps[ti];
    for (int g = 0; g < 4; g++) {
      /* sum |h| below 2, just below, exactly 2 (wide), far above */
      for (uint32_t k = 0U; k < t; k++) {
        uint32_t u = test_rng_u32(&rng);
        int64_t v = g == 0   ? (int64_t)(int32_t)u >> 6
                    : g == 3 ? (int64_t)(int32_t)u | INT32_MIN
                             : (int64_t)((1ULL << 32) - (g == 1 ? 1U : 0U)) / t;
        v = v > INT32_MAX ? INT32_MAX : v;
        h[k] = (fm_q31)((u & 1U) != 0U && g != 3 ? -v : v);
      }
      if (g == 2 && t == 1U) {
        h[0] = INT32_MIN; /* |h| = 1 is not wide; check it anyway */
      }
      for (int kind = 0; kind < 3; kind++) {
        signal31(x, NX, kind);
        ref31(h, t, x, NX, r);
        for (unsigned ci = 0U; ci < COUNT(chunks); ci++) {
          FD_FirQ31 f;
          fd_fir_init_q31(&f, h, t, st, FD_FIR_STATE_LEN(t, chunks[ci]));
          for (uint32_t i = 0U, p; i < NX; i += p) {
            p = piece(NX - i);
            fd_fir_q31(&f, x + i, y + i, p);
          }
          ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
        }
        FD_FirQ31 f;
        fd_fir_init_q31(&f, h, t, st, FD_FIR_STATE_LEN(t, 5U));
        memcpy(y, x, sizeof y);
        fd_fir_q31(&f, y, y, NX);
        ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
        fd_fir_reset_q31(&f);
        for (uint32_t i = 0U; i < 50U; i++) {
          ASSERT_EQ(fd_fir_sample_q31(&f, x[i]), r[i]);
        }
      }
    }
  }
}

void test_fd_fir_decim(void) {
  static fm_q15 h[MAXT], x[NX], y[NX], r[NX], st[MAXT + 256U];
  static fm_q31 H[MAXT], X[NX], Y[NX], R[NX], ST[MAXT + 256U];
  for (unsigned ti = 0U; ti < COUNT(taps); ti++) {
    uint32_t t = taps[ti];
    for (uint32_t m = 1U; m <= 7U; m++) {
      coefs15(h, t, m == 3U ? 70000U : 40000U);
      for (uint32_t k = 0U; k < t; k++) {
        H[k] = (fm_q31)test_rng_u32(&rng) >> (m == 3U ? 0U : 5U);
      }
      signal15(x, NX, (int)(m % 3U));
      signal31(X, NX, (int)(m % 3U));
      ref15(h, t, x, NX, r);
      ref31(H, t, X, NX, R);
      for (unsigned ci = 0U; ci < COUNT(chunks); ci++) {
        FD_FirDecimQ15 d;
        FD_FirDecimQ31 D;
        fd_fir_decim_init_q15(&d, h, t, m, st, FD_FIR_STATE_LEN(t, chunks[ci]));
        fd_fir_decim_init_q31(&D, H, t, m, ST, FD_FIR_STATE_LEN(t, chunks[ci]));
        uint32_t o = 0U, O = 0U;
        for (uint32_t i = 0U, p; i < NX; i += p) {
          p = piece(NX - i);
          o += fd_fir_decim_q15(&d, x + i, y + o, p);
          O += fd_fir_decim_q31(&D, X + i, Y + O, p);
        }
        ASSERT_EQ(o, NX / m);
        ASSERT_EQ(O, NX / m);
        for (uint32_t j = 0U; j < o; j++) {
          ASSERT_EQ(y[j], r[(j + 1U) * m - 1U]);
          ASSERT_EQ(Y[j], R[(j + 1U) * m - 1U]);
        }
      }
      FD_FirDecimQ15 d; /* in place */
      fd_fir_decim_init_q15(&d, h, t, m, st, FD_FIR_STATE_LEN(t, 3U));
      memcpy(y, x, sizeof y);
      ASSERT_EQ(fd_fir_decim_q15(&d, y, y, NX), NX / m);
      for (uint32_t j = 0U; j < NX / m; j++) {
        ASSERT_EQ(y[j], r[(j + 1U) * m - 1U]);
      }
    }
  }
}

void test_fd_fir_interp(void) {
  static fm_q15 h[MAXT], x[NX / 8U], y[NX], r[NX], u[NX], st[MAXT + 256U];
  static fm_q31 H[MAXT], X[NX / 8U], Y[NX], R[NX], U[NX], ST[MAXT + 256U];
  const uint32_t nx = NX / 8U;
  for (unsigned ti = 0U; ti < COUNT(taps); ti++) {
    uint32_t t = taps[ti];
    for (uint32_t l = 1U; l <= 8U; l++) {
      coefs15(h, t, l == 5U ? 100000U : 40000U);
      for (uint32_t k = 0U; k < t; k++) {
        H[k] = (fm_q31)test_rng_u32(&rng) >> (l == 5U ? 0U : 5U);
      }
      signal15(x, nx, (int)(l % 3U));
      signal31(X, nx, (int)(l % 3U));
      memset(u, 0, sizeof u); /* zero-stuffed */
      memset(U, 0, sizeof U);
      for (uint32_t i = 0U; i < nx; i++) {
        u[i * l] = x[i];
        U[i * l] = X[i];
      }
      ref15(h, t, u, nx * l, r);
      ref31(H, t, U, nx * l, R);
      for (unsigned ci = 0U; ci < COUNT(chunks); ci++) {
        FD_FirInterpQ15 p;
        FD_FirInterpQ31 P;
        fd_fir_interp_init_q15(&p, h, t, l, st,
                               FD_FIR_INTERP_STATE_LEN(t, l, chunks[ci]));
        fd_fir_interp_init_q31(&P, H, t, l, ST,
                               FD_FIR_INTERP_STATE_LEN(t, l, chunks[ci]));
        for (uint32_t i = 0U, q; i < nx; i += q) {
          q = piece(nx - i);
          fd_fir_interp_q15(&p, x + i, y + i * l, q);
          fd_fir_interp_q31(&P, X + i, Y + i * l, q);
        }
        ASSERT_TRUE(memcmp(y, r, nx * l * sizeof y[0]) == 0);
        ASSERT_TRUE(memcmp(Y, R, nx * l * sizeof Y[0]) == 0);
      }
    }
  }
}

/* a branch with gain exactly 2 and full-scale input needs the wide sums */
void test_fd_fir_interp_gain2(void) {
  static const fm_q15 h[4] = {-32768, 5, -32768, 7};
  static const fm_q31 H[4] = {INT32_MIN, 5, INT32_MIN, 7};
  fm_q15 x[8], y[16], st[FD_FIR_INTERP_STATE_LEN(4U, 2U, 8U)];
  fm_q31 X[8], Y[16], ST[FD_FIR_INTERP_STATE_LEN(4U, 2U, 8U)];
  for (int i = 0; i < 8; i++) {
    x[i] = -32768;
    X[i] = INT32_MIN;
  }
  FD_FirInterpQ15 p;
  FD_FirInterpQ31 P;
  fd_fir_interp_init_q15(&p, h, 4U, 2U, st, sizeof st / sizeof st[0]);
  fd_fir_interp_init_q31(&P, H, 4U, 2U, ST, sizeof ST / sizeof ST[0]);
  fd_fir_interp_q15(&p, x, y, 8U);
  fd_fir_interp_q31(&P, X, Y, 8U);
  for (int i = 2; i < 16; i += 2) { /* branch 0: 2 x 32768^2 = 2^31 */
    ASSERT_EQ(y[i], 32767);
    ASSERT_EQ(Y[i], INT32_MAX);
  }
}

/* windowed sinc in double, written independently of fd_fir_design */
static void ref_design(double *h, uint32_t n, FD_Band band, double f1,
                       double f2, const double w[4]) {
  const double pi = 3.14159265358979323846;
  double c = (n - 1U) / 2.0, g = 0.0;
  double f0 = band == FD_HIGHPASS   ? 0.5
              : band == FD_BANDPASS ? (f1 + f2) / 2
                                    : 0.0;
  for (uint32_t k = 0U; k < n; k++) {
    double m = k - c;
    double l1 = m == 0.0 ? 2.0 * f1 : sin(2.0 * pi * f1 * m) / (pi * m);
    double l2 = m == 0.0 ? 2.0 * f2 : sin(2.0 * pi * f2 * m) / (pi * m);
    double d = m == 0.0 ? 1.0 : 0.0;
    double v = band == FD_LOWPASS    ? l1
               : band == FD_HIGHPASS ? d - l1
               : band == FD_BANDPASS ? l2 - l1
                                     : d - (l2 - l1);
    double t = n > 1U ? 2.0 * pi * k / (n - 1U) : 0.0;
    h[k] = v * (w[0] - w[1] * cos(t) + w[2] * cos(2 * t) - w[3] * cos(3 * t));
    g += h[k] * cos(2.0 * pi * f0 * m);
  }
  for (uint32_t k = 0U; k < n; k++) {
    h[k] /= g;
  }
}

void test_fd_fir_design(void) {
  static const double win[5][4] = {{1, 0, 0, 0},
                                   {0.5, 0.5, 0, 0},
                                   {0.54, 0.46, 0, 0},
                                   {0.42, 0.5, 0.08, 0},
                                   {0.35875, 0.48829, 0.14128, 0.01168}};
  static const double sidelobe_db[5] = {-20, -43, -52, -73, -90};
  float h[101];
  double d[101];
  for (int w = 0; w < 5; w++) {
    for (int b = 0; b < 4; b++) {
      uint32_t n = 101U;
      const double f1 = 0.15, f2 = 0.3;
      ASSERT_EQ(
          fd_fir_design(h, n, (FD_Band)b, (float)f1, (float)f2, (FD_Window)w),
          FD_OK);
      ref_design(d, n, (FD_Band)b, f1, f2, win[w]);
      for (uint32_t k = 0U; k < n; k++) {
        ASSERT_NEAR((double)h[k], d[k], 2e-6);
        ASSERT_NEAR((double)h[k], (double)h[n - 1U - k], 1e-6);
      }
      /* the stopband: well past the transition, below the window's sidelobes */
      double hd[101];
      for (uint32_t k = 0U; k < n; k++) {
        hd[k] = (double)h[k];
      }
      double worst = -400.0;
      for (double f = 0.0; f <= 0.5; f += 0.0025) {
        bool stop = b == FD_LOWPASS    ? f > f1 + 0.06
                    : b == FD_HIGHPASS ? f < f1 - 0.06
                    : b == FD_BANDPASS ? f < f1 - 0.06 || f > f2 + 0.06
                                       : f > f1 + 0.06 && f < f2 - 0.06;
        if (stop && w != 0) {
          double re, im;
          ref_dft_bin(hd, n, f, &re, &im);
          double db = 10.0 * log10(re * re + im * im + 1e-30);
          worst = db > worst ? db : worst;
        }
      }
      if (w != 0) {
        ASSERT_TRUE(worst < sidelobe_db[w] + 3.0);
      }
    }
  }
  /* the window's ends: at f1 = 0.105 the sinc peaks there (sin 10.5 pi) */
  ASSERT_EQ(
      fd_fir_design(h, 101U, FD_LOWPASS, 0.105f, 0.0f, FD_WIN_BLACKMAN_HARRIS),
      FD_OK);
  ref_design(d, 101U, FD_LOWPASS, 0.105, 0.0, win[4]);
  ASSERT_TRUE(fabs((double)h[0] - d[0]) <= 1e-2 * fabs(d[0]));
  ASSERT_TRUE(fabs((double)h[100] - d[100]) <= 1e-2 * fabs(d[100]));

  ASSERT_EQ(fd_fir_design(h, 100U, FD_HIGHPASS, 0.2f, 0.0f, FD_WIN_HANN),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_fir_design(h, 101U, FD_BANDPASS, 0.3f, 0.2f, FD_WIN_HANN),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_fir_design(h, 11U, FD_LOWPASS, 0.5f, 0.0f, FD_WIN_HANN),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_fir_design(h, 1U, FD_LOWPASS, 0.25f, 0.0f, FD_WIN_RECT), FD_OK);
  ASSERT_NEAR((double)h[0], 1.0, 0.0);

  float v[4] = {0.5f, -1.0f, 0.99999f, 1.0f};
  fm_q15 q[4];
  fm_q31 Q[4];
  ASSERT_EQ(fd_fir_quantize_q15(v, q, 2U), FD_OK);
  ASSERT_TRUE(q[0] == 16384 && q[1] == -32768);
  ASSERT_EQ(fd_fir_quantize_q15(v, q, 3U), FD_ERR_RANGE);
  ASSERT_EQ(q[2], 32767);
  ASSERT_EQ(fd_fir_quantize_q31(v, Q, 3U), FD_OK);
  ASSERT_EQ(fd_fir_quantize_q31(v, Q, 4U), FD_ERR_RANGE);
  ASSERT_EQ(Q[3], INT32_MAX);

  fm_q15 g15[3] = {-32768, 16384, 1};
  fm_q31 g31[2] = {INT32_MIN, 1 << 30};
  ASSERT_EQ(fd_fir_gain_q15(g15, 3U), 2 * (32768 + 16384 + 1));
  ASSERT_EQ(fd_fir_gain_q31(g31, 2U), 65536 + 32768);
  fm_q31 half = 16384; /* 0.5 LSB of Q16.16 rounds up */
  ASSERT_EQ(fd_fir_gain_q31(&half, 1U), 1);
}
