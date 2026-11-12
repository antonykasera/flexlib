/**
 * @file exhaustive_fd_window.c
 * @brief Every FD_Window type at every length up to 4096, symmetric and periodic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_window.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static long double ref(int t, uint32_t k, uint32_t n, int periodic) {
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

int main(void) {
  double w15max[8] = {0}, w31max[8] = {0};
#pragma omp parallel
  {
    fm_q15 *w15 = malloc(MAXN * sizeof *w15);
    fm_q31 *w31 = malloc(MAXN * sizeof *w31);
    double m15[8] = {0}, m31[8] = {0};
#pragma omp for schedule(dynamic, 16)
    for (uint32_t n = 1U; n <= MAXN; n++) {
      for (int t = 0; t <= (int)FD_WIN_BARTLETT; t++) {
        for (int per = 0; per < 2; per++) {
          fd_window_q15(w15, n, (FD_Window)t, per != 0);
          fd_window_q31(w31, n, (FD_Window)t, per != 0);
          for (uint32_t k = 0U; k < n; k++) {
            long double r = ref(t, k, n, per);
            long double r15 = fminl(r * 32768.0L, 32767.0L);
            long double r31 = fminl(r * 2147483648.0L, 2147483647.0L);
            double e15 = fabs((double)(w15[k] - r15));
            double e31 = fabs((double)(w31[k] - r31));
            m15[t] = e15 > m15[t] ? e15 : m15[t];
            m31[t] = e31 > m31[t] ? e31 : m31[t];
          }
        }
      }
    }
#pragma omp critical
    for (int t = 0; t < 8; t++) {
      w15max[t] = m15[t] > w15max[t] ? m15[t] : w15max[t];
      w31max[t] = m31[t] > w31max[t] ? m31[t] : w31max[t];
    }
    free(w15);
    free(w31);
  }
  int bad = 0;
  printf("exhaustive: windows, every n <= %u, symmetric and periodic, max LSB "
         "q15 / q31:",
         MAXN);
  for (int t = 0; t < 8; t++) {
    printf(" %.4g/%.4g", w15max[t], w31max[t]);
    bad |= w15max[t] > 0.5001 || w31max[t] > 3.0;
  }
  printf("\n");
  return bad;
}
