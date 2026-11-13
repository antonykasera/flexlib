/**
 * @file fd_window.h
 * @brief Window functions, generated at run time for any length
 *
 * `periodic` gives the DFT's form, for spectra; otherwise the window is
 * symmetric, for filter design. Apply one with fd_mul_q15() or fd_mul_q31().
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_WINDOW_H
#define FD_WINDOW_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdbool.h>
#include <stdint.h>

/** n points of a fixed window, within 0.5 LSB. */
void fd_window_q15(fm_q15 *w, uint32_t n, FD_Window type, bool periodic);
/** n points of a fixed window, within 3 LSB. */
void fd_window_q31(fm_q31 *w, uint32_t n, FD_Window type, bool periodic);

/**
 * @name Parametric windows
 * Computed in float, parameters as in MATLAB and scipy; q15 within 0.52 LSB,
 * q31 within 1.5e-6.
 * @{
 */

/** Kaiser window, $beta >= 0$. */
void fd_window_kaiser_q15(fm_q15 *w, uint32_t n, float beta, bool periodic);
/** Kaiser window, $beta >= 0$. */
void fd_window_kaiser_q31(fm_q31 *w, uint32_t n, float beta, bool periodic);
/** Gaussian window; $sigma > 0$, in units of the half length. */
void fd_window_gauss_q15(fm_q15 *w, uint32_t n, float sigma, bool periodic);
/** Gaussian window; $sigma > 0$, in units of the half length. */
void fd_window_gauss_q31(fm_q31 *w, uint32_t n, float sigma, bool periodic);
/** Tukey window; alpha, $0 <= alpha <= 1$, is the tapered fraction. */
void fd_window_tukey_q15(fm_q15 *w, uint32_t n, float alpha, bool periodic);
/** Tukey window; alpha, $0 <= alpha <= 1$, is the tapered fraction. */
void fd_window_tukey_q31(fm_q31 *w, uint32_t n, float alpha, bool periodic);

/** @} */

/** Coherent ($sum w / n$) and power ($sum w^2 / n$) gains in Q16.16,
 * rounded. */
void fd_window_gains_q15(const fm_q15 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power);
/** Coherent ($sum w / n$) and power ($sum w^2 / n$) gains in Q16.16,
 * rounded. */
void fd_window_gains_q31(const fm_q31 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power);

#endif /* FD_WINDOW_H */
