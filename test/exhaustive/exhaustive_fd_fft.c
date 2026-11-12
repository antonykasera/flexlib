/**
 * @file exhaustive_fd_fft.c
 * @brief Every size and bin against the exact DFT, 10^6 small blocks
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fft.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define NMAX FD_FFT_MAX

/* long double radix-2, in place; the reference for every check */
static void ref_fft(long double *re, long double *im, uint32_t n) {
  const long double pi = 3.14159265358979323846264338327950288L;
  for (uint32_t i = 1U, j = 0U; i < n; i++) {
    uint32_t b = n >> 1;
    for (; (j & b) != 0U; b >>= 1) {
      j ^= b;
    }
    j |= b;
    if (i < j) {
      long double t = re[i];
      re[i] = re[j];
      re[j] = t;
      t = im[i];
      im[i] = im[j];
      im[j] = t;
    }
  }
  for (uint32_t h = 1U; h < n; h *= 2U) {
    for (uint32_t k = 0U; k < h; k++) {
      long double c = cosl(pi * k / h), s = -sinl(pi * k / h);
      for (uint32_t p = k; p < n; p += 2U * h) {
        long double br = re[p + h] * c - im[p + h] * s;
        long double bi = re[p + h] * s + im[p + h] * c;
        re[p + h] = re[p] - br;
        im[p + h] = im[p] - bi;
        re[p] += br;
        im[p] += bi;
      }
    }
  }
}

static uint32_t lg2(uint32_t n) {
  uint32_t m = 0U;
  while ((1U << m) < n) {
    m++;
  }
  return m;
}

/* the largest error over the bins, in LSB of the result's exponent */
static double err_c15(const fd_cq15 *y, int32_t e, const long double *re,
                      const long double *im, uint32_t n) {
  double m = 0.0;
  for (uint32_t k = 0U; k < n; k++) {
    m = fmax(m, (double)fabsl(ldexpl(y[k].re, e) - re[k]) / ldexp(1.0, e));
    m = fmax(m, (double)fabsl(ldexpl(y[k].im, e) - im[k]) / ldexp(1.0, e));
  }
  return m;
}

static double err_c31(const fd_cq31 *y, int32_t e, const long double *re,
                      const long double *im, uint32_t n) {
  double m = 0.0;
  for (uint32_t k = 0U; k < n; k++) {
    m = fmax(m, (double)fabsl(ldexpl(y[k].re, e) - re[k]) / ldexp(1.0, e));
    m = fmax(m, (double)fabsl(ldexpl(y[k].im, e) - im[k]) / ldexp(1.0, e));
  }
  return m;
}

/* real layout: slot 0 holds Re X[0], Re X[n/2] */
static double err_r(const int32_t *y, int32_t e, const long double *re,
                    const long double *im, uint32_t n) {
  double m = (double)fmaxl(fabsl(ldexpl(y[0], e) - re[0]),
                           fabsl(ldexpl(y[1], e) - re[n / 2U]));
  for (uint32_t k = 1U; k < n / 2U; k++) {
    m = fmax(m, (double)fabsl(ldexpl(y[2U * k], e) - re[k]));
    m = fmax(m, (double)fabsl(ldexpl(y[2U * k + 1U], e) - im[k]));
  }
  return m / ldexp(1.0, e);
}

/* the stage errors (0.5 LSB each, passed on with up to the stage gain over
 * its shift) stay within this, measured with margin */
static double bound(uint32_t n) { return 1.0 + 0.75 * lg2(n); }

