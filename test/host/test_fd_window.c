/**
 * @file test_fd_window.c
 * @brief Host tests for fd_window.h against long double windows
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_window.h"
#include "test_stats.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define MAXN 4096U

static const long double coefs[][5] = {
    {1, 0, 0, 0, 0},
    {0.5L, 0.5L, 0, 0, 0},
    {0.54L, 0.46L, 0, 0, 0},
    {0.42L, 0.5L, 0.08L, 0, 0},
    {0.35875L, 0.48829L, 0.14128L, 0.01168L, 0},
    {0.355768L, 0.487396L, 0.144232L, 0.012604L, 0},
    {0.21557895L, 0.41663158L, 0.277263158L, 0.083578947L, 0.006947368L},
};

static long double ref(FD_Window t, uint32_t k, uint32_t n, bool periodic) {
  const long double pi = 3.14159265358979323846264338327950288L;
  uint32_t d = periodic ? n : n - 1U;
  if (d == 0U || t == FD_WIN_RECT) {
    return 1.0L;
  }
  if (t == FD_WIN_BARTLETT) {
    return 1.0L - fabsl((2.0L * k - d) / d);
  }
  long double s = 0.0L;
  for (uint32_t j = 0U; j < 5U; j++) {
    unsigned long long jk = ((unsigned long long)j * k) % d;
    s += ((j & 1U) != 0U ? -1.0L : 1.0L) * coefs[t][j] *
         cosl(2.0L * pi * (long double)jk / d);
  }
  return s;
}

static void check(FD_Window t, uint32_t n, bool periodic, ErrStats *e15,
                  ErrStats *e31) {
  static fm_q15 w15[MAXN + 4U];
  static fm_q31 w31[MAXN + 4U];
  memset(w15, 0x5A, sizeof w15);
  memset(w31, 0x5A, sizeof w31);
  fd_window_q15(w15, n, t, periodic);
  fd_window_q31(w31, n, t, periodic);
  ASSERT_TRUE(w15[n] == 0x5A5A && w31[n] == 0x5A5A5A5A); /* no overrun */
  for (uint32_t k = 0U; k < n; k++) {
    long double r = ref(t, k, n, periodic);
    long double r15 = r * 32768.0L, r31 = r * 2147483648.0L;
    r15 = r15 > 32767.0L ? 32767.0L : r15;
    r31 = r31 > 2147483647.0L ? 2147483647.0L : r31;
    stats_add(e15, k, (double)(w15[k] - r15));
    stats_add(e31, k, (double)(w31[k] - r31));
  }
}

void test_fd_window(void) {
  static const char *name[] = {"rect",     "Hann",    "Hamming",  "Blackman",
                               "B-Harris", "Nuttall", "flat-top", "Bartlett"};
  static const uint32_t lens[] = {1U,   2U,   3U,    4U,    5U,   7U,
                                  8U,   31U,  32U,   33U,   64U,  255U,
                                  256U, 999U, 1024U, 1025U, 4096U};
  ErrStats w15, w31;
  stats_init(&w15, "windows q15 (worst type)", "lsb");
  stats_init(&w31, "windows q31 (worst type)", "lsb");
  for (int t = 0; t <= (int)FD_WIN_BARTLETT; t++) {
    ErrStats e15, e31;
    char n15[48], n31[48];
    snprintf(n15, sizeof n15, "window %s q15", name[t]);
    snprintf(n31, sizeof n31, "window %s q31", name[t]);
    stats_init(&e15, n15, "lsb");
    stats_init(&e31, n31, "lsb");
    for (unsigned i = 0U; i < sizeof lens / sizeof lens[0]; i++) {
      check((FD_Window)t, lens[i], false, &e15, &e31);
      check((FD_Window)t, lens[i], true, &e15, &e31);
    }
    stats_print(&e15);
    stats_print(&e31);
    ASSERT_TRUE(e15.max_abs <= 0.5001);
    ASSERT_TRUE(e31.max_abs <= 3.0);
  }
  (void)w15;
  (void)w31;
  /* exact points: Hann ends at 0 and peaks at full scale */
  fm_q31 h[9];
  fd_window_q31(h, 9U, FD_WIN_HANN, false);
  ASSERT_EQ(h[0], 0);
  ASSERT_EQ(h[8], 0);
  ASSERT_EQ(h[4], INT32_MAX);
  fd_window_q31(h, 8U, FD_WIN_HANN,
                true); /* periodic: 9-point symmetric's head */
  ASSERT_EQ(h[0], 0);
  ASSERT_EQ(h[4], INT32_MAX);
}

static long double bessel_i0l(long double x) {
  long double t = 1.0L, s = 1.0L;
  for (int m = 1; m < 400 && t > 1e-30L * s; m++) {
    t *= x * x / (4.0L * m * m);
    s += t;
  }
  return s;
}

