/**
 * @file fd_fft.h
 * @brief Complex and real FFTs in q15 / q31, block floating point
 *
 * The transforms work in place and return an exponent e: the result is
 * x[k] 2^e in integer units. Each stage scales by what its growth needs and
 * rounds its exact result once (to nearest, ties down); nothing saturates or
 * wraps. An all-zero block gives zeros and e = 0.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FFT_H
#define FD_FFT_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

#ifndef FD_FFT_MAX
/** Largest transform, a power of two from 4 to 4096. Up to 1024 the
 * twiddles come from flexmath's sine table; 2048 and 4096 add a 4 KB table. */
#define FD_FFT_MAX 1024U
#endif

/** Complex FFT, n a power of two from 2 to `FD_FFT_MAX`. */
int32_t fd_cfft_q15(fd_cq15 *x, uint32_t n);
/** Complex FFT, n a power of two from 2 to `FD_FFT_MAX`. */
int32_t fd_cfft_q31(fd_cq31 *x, uint32_t n);

/** Inverse of fd_cfft_q15(), 1/n included: the signal is x[k] 2^e. */
int32_t fd_icfft_q15(fd_cq15 *x, uint32_t n);
/** Inverse of fd_cfft_q31(), 1/n included: the signal is x[k] 2^e. */
int32_t fd_icfft_q31(fd_cq31 *x, uint32_t n);

/** n real samples, n a power of two from 4 to `FD_FFT_MAX`, 4-byte aligned,
 * to the n/2 + 1 bins: x[0] = Re X[0], x[1] = Re X[n/2], then
 * x[2k], x[2k + 1] = X[k]. */
int32_t fd_rfft_q15(fm_q15 *x, uint32_t n);
/** n real samples, n a power of two from 4 to `FD_FFT_MAX`, to the n/2 + 1
 * bins in the fd_rfft_q15() layout. */
int32_t fd_rfft_q31(fm_q31 *x, uint32_t n);

/** Inverse of fd_rfft_q15() from its layout, 1/n included: the signal is
 * x[j] 2^e. */
int32_t fd_irfft_q15(fm_q15 *x, uint32_t n);
/** Inverse of fd_rfft_q31() from its layout, 1/n included: the signal is
 * x[j] 2^e. */
int32_t fd_irfft_q31(fm_q31 *x, uint32_t n);

/** Move n bins from exponent e to exponent to, rounded (ties up) and
 * saturated; to = log2 n gives X / n, the fixed scaling of CMSIS-DSP. */
void fd_fft_rescale_q15(fd_cq15 *x, uint32_t n, int32_t e, int32_t to);
/** Move n bins from exponent e to exponent to, rounded (ties up) and
 * saturated; to = log2 n gives X / n, the fixed scaling of CMSIS-DSP. */
void fd_fft_rescale_q31(fd_cq31 *x, uint32_t n, int32_t e, int32_t to);

/** 10 log10 |x 2^e|^2 in Q16.16, 0 dB at |X| = 1.0 (full scale); a zero bin
 * gives `INT32_MIN`. */
void fd_fft_db_q15(const fd_cq15 *x, int32_t e, fm_q16 *y, uint32_t n);
/** 10 log10 |x 2^e|^2 in Q16.16, 0 dB at |X| = 1.0 (full scale); a zero bin
 * gives `INT32_MIN`. */
void fd_fft_db_q31(const fd_cq31 *x, int32_t e, fm_q16 *y, uint32_t n);

#endif /* FD_FFT_H */
