/**
 * @file ref_dsp.c
 * @brief Double-precision DSP references for the flexdsp tests
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "ref_dsp.h"
#include <math.h>
#include <stdlib.h>

static const double two_pi = 6.283185307179586476925286766559;

void ref_fir(const double *h, unsigned ntaps, const double *x, unsigned n,
             double *y) {
  for (unsigned k = 0U; k < n; k++) {
    long double acc = 0.0L;
    for (unsigned j = 0U; j < ntaps && j <= k; j++) {
      acc += (long double)h[j] * (long double)x[k - j];
    }
    y[k] = (double)acc;
  }
}

void ref_biquad(const double *coef, unsigned nsec, const double *x, unsigned n,
                double *y) {
  for (unsigned i = 0U; i < n; i++) {
    y[i] = x[i];
  }
  for (unsigned s = 0U; s < nsec; s++) {
    const double *c = &coef[5U * s];
    long double x1 = 0.0L, x2 = 0.0L, y1 = 0.0L, y2 = 0.0L;
    for (unsigned i = 0U; i < n; i++) {
      long double x0 = (long double)y[i];
      long double y0 = (long double)c[0] * x0 + (long double)c[1] * x1 +
                       (long double)c[2] * x2 + (long double)c[3] * y1 +
                       (long double)c[4] * y2;
      x2 = x1;
      x1 = x0;
      y2 = y1;
      y1 = y0;
      y[i] = (double)y0;
    }
  }
}

void ref_conv(const double *a, unsigned na, const double *b, unsigned nb,
              double *y) {
  for (unsigned k = 0U; k < na + nb - 1U; k++) {
    long double acc = 0.0L;
    for (unsigned j = 0U; j < nb; j++) {
      if (k >= j && k - j < na) {
        acc += (long double)a[k - j] * (long double)b[j];
      }
    }
    y[k] = (double)acc;
  }
}

void ref_corr(const double *a, unsigned na, const double *b, unsigned nb,
              double *r) {
  for (unsigned m = 0U; m < na + nb - 1U; m++) {
    long lag = (long)m - (long)(nb - 1U);
    long double acc = 0.0L;
    for (unsigned i = 0U; i < nb; i++) {
      long ia = (long)i + lag;
      if (ia >= 0 && ia < (long)na) {
        acc += (long double)a[ia] * (long double)b[i];
      }
    }
    r[m] = (double)acc;
  }
}

void ref_dft(const double *re, const double *im, unsigned n, double *ore,
             double *oim, int inverse) {
  const long double sign = inverse ? 1.0L : -1.0L;
  for (unsigned k = 0U; k < n; k++) {
    long double sr = 0.0L, si = 0.0L;
    for (unsigned j = 0U; j < n; j++) {
      /* the angle from (jk mod n): exact, and small */
      unsigned long long jk = ((unsigned long long)j * k) % n;
      long double t = sign * (long double)two_pi * (long double)jk / n;
      long double c = cosl(t), s = sinl(t);
      long double xr = (long double)re[j];
      long double xi = im != NULL ? (long double)im[j] : 0.0L;
      sr += xr * c - xi * s;
      si += xr * s + xi * c;
    }
    ore[k] = (double)sr;
    oim[k] = (double)si;
  }
}

void ref_fft(double *re, double *im, unsigned n, int inverse) {
  const long double sign = inverse ? 1.0L : -1.0L;
  unsigned m = 0U;
  while ((1U << m) < n) {
    m++;
  }
  for (unsigned i = 0U; i < n; i++) {
    unsigned r = 0U;
    for (unsigned b = 0U; b < m; b++) {
      r |= ((i >> b) & 1U) << (m - 1U - b);
    }
    if (i < r) {
      double t = re[i];
      re[i] = re[r];
      re[r] = t;
      t = im[i];
      im[i] = im[r];
      im[r] = t;
    }
  }
  for (unsigned h = 1U; h < n; h *= 2U) {
    for (unsigned j = 0U; j < h; j++) {
      long double t = sign * (long double)two_pi * (long double)j / (2U * h);
      long double c = cosl(t), s = sinl(t);
      for (unsigned p = j; p < n; p += 2U * h) {
        long double br = (long double)re[p + h], bi = (long double)im[p + h];
        long double pr = br * c - bi * s, pi = br * s + bi * c;
        long double ar = (long double)re[p], ai = (long double)im[p];
        re[p] = (double)(ar + pr);
        im[p] = (double)(ai + pi);
        re[p + h] = (double)(ar - pr);
        im[p + h] = (double)(ai - pi);
      }
    }
  }
}

void ref_dft_bin(const double *x, unsigned n, double f, double *re,
                 double *im) {
  long double sr = 0.0L, si = 0.0L;
  for (unsigned j = 0U; j < n; j++) {
    long double turns = (long double)f * j;
    turns -= floorl(turns);
    long double t = -(long double)two_pi * turns;
    sr += (long double)x[j] * cosl(t);
    si += (long double)x[j] * sinl(t);
  }
  *re = (double)sr;
  *im = (double)si;
}

void ref_window_cosine(const double *a, unsigned terms, unsigned n,
                       int symmetric, double *w) {
  unsigned d = symmetric ? n - 1U : n;
  for (unsigned k = 0U; k < n; k++) {
    long double acc = 0.0L;
    for (unsigned j = 0U; j < terms; j++) {
      unsigned long long jk = d != 0U ? ((unsigned long long)j * k) % d : 0U;
      long double t = (long double)two_pi * (long double)jk /
                      (long double)(d != 0U ? d : 1U);
      acc += ((j & 1U) != 0U ? -1.0L : 1.0L) * (long double)a[j] * cosl(t);
    }
    w[k] = (double)acc;
  }
}

void ref_resample(const double *h, unsigned ntaps, unsigned l, unsigned m,
                  const double *x, unsigned n, double *y) {
  unsigned nu = n * l;
  for (unsigned o = 0U; o * m < nu; o++) {
    unsigned k = o * m; /* index into the upsampled signal */
    long double acc = 0.0L;
    for (unsigned j = 0U; j < ntaps && j <= k; j++) {
      unsigned u = k - j;
      if (u % l == 0U) {
        acc += (long double)h[j] * (long double)x[u / l];
      }
    }
    y[o] = (double)acc;
  }
}
