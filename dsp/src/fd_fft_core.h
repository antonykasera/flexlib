/**
 * @file fd_fft_core.h
 * @brief FFT stages and twiddles shared by the complex and real transforms
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FFT_CORE_H
#define FD_FFT_CORE_H

#include "fd_fft.h"
#include "fm_core.h"
#include "fx_arch.h"
#include <stdbool.h>
#include <stdint.h>

/* the quarter-wave sine table, QP + 1 entries in Q31 (2^31 at QP) */
#if FD_FFT_MAX <= 1024U
#define FD_FFT_QP 256U
#define FD_FFT_TAB fm_sin_tab
#else
#define FD_FFT_QP 1024U
extern const uint32_t fd_fft_tab[FD_FFT_QP + 1U];
#define FD_FFT_TAB fd_fft_tab
#endif

/* the drivers' result for an all-zero block */
#define FD_FFT_ZERO INT32_MIN

FX_INLINE bool fd_fft_size_ok(uint32_t n, uint32_t min) {
  return n >= min && n <= FD_FFT_MAX && (n & (n - 1U)) == 0U;
}

/* b with every value in [-2^b, 2^b), from the OR of v ^ (v << 1) */
FX_INLINE uint32_t fd_fft_bits(uint32_t acc) { return 31U - fx_clz(acc | 1U); }

/* log2 of a power of two: shifts, not divides, in the kernels */
FX_INLINE uint32_t lg2(uint32_t n) { return 31U - fx_clz(n); }

/* q15 twiddle from a Q31 table entry */
FX_INLINE int32_t fd_fft_tw15(uint32_t t) {
  return (int32_t)((t + 0x8000U) >> 16);
}

/* n complex values: x is 2n int16 / int32, re then im */
uint32_t fd_fft15_scan(const int16_t *x, uint32_t n);
uint32_t fd_fft31_scan(const int32_t *x, uint32_t n);
/* bit reversal, n >= 4; returns the scan's b of the block */
uint32_t fd_fft15_rev(int16_t *x, uint32_t n);
uint32_t fd_fft31_rev(int32_t *x, uint32_t n);

/* stages: radix-2 pairs, the twiddle-free radix-4 butterflies (j = 0) and the
 * twiddle ones of stage q; r (sh > 0) rounds, l shifts left by -sh */
void fd_fft15_t2r(int16_t *x, uint32_t n, int32_t sh);
void fd_fft15_t2l(int16_t *x, uint32_t n, int32_t sh);
void fd_fft15_t4r(int16_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft15_t4l(int16_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft15_r4(int16_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft15_r4w(int16_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft31_t2r(int32_t *x, uint32_t n, int32_t sh);
void fd_fft31_t2l(int32_t *x, uint32_t n, int32_t sh);
void fd_fft31_t4r(int32_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft31_t4l(int32_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft31_r4(int32_t *x, uint32_t n, uint32_t q, int32_t sh);
void fd_fft31_r4x(int32_t *x, uint32_t n, uint32_t q, int32_t sh);

/* the whole transform; FD_FFT_ZERO for an all-zero block */
int32_t fd_fft15_cfft(int16_t *x, uint32_t n);
int32_t fd_fft31_cfft(int32_t *x, uint32_t n);

/* x[k] <-> x[n - k]: the forward transform's output to the inverse's */
void fd_fft15_flip(int16_t *x, uint32_t n);
void fd_fft31_flip(int32_t *x, uint32_t n);

#endif /* FD_FFT_CORE_H */