int main(void) {
  const long double pi = 3.14159265358979323846264338327950288L;
  double worst[4][13] = {{0}};
  unsigned fails = 0U;
  for (uint32_t m = 1U; (1U << m) <= NMAX; m++) {
    const uint32_t n = 1U << m;
#pragma omp parallel
    {
      fd_cq15 *c15 = malloc(n * sizeof *c15);
      fd_cq31 *c31 = malloc(n * sizeof *c31);
      fm_q15 *r15 = aligned_alloc(4U, ((n * sizeof *r15) + 3U) & ~3U);
      fm_q31 *r31 = malloc(n * sizeof *r31);
      long double *re = malloc(n * sizeof *re), *im = malloc(n * sizeof *im);
      int32_t *ry = malloc(n * sizeof *ry);
      double w[4] = {0};
      unsigned f = 0U;
#pragma omp for schedule(dynamic, 8)
      for (uint32_t k = 0U; k < n; k++) {
        /* e^(2 pi i jk/n) at full scale, quantised; the reference is the
         * exact DFT of the quantised values */
        for (int q31 = 0; q31 <= 1; q31++) {
          long double a = q31 ? 2147483647.0L : 32767.0L;
          for (uint32_t j = 0U; j < n; j++) {
            unsigned long long t = (unsigned long long)j * k % n;
            long double ph = 2.0L * pi * (long double)t / n;
            long double vr = roundl(a * cosl(ph)), vi = roundl(a * sinl(ph));
            re[j] = vr;
            im[j] = vi;
            if (q31) {
              c31[j].re = (fm_q31)vr;
              c31[j].im = (fm_q31)vi;
            } else {
              c15[j].re = (fm_q15)vr;
              c15[j].im = (fm_q15)vi;
            }
          }
          ref_fft(re, im, n);
          int32_t e = q31 ? fd_cfft_q31(c31, n) : fd_cfft_q15(c15, n);
          double d =
              q31 ? err_c31(c31, e, re, im, n) : err_c15(c15, e, re, im, n);
          w[q31] = fmax(w[q31], d);
          f += d > bound(n);
          if (n < 4U) {
            continue;
          }
          /* a cosine at bin k (of n reals) for the real transform */
          for (uint32_t j = 0U; j < n; j++) {
            unsigned long long t = (unsigned long long)j * k % n;
            long double v = roundl(a * cosl(2.0L * pi * (long double)t / n));
            re[j] = v;
            im[j] = 0.0L;
            if (q31) {
              r31[j] = (fm_q31)v;
            } else {
              r15[j] = (fm_q15)v;
            }
          }
          ref_fft(re, im, n);
          e = q31 ? fd_rfft_q31(r31, n) : fd_rfft_q15(r15, n);
          for (uint32_t j = 0U; j < n; j++) {
            ry[j] = q31 ? r31[j] : r15[j];
          }
          d = err_r(ry, e, re, im, n);
          w[2 + q31] = fmax(w[2 + q31], d);
          f += d > bound(n);
        }
      }
#pragma omp critical
      {
        for (int i = 0; i < 4; i++) {
          worst[i][m] = fmax(worst[i][m], w[i]);
        }
        fails += f;
      }
      free(c15);
      free(c31);
      free(r15);
      free(r31);
      free(re);
      free(im);
      free(ry);
    }
    printf("n %4u: tones, max error cfft q15 %.2f q31 %.2f, rfft q15 %.2f "
           "q31 %.2f LSB (bound %.2f)\n",
           n, worst[0][m], worst[1][m], worst[2][m], worst[3][m], bound(n));
  }

  /* 10^6 blocks of 2..16 points: random, and corners (every value -2^b or
   * 2^b - 1) that reach each stage's bound */
  double small = 0.0;
#pragma omp parallel reduction(max : small) reduction(+ : fails)
  {
    fd_cq15 c15[16];
    fd_cq31 c31[16];
    long double re[16], im[16];
#pragma omp for
    for (uint32_t blk = 0U; blk < 1000000U; blk++) {
      uint32_t s = blk * 2654435761U + 1U;
      uint32_t n = 2U << (blk % 4U);
      int corner = (blk / 4U) % 2U;
      int q31 = (blk / 8U) % 2U;
      for (uint32_t j = 0U; j < n; j++) {
        int64_t v[2];
        for (int c = 0; c < 2; c++) {
          s ^= s << 13;
          s ^= s >> 17;
          s ^= s << 5;
          if (corner) {
            v[c] = (s & 1U) != 0U ? (q31 ? INT32_MAX : 32767)
                                  : (q31 ? INT32_MIN : -32768);
          } else {
            v[c] = q31 ? (int64_t)(int32_t)s : (int64_t)(int16_t)s;
          }
        }
        re[j] = (long double)v[0];
        im[j] = (long double)v[1];
        if (q31) {
          c31[j].re = (fm_q31)v[0];
          c31[j].im = (fm_q31)v[1];
        } else {
          c15[j].re = (fm_q15)v[0];
          c15[j].im = (fm_q15)v[1];
        }
      }
      ref_fft(re, im, n);
      int32_t e = q31 ? fd_cfft_q31(c31, n) : fd_cfft_q15(c15, n);
      double d = q31 ? err_c31(c31, e, re, im, n) : err_c15(c15, e, re, im, n);
      small = fmax(small, d);
      fails += d > bound(n);
    }
  }
  printf("10^6 blocks of 2..16 points, random and corners: max error %.2f "
         "LSB\n",
         small);
  if (fails != 0U) {
    printf("FAIL: %u transforms past the bound\n", fails);
    return 1;
  }
  printf("PASS\n");
  return 0;
}
