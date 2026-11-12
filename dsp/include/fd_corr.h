/**
 * @file fd_corr.h
 * @brief Convolution and correlation of q15 / q31 blocks
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_CORR_H
#define FD_CORR_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* Every output is an exact sum over 2^(15 + shift) (q31: 2^(31 + shift)),
 * rounded once (ties up) and saturated; shift <= 16 (q31: 32), and
 * shift = ceil(log2 min(na, nb)) rules out saturation. na, nb >= 1; y
 * overlaps neither input. */

/* y[k] = sum a[i] b[k - i], k = 0 .. na + nb - 2 */
void fd_conv_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 uint32_t shift, fm_q15 *y);
void fd_conv_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 uint32_t shift, fm_q31 *y);

/* outputs first .. first + n - 1 of the same */
void fd_conv_part_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q15 *y);
void fd_conv_part_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q31 *y);

/* y[i] = sum a[j + lag0 + i] b[j] for nlags lags from lag0 (may be negative);
 * terms outside either array are zero. All lags: lag0 = 1 - nb, nlags =
 * na + nb - 1 */
void fd_corr_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q15 *y);
void fd_corr_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q31 *y);

/* circular correlation through q15 FFTs: a[k] 2^e = sum a[(j + k) mod n]
 * b[j], in place in a, b overwritten; n a power of two, 4..FD_FFT_MAX, both
 * 4-byte aligned. With both blocks zero-padded to n >= na + nb - 1 it is the
 * linear correlation: lag k in slot k, lag -k in slot n - k. The error is
 * about 45 dB below the output's power (41 dB at worst on noise, chirps and
 * delays); fd_corr is exact */
int32_t fd_corr_fft_q15(fm_q15 *a, fm_q15 *b, uint32_t n);

/* r[k] 2^e = sum x[j] x[j + k] for k < nlags, each sum rounded once (ties
 * up); e is returned and puts r[0] in [2^30, 2^31). A zero block gives zeros
 * and 0. n, nlags >= 1 */
int32_t fd_autocorr_q15(const fm_q15 *x, uint32_t n, fm_q31 *r, uint32_t nlags);
int32_t fd_autocorr_q31(const fm_q31 *x, uint32_t n, fm_q31 *r, uint32_t nlags);

/* index of the largest value (the first), moved to the vertex of the parabola
 * through it and its neighbours, in Q16.16 (to nearest); peak (may be NULL)
 * gets the value. 1 <= n <= 32768 */
fm_q16 fd_peak_q15(const fm_q15 *y, uint32_t n, fm_q15 *peak);
fm_q16 fd_peak_q31(const fm_q31 *y, uint32_t n, fm_q31 *peak);

/* The order-p predictor x[n] ~ sum a[j - 1] x[n - j], j = 1..p, from r[0..p]:
 * a in Q(31 - ashift), reflection coefficients k (q31, may be NULL), err the
 * remaining error power in r's format (may be NULL). Each step's sums are
 * exact and rounded once. FD_ERR_UNSTABLE: r not positive definite (a
 * reflection coefficient reaches 1); FD_ERR_RANGE: a coefficient passes
 * 2^ashift. On an error a, k and err hold nothing useful. 1 <= p < 64 */
FD_Status fd_levinson_q31(const fm_q31 *r, uint32_t order, uint32_t ashift,
                          fm_q31 *a, fm_q31 *k, fm_q31 *err);

#endif /* FD_CORR_H */
