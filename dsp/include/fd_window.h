/**
 * @file fd_window.h
 * @brief Window functions, generated at run time for any length
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

/* periodic: the DFT's form (spectra); else symmetric (filter design). Apply a
 * window with fd_mul_q15 / _q31 */

/* the FD_Window types; within 0.5 LSB (q15), 3 LSB (q31) of the exact value */
void fd_window_q15(fm_q15 *w, uint32_t n, FD_Window type, bool periodic);
void fd_window_q31(fm_q31 *w, uint32_t n, FD_Window type, bool periodic);

/* float-computed: within 0.52 LSB (q15), 1.5e-6 (q31); beta, sigma (of the
 * half length), alpha (the tapered fraction, 0..1) as in MATLAB / scipy */
void fd_window_kaiser_q15(fm_q15 *w, uint32_t n, float beta, bool periodic);
void fd_window_kaiser_q31(fm_q31 *w, uint32_t n, float beta, bool periodic);
void fd_window_gauss_q15(fm_q15 *w, uint32_t n, float sigma, bool periodic);
void fd_window_gauss_q31(fm_q31 *w, uint32_t n, float sigma, bool periodic);
void fd_window_tukey_q15(fm_q15 *w, uint32_t n, float alpha, bool periodic);
void fd_window_tukey_q31(fm_q31 *w, uint32_t n, float alpha, bool periodic);

/* coherent (sum w / n) and power (sum w^2 / n) gains, Q16.16, rounded */
void fd_window_gains_q15(const fm_q15 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power);
void fd_window_gains_q31(const fm_q31 *w, uint32_t n, fm_q16 *coherent,
                         fm_q16 *power);

#endif /* FD_WINDOW_H */
