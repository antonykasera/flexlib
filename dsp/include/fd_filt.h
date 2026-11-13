/**
 * @file fd_filt.h
 * @brief Small filters: DC blocker, one-pole smoother, moving average, medians
 *
 * Outputs are rounded once (0.5 LSB) and saturated; `y` may equal `x`.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FILT_H
#define FD_FILT_H

#include "fm_fixed.h"
#include <stdint.h>

/**
 * @name DC blocker
 * $y = x - x1 + a y1$.
 * @{
 */

/** DC blocker state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q15 a, x1, y1;
} FD_DcBlockQ15;

/** DC blocker state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q31 a, x1, y1;
} FD_DcBlockQ31;

/** Set up with pole $a$, from zero history. */
void fd_dcblock_init_q15(FD_DcBlockQ15 *d, fm_q15 a);
/** Set up with pole $a$, from zero history. */
void fd_dcblock_init_q31(FD_DcBlockQ31 *d, fm_q31 a);
/** Filter n samples. */
void fd_dcblock_q15(FD_DcBlockQ15 *d, const fm_q15 *x, fm_q15 *y, uint32_t n);
/** Filter n samples. */
void fd_dcblock_q31(FD_DcBlockQ31 *d, const fm_q31 *x, fm_q31 *y, uint32_t n);

/** @} */

/**
 * @name One-pole smoother
 * $y += a (x - y)$, $a >= 0$; y never passes x.
 * @{
 */

/** One-pole state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q15 a, y;
} FD_OnePoleQ15;

/** One-pole state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q31 a, y;
} FD_OnePoleQ31;

/** Set up with coefficient $a >= 0$ and output y0. */
void fd_onepole_init_q15(FD_OnePoleQ15 *p, fm_q15 a, fm_q15 y0);
/** Set up with coefficient $a >= 0$ and output y0. */
void fd_onepole_init_q31(FD_OnePoleQ31 *p, fm_q31 a, fm_q31 y0);
/** Filter n samples. */
void fd_onepole_q15(FD_OnePoleQ15 *p, const fm_q15 *x, fm_q15 *y, uint32_t n);
/** Filter n samples. */
void fd_onepole_q31(FD_OnePoleQ31 *p, const fm_q31 *x, fm_q31 *y, uint32_t n);

/** a for a -3 dB corner at fc cycles per sample, $0 < fc < 0.5$ (float). */
fm_q15 fd_onepole_coef_q15(float fc);
/** a for a -3 dB corner at fc cycles per sample, $0 < fc < 0.5$ (float). */
fm_q31 fd_onepole_coef_q31(float fc);

/** @} */

/**
 * @name Moving average
 * Mean of the last n inputs, zeros before the first, from an exact running
 * sum. A power-of-two n divides by shifting.
 * @{
 */

/** Moving average state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q15 *buf;
  uint32_t n, pos, shift;
  int32_t sum;
} FD_MovAvgQ15;

/** Moving average state; opaque. */
typedef struct {
  /** @privatesection */
  fm_q31 *buf;
  uint32_t n, pos, shift;
  int64_t sum;
} FD_MovAvgQ31;

/** Set up over n samples, $1 <= n <= 65536$; buf holds n and must outlive m. */
void fd_movavg_init_q15(FD_MovAvgQ15 *m, fm_q15 *buf, uint32_t n);
/** Set up over $n >= 1$ samples; buf holds n and must outlive m. */
void fd_movavg_init_q31(FD_MovAvgQ31 *m, fm_q31 *buf, uint32_t n);
/** Filter n samples. */
void fd_movavg_q15(FD_MovAvgQ15 *m, const fm_q15 *x, fm_q15 *y, uint32_t n);
/** Filter n samples. */
void fd_movavg_q31(FD_MovAvgQ31 *m, const fm_q31 *x, fm_q31 *y, uint32_t n);

/** @} */

/**
 * @name Medians
 * Branchless.
 * @{
 */

/** Median of three. */
FX_INLINE int32_t fd_median3(int32_t a, int32_t b, int32_t c) {
  int32_t lo = a < b ? a : b, hi = a < b ? b : a;
  return c < lo ? lo : (c > hi ? hi : c);
}

/** @cond */
#define FD_SWAP(p, q)                                                          \
  do {                                                                         \
    t = (p) < (q) ? (p) : (q);                                                 \
    (q) = (p) < (q) ? (q) : (p);                                               \
    (p) = t;                                                                   \
  } while (0)
/** @endcond */

/** Median of five. */
FX_INLINE int32_t fd_median5(int32_t a, int32_t b, int32_t c, int32_t d,
                             int32_t e) {
  int32_t t;
  FD_SWAP(a, b);
  FD_SWAP(d, e);
  FD_SWAP(a, d); /* a: the least, out */
  FD_SWAP(b, e); /* e: the greatest, out */
  FD_SWAP(b, c);
  FD_SWAP(c, d);
  FD_SWAP(b, c); /* the middle of b, c, d */
  return c;
}
#undef FD_SWAP

/** @} */

#endif /* FD_FILT_H */
