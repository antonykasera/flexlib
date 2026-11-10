/**
 * @file ref_dsp.h
 * @brief Double-precision DSP references for the flexdsp tests
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
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

/* Cascade of DF1 sections from zero state, coefficients b0 b1 b2 a1 a2 per
 * section in the CMSIS-DSP sign convention:
 * y = b0 x + b1 x[-1] + b2 x[-2] + a1 y[-1] + a2 y[-2] */
void ref_biquad(const double *coef, unsigned nsec, const double *x, unsigned n,
                double *y);

/* Full linear convolution: y has na + nb - 1 samples */
void ref_conv(const double *a, unsigned na, const double *b, unsigned nb,
              double *y);

/* Cross-correlation r[m] = sum_i a[i + lag] b[i], lag = m - (nb - 1),
 * m = 0 .. na + nb - 2 */
void ref_corr(const double *a, unsigned na, const double *b, unsigned nb,
              double *r);

/* Direct DFT, X[k] = sum_j x[j] e^(-+2 pi i jk/n); inverse adds no 1/n */
void ref_dft(const double *re, const double *im, unsigned n, double *ore,
             double *oim, int inverse);

/* One DFT term at any frequency (cycles per sample): sum_j x[j] e^(-2 pi i f j)
 */
void ref_dft_bin(const double *x, unsigned n, double f, double *re, double *im);

/* Cosine-sum window w[k] = sum_j (-1)^j a[j] cos(2 pi j k / d), d = n - 1
 * (symmetric) or n (periodic): Hann {0.5, 0.5}, Hamming {0.54, 0.46}, ... */
void ref_window_cosine(const double *a, unsigned terms, unsigned n,
                       int symmetric, double *w);

/* Upsample by l (zero stuffing), FIR h, keep every m-th output; y holds
 * (n * l + m - 1) / m samples */
void ref_resample(const double *h, unsigned ntaps, unsigned l, unsigned m,
                  const double *x, unsigned n, double *y);

#endif /* REF_DSP_H */