void test_fd_window_param(void) {
  static fm_q15 w15[1100];
  static fm_q31 w31[1100];
  ErrStats e15, e31;
  stats_init(&e15, "Kaiser / Gauss / Tukey q15", "lsb");
  stats_init(&e31, "Kaiser / Gauss / Tukey q31", "lsb");
  static const uint32_t lens[] = {1U, 2U, 5U, 64U, 101U, 1024U, 1025U};
  static const float ps[3][3] = {
      {0.0f, 5.0f, 14.0f}, {0.2f, 0.4f, 1.0f}, {0.0f, 0.5f, 1.0f}};
  for (int kind = 0; kind < 3; kind++) {
    for (int pi = 0; pi < 3; pi++) {
      float p = ps[kind][pi];
      long double pl = (long double)p;
      for (unsigned li = 0U; li < sizeof lens / sizeof lens[0]; li++) {
        for (int per = 0; per < 2; per++) {
          uint32_t n = lens[li], d = per ? n : n - 1U;
          if (kind == 0) {
            fd_window_kaiser_q15(w15, n, p, per != 0);
            fd_window_kaiser_q31(w31, n, p, per != 0);
          } else if (kind == 1) {
            fd_window_gauss_q15(w15, n, p, per != 0);
            fd_window_gauss_q31(w31, n, p, per != 0);
          } else {
            fd_window_tukey_q15(w15, n, p, per != 0);
            fd_window_tukey_q31(w31, n, p, per != 0);
          }
          for (uint32_t k = 0U; k < n; k++) {
            long double r = 1.0L;
            if (d != 0U) {
              long double x = (2.0L * k - d) / d;
              long double kk = k <= d / 2U ? k : d - k;
              if (kind == 0) {
                r = bessel_i0l(pl * sqrtl(1.0L - x * x)) / bessel_i0l(pl);
              } else if (kind == 1) {
                r = expl(-0.5L * (x / pl) * (x / pl));
              } else if (kk < 0.5L * pl * d) {
                r = 0.5L * (1.0L - cosl(2.0L * 3.14159265358979323846L * kk /
                                        (pl * d)));
              }
            }
            long double r15 = fminl(r * 32768.0L, 32767.0L);
            long double r31 = fminl(r * 2147483648.0L, 2147483647.0L);
            stats_add(&e15, k, (double)(w15[k] - r15));
            stats_add(&e31, k, (double)(w31[k] - r31));
          }
        }
      }
    }
  }
  ASSERT_STATS_MAX(&e15, 0.52);
  ASSERT_STATS_MAX(&e31, 1.5e-6 * 2147483648.0);
}

void test_fd_window_gains(void) {
  static fm_q15 w15[1024];
  static fm_q31 w31[1024];
  for (int t = 0; t <= (int)FD_WIN_BARTLETT; t++) {
    fd_window_q15(w15, 1024U, (FD_Window)t, true);
    fd_window_q31(w31, 1024U, (FD_Window)t, true);
    long double s15 = 0, p15 = 0, s31 = 0, p31 = 0;
    for (int k = 0; k < 1024; k++) {
      s15 += w15[k];
      p15 += (long double)w15[k] * w15[k];
      s31 += w31[k];
      p31 += (long double)w31[k] * w31[k];
    }
    fm_q16 c, p;
    fd_window_gains_q15(w15, 1024U, &c, &p);
    ASSERT_NEAR(c, (double)(s15 / 1024 / 32768 * 65536), 0.5);
    ASSERT_NEAR(p, (double)(p15 / 1024 / (32768.0L * 32768) * 65536), 0.5);
    fd_window_gains_q31(w31, 1024U, &c, &p);
    ASSERT_NEAR(c, (double)(s31 / 1024 / 2147483648.0L * 65536), 0.5);
    ASSERT_NEAR(
        p, (double)(p31 / 1024 / (2147483648.0L * 2147483648.0L) * 65536), 0.5);
  }
  /* Hann, periodic: coherent 0.5, power 0.375 */
  fm_q16 c, p;
  fd_window_q31(w31, 1024U, FD_WIN_HANN, true);
  fd_window_gains_q31(w31, 1024U, &c, &p);
  ASSERT_NEAR(c, 32768, 1);
  ASSERT_NEAR(p, 24576, 1);
}

/* exact ties and the power rounding */
void test_fd_window_edges(void) {
  /* periodic n = 2^17: the Bartlett q31 value at k = 1 is 2^15, a q15 tie */
  static fm_q15 w[131072];
  fd_window_q15(w, 131072U, FD_WIN_BARTLETT, true);
  ASSERT_EQ(w[1], 1);
  /* coherent gain of {1, 0, 0, 0}: 2 x 1 / 4 = 0.5 LSB rounds up */
  static const fm_q15 one[4] = {1, 0, 0, 0};
  fm_q16 c, p;
  fd_window_gains_q15(one, 4U, &c, &p);
  ASSERT_EQ(c, 1);
  /* q31 power 1.60001 LSB: rounds to 2 */
  static const fm_q31 v[1] = {10610885};
  fd_window_gains_q31(v, 1U, &c, &p);
  ASSERT_EQ(p, 2);
}
