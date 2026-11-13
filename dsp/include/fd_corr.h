/**
 * @file fd_corr.h
 * @brief Convolution and correlation of q15 / q31 blocks
 *
 * Direct forms: every output is an exact sum over $2^(15 + shift)$ (q31:
 * $2^(31 + shift)$), rounded once (ties up) and saturated. $shift <= 16$
 * (q31: 32), and $shift = ceil(log2 min(na, nb))$ rules out saturation.
 * $na, nb >= 1$; y overlaps neither input.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_CORR_H
#define FD_CORR_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/** $y[k] = sum a[i] b[k - i]$ for the $na + nb - 1$ outputs. */
void fd_conv_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 uint32_t shift, fm_q15 *y);
/** $y[k] = sum a[i] b[k - i]$ for the $na + nb - 1$ outputs. */
void fd_conv_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 uint32_t shift, fm_q31 *y);

/** Outputs first to first + n - 1 of fd_conv_q15(). */
void fd_conv_part_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q15 *y);
/** Outputs first to first + n - 1 of fd_conv_q31(). */
void fd_conv_part_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b,
                      uint32_t nb, uint32_t shift, uint32_t first, uint32_t n,
                      fm_q31 *y);

/** $y[i] = sum a[j + lag0 + i] b[j]$ for nlags lags from lag0, which may be
 * negative; terms outside either array are zero. All lags: $lag0 = 1 - nb$,
 * $nlags = na + nb - 1$. */
void fd_corr_q15(const fm_q15 *a, uint32_t na, const fm_q15 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q15 *y);
/** $y[i] = sum a[j + lag0 + i] b[j]$ for nlags lags from lag0, which may be
 * negative; terms outside either array are zero. All lags: $lag0 = 1 - nb$,
 * $nlags = na + nb - 1$. */
void fd_corr_q31(const fm_q31 *a, uint32_t na, const fm_q31 *b, uint32_t nb,
                 int32_t lag0, uint32_t nlags, uint32_t shift, fm_q31 *y);

/**
 * @brief Circular correlation through q15 FFTs, in place:
 * $a[k] 2^e = sum a[(j + k) mod n] b[j]$.
 *
 * With both blocks zero-padded to $n >= na + nb - 1$ it is the linear
 * correlation, lag k in slot k and lag -k in slot n - k. The error is about
 * 45 dB below the output's power (41 dB at worst on noise, chirps and
 * delays); fd_corr_q15() is exact.
 *
 * @param a n samples, 4-byte aligned; the result out
 * @param b n samples, 4-byte aligned; overwritten
 * @param n a power of two from 4 to `FD_FFT_MAX`
 * @return e
 */
int32_t fd_corr_fft_q15(fm_q15 *a, fm_q15 *b, uint32_t n);

/** $r[k] 2^e = sum x[j] x[j + k]$ for $k < nlags$, each rounded once (ties
 * up); returns e, which puts $r[0]$ in $[2^30, 2^31)$. A zero block gives
 * zeros and 0. $n, nlags >= 1$. */
int32_t fd_autocorr_q15(const fm_q15 *x, uint32_t n, fm_q31 *r, uint32_t nlags);
/** $r[k] 2^e = sum x[j] x[j + k]$ for $k < nlags$, each rounded once (ties
 * up); returns e, which puts $r[0]$ in $[2^30, 2^31)$. A zero block gives
 * zeros and 0. $n, nlags >= 1$. */
int32_t fd_autocorr_q31(const fm_q31 *x, uint32_t n, fm_q31 *r, uint32_t nlags);

/** Index of the first largest value, moved to the vertex of the parabola
 * through it and its neighbours, in Q16.16 (to nearest); peak (may be NULL)
 * gets the value. $1 <= n <= 32768$. */
fm_q16 fd_peak_q15(const fm_q15 *y, uint32_t n, fm_q15 *peak);
/** Index of the first largest value, moved to the vertex of the parabola
 * through it and its neighbours, in Q16.16 (to nearest); peak (may be NULL)
 * gets the value. $1 <= n <= 32768$. */
fm_q16 fd_peak_q31(const fm_q31 *y, uint32_t n, fm_q31 *peak);

/**
 * @brief Levinson-Durbin: the order-p predictor
 * $x[n] ~ sum a[j - 1] x[n - j]$, $j = 1$ to p, from an autocorrelation. Each
 * step's sums are exact and rounded once.
 *
 * @param r      autocorrelation, order + 1 values, as fd_autocorr_q31() gives
 * @param order  p, $1 <= p <= 63$
 * @param ashift a is Q(31 - ashift), $0 <= ashift <= 30$
 * @param a      order coefficients out
 * @param k      order reflection coefficients out, q31; may be NULL
 * @param err    remaining error power in r's format; may be NULL
 * @retval FD_ERR_UNSTABLE r is not positive definite (a reflection
 *                         coefficient reaches 1)
 * @retval FD_ERR_RANGE    a coefficient passes $2^ashift$
 *
 * On an error a, k and err hold nothing useful.
 */
FD_Status fd_levinson_q31(const fm_q31 *r, uint32_t order, uint32_t ashift,
                          fm_q31 *a, fm_q31 *k, fm_q31 *err);

#endif /* FD_CORR_H */
