/**
 * @file ref_dsp.h
 * @brief Double-precision DSP references for the flexdsp tests
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REF_DSP_H
#define REF_DSP_H

#include <stdint.h>

/* Fixed point to double, for comparing kernel outputs in LSBs */
static inline double ref_q15(int32_t x) { return (double)x / 32768.0; }
static inline double ref_q31(int32_t x) { return (double)x / 2147483648.0; }

/* y[k] = sum_j h[j] x[k - j], from zero state */
void ref_fir(const double *h, unsigned ntaps, const double *x, unsigned n,
             double *y);

/* DF1 sections b0 b1 b2 a1 a2, CMSIS signs: y = b.x + a1 y[-1] + a2 y[-2] */
void ref_biquad(const double *coef, unsigned nsec, const double *x, unsigned n,
                double *y);

/* Full linear convolution: y has na + nb - 1 samples */
void ref_conv(const double *a, unsigned na, const double *b, unsigned nb,
              double *y);

/* r[m] = sum_i a[i + m - nb + 1] b[i], m = 0 .. na + nb - 2 */
void ref_corr(const double *a, unsigned na, const double *b, unsigned nb,
              double *r);

/* Direct DFT, X[k] = sum_j x[j] e^(-+2 pi i jk/n); inverse adds no 1/n */
void ref_dft(const double *re, const double *im, unsigned n, double *ore,
             double *oim, int inverse);

/* the same transform in O(n log n), n a power of two; in place */
void ref_fft(double *re, double *im, unsigned n, int inverse);

/* one DFT term at f cycles per sample */
void ref_dft_bin(const double *x, unsigned n, double f, double *re, double *im);

/* sum_j (-1)^j a[j] cos(2 pi j k / d), d = n - 1 (symmetric) or n */
void ref_window_cosine(const double *a, unsigned terms, unsigned n,
                       int symmetric, double *w);

/* zero-stuff by l, FIR h, keep every m-th: (n l + m - 1) / m outputs */
void ref_resample(const double *h, unsigned ntaps, unsigned l, unsigned m,
                  const double *x, unsigned n, double *y);

#endif /* REF_DSP_H */
