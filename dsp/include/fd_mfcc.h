/**
 * @file fd_mfcc.h
 * @brief Mel bands, their logs and the DCT-II, in integer arithmetic
 *
 * The bands are triangles linear in mel. The design is integer too and gives
 * the same tables on every target.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_MFCC_H
#define FD_MFCC_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/** A mel filter bank; opaque. */
typedef struct {
  /** @privatesection */
  const uint16_t *cnt;
  const uint16_t *w;
  uint32_t bands;
  uint32_t k0;
} FD_Mel;

/**
 * Design the mel bands of an n-point real FFT.
 *
 * @param m     the filter bank
 * @param cnt   room for bands + 1 values; must outlive m
 * @param w     room for $n/2 + 1$ values; must outlive m
 * @param n     FFT length, a power of two from 4 to `FD_FFT_MAX`
 * @param rate  sample rate in Hz, 1 to 64000
 * @param bands 1 to n / 2
 * @param f_lo  lowest frequency in Hz, below f_hi
 * @param f_hi  highest frequency in Hz, at most rate / 2
 * @return `FD_ERR_RANGE` when a parameter is out of range
 */
FD_Status fd_mel_init(FD_Mel *m, uint16_t *cnt, uint16_t *w, uint32_t n,
                      uint32_t rate, uint32_t bands, uint32_t f_lo,
                      uint32_t f_hi);

/** Sum the power of fd_rfft_q15()'s bins x, exponent e, into the bands
 * exactly. Band b is $y_b 2^r$, r the value returned. */
int32_t fd_mel_q15(const FD_Mel *m, const fm_q15 *x, int32_t e, uint64_t *y);

/** $ln(x 2^e)$ in Q16.16 within 0.51 LSB, saturated. x = 0 counts as 1. */
void fd_log_q16(const uint64_t *x, int32_t e, fm_q16 *y, uint32_t n);

/** A DCT-II; opaque. */
typedef struct {
  /** @privatesection */
  const fm_q31 *c;
  uint32_t n;
  uint32_t k;
} FD_Dct;

/**
 * Set up the first k outputs of an n-point orthonormal DCT-II, its table
 * within 1.5 LSB.
 *
 * @param d the transform
 * @param c room for $n k$ values; must outlive d
 * @param n $>= 2$
 * @param k 1 to n
 * @return `FD_ERR_RANGE` when n or k is out of range
 */
FD_Status fd_dct2_init(FD_Dct *d, fm_q31 *c, uint32_t n, uint32_t k);

/** The k outputs in x's format, each exact, rounded once and saturated. */
void fd_dct2_q31(const FD_Dct *d, const fm_q31 *x, fm_q31 *y);

#endif /* FD_MFCC_H */
