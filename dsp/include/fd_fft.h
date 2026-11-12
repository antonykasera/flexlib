/**
 * @file fd_fft.h
 * @brief Complex and real FFTs in q15 / q31, block floating point
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FFT_H
#define FD_FFT_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* largest transform: 1024 (no table of its own), 2048 or 4096 (a 4 KB table) */
#ifndef FD_FFT_MAX
#define FD_FFT_MAX 1024U
#endif

/* in place, n a power of two, 2..FD_FFT_MAX: the transform is x[k] 2^e, e
 * returned; each stage rounds its exact result once (ties down) */
int32_t fd_cfft_q15(fd_cq15 *x, uint32_t n);
int32_t fd_cfft_q31(fd_cq31 *x, uint32_t n);

/* the inverse, 1/n included: the signal is x[k] 2^e */
int32_t fd_icfft_q15(fd_cq15 *x, uint32_t n);
int32_t fd_icfft_q31(fd_cq31 *x, uint32_t n);

/* n real samples (4..FD_FFT_MAX, 4-byte aligned) to the n/2 + 1 bins in
 * place: x[0], x[1] = Re X[0], Re X[n/2]; x[2k], x[2k + 1] = X[k] */
int32_t fd_rfft_q15(fm_q15 *x, uint32_t n);
int32_t fd_rfft_q31(fm_q31 *x, uint32_t n);

/* the inverse from that layout, 1/n included: the signal is x[j] 2^e */
int32_t fd_irfft_q15(fm_q15 *x, uint32_t n);
int32_t fd_irfft_q31(fm_q31 *x, uint32_t n);

/* bins from exponent e to exponent to, rounded (ties up) and saturated: to =
 * log2 n gives X / n, the fixed scaling of CMSIS-DSP */
void fd_fft_rescale_q15(fd_cq15 *x, uint32_t n, int32_t e, int32_t to);
void fd_fft_rescale_q31(fd_cq31 *x, uint32_t n, int32_t e, int32_t to);

/* 10 log10 |x 2^e|^2 in Q16.16, 0 dB at |X| = 1.0 (full scale); zero bins:
 * INT32_MIN */
void fd_fft_db_q15(const fd_cq15 *x, int32_t e, fm_q16 *y, uint32_t n);
void fd_fft_db_q31(const fd_cq31 *x, int32_t e, fm_q16 *y, uint32_t n);

#endif /* FD_FFT_H */
