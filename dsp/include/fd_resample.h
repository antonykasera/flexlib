/**
 * @file fd_resample.h
 * @brief Sample-rate conversion: rational l / m resampling, half-band by 2,
 * CIC decimation
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_RESAMPLE_H
#define FD_RESAMPLE_H

#include "fd_fir.h"
#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/**
 * @name Rational l / m
 * h is a lowpass at l times the input rate with gain l. Outputs within
 * 0.5 LSB, the same bits as fd_fir_interp_q15() by l and then every m-th
 * output.
 * @{
 */

/** State length, as `FD_FIR_INTERP_STATE_LEN`. */
#define FD_RESAMPLE_STATE_LEN(ntaps, l, chunk)                                 \
  FD_FIR_INTERP_STATE_LEN(ntaps, l, chunk)
/** The most outputs n inputs give. */
#define FD_RESAMPLE_OUT_MAX(n, l, m) (((n) * (l) + (m) - 1U) / (m))

/** Resampler state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirInterpQ15 ip;
  uint32_t m;
  uint32_t ph; /* next output at the l-times rate, from the next input */
} FD_ResampleQ15;

/** Resampler state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirInterpQ31 ip;
  uint32_t m;
  uint32_t ph;
} FD_ResampleQ31;

/**
 * Set up a resampler with zero history.
 *
 * @param r         state
 * @param h         ntaps coefficients; must outlive r
 * @param ntaps     $>= 1$
 * @param l         up factor, $l >= 1$
 * @param m         down factor, $m >= 1$
 * @param state     history buffer; must outlive r
 * @param state_len at least `FD_RESAMPLE_STATE_LEN(ntaps, l, 1)`, and
 *                  $l state_len + m < 2^32$
 */
void fd_resample_init_q15(FD_ResampleQ15 *r, const fm_q15 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q15 *state,
                          uint32_t state_len);
/** q31 form of fd_resample_init_q15(). */
void fd_resample_init_q31(FD_ResampleQ31 *r, const fm_q31 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q31 *state,
                          uint32_t state_len);
/** Clear the history. */
void fd_resample_reset_q15(FD_ResampleQ15 *r);
/** Clear the history. */
void fd_resample_reset_q31(FD_ResampleQ31 *r);
/** Resample n samples, any n; returns the outputs written. y must not
 * overlap x. */
uint32_t fd_resample_q15(FD_ResampleQ15 *r, const fm_q15 *x, fm_q15 *y,
                         uint32_t n);
/** Resample n samples, any n; returns the outputs written. y must not
 * overlap x. */
uint32_t fd_resample_q31(FD_ResampleQ31 *r, const fm_q31 *x, fm_q31 *y,
                         uint32_t n);

/** Lowpass prototype for $l / m$ with gain l, cut off at fc ($0 < fc < 1$) of
 * the lower Nyquist; float, not timed. */
FD_Status fd_resample_design(float *h, uint32_t ntaps, uint32_t l, uint32_t m,
                             float fc, FD_Window win);

/** @} */

/**
 * @name Half-band by 2
 * hb holds the 2k taps at odd offsets from the centre, at gain 1: hb[j] for
 * the sample 2j back. The centre (1/2) is implied. Outputs within 0.5 LSB,
 * the same bits as the full 4k - 1 tap filter through fd_fir_decim_q15().
 * @{
 */

/** Decimator state length. */
#define FD_HALFBAND_DECIM_STATE_LEN(k, chunk) (4U * (k) - 2U + (chunk))
/** Interpolator state length. */
#define FD_HALFBAND_INTERP_STATE_LEN(k, chunk) (2U * (k) - 1U + (chunk))

/** Half-band state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ15 fir; /* over the 4k - 1 (interpolator: 2k) sample window */
  uint32_t k;
  uint32_t phase;
} FD_HalfbandQ15;

/** Half-band state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ31 fir;
  uint32_t k;
  uint32_t phase;
} FD_HalfbandQ31;

/**
 * Set up a half-band decimator with zero history.
 *
 * @param d         state
 * @param hb        2k taps; must outlive d
 * @param k         $>= 1$
 * @param state     history buffer; must outlive d
 * @param state_len at least `FD_HALFBAND_DECIM_STATE_LEN(k, 1)`
 */
void fd_halfband_decim_init_q15(FD_HalfbandQ15 *d, const fm_q15 *hb, uint32_t k,
                                fm_q15 *state, uint32_t state_len);
/** q31 form of fd_halfband_decim_init_q15(). */
void fd_halfband_decim_init_q31(FD_HalfbandQ31 *d, const fm_q31 *hb, uint32_t k,
                                fm_q31 *state, uint32_t state_len);
/** fd_halfband_decim_init_q15() for an interpolator; state_len >=
 * `FD_HALFBAND_INTERP_STATE_LEN(k, 1)`. */
void fd_halfband_interp_init_q15(FD_HalfbandQ15 *p, const fm_q15 *hb,
                                 uint32_t k, fm_q15 *state, uint32_t state_len);
/** q31 form of fd_halfband_interp_init_q15(). */
void fd_halfband_interp_init_q31(FD_HalfbandQ31 *p, const fm_q31 *hb,
                                 uint32_t k, fm_q31 *state, uint32_t state_len);
/** Clear the history. */
void fd_halfband_reset_q15(FD_HalfbandQ15 *f);
/** Clear the history. */
void fd_halfband_reset_q31(FD_HalfbandQ31 *f);
/** Decimate n samples: the full filter's 2nd, 4th, ... outputs. Returns the
 * outputs written, n / 2 for even n. */
uint32_t fd_halfband_decim_q15(FD_HalfbandQ15 *d, const fm_q15 *x, fm_q15 *y,
                               uint32_t n);
/** Decimate n samples: the full filter's 2nd, 4th, ... outputs. Returns the
 * outputs written, n / 2 for even n. */
uint32_t fd_halfband_decim_q31(FD_HalfbandQ31 *d, const fm_q31 *x, fm_q31 *y,
                               uint32_t n);
/** Interpolate n samples into 2n: $y[2i] = 2 sum hb[j] x[i - j]$,
 * $y[2i + 1] = x[i - k + 1]$. y must not overlap x. */
void fd_halfband_interp_q15(FD_HalfbandQ15 *p, const fm_q15 *x, fm_q15 *y,
                            uint32_t n);
/** Interpolate n samples into 2n: $y[2i] = 2 sum hb[j] x[i - j]$,
 * $y[2i + 1] = x[i - k + 1]$. y must not overlap x. */
void fd_halfband_interp_q31(FD_HalfbandQ31 *p, const fm_q31 *x, fm_q31 *y,
                            uint32_t n);

/** 2k taps summing to 1/2: gain 1 at DC, 0 at Nyquist; float, not timed. */
FD_Status fd_halfband_design(float *hb, uint32_t k, FD_Window win);

/** @} */

/**
 * @name CIC decimator
 * Integrators and combs, exact in 32- or 64-bit registers sized from the
 * input width. The q31 output is $gain x / 2^(in_bits - 1)$, rounded once
 * (0.5 LSB) only when the registers are wider than 32 bits.
 * @{
 */

/** Highest order. */
#define FD_CIC_MAX_ORDER 6U

/** CIC state; opaque. */
typedef struct {
  /** @privatesection */
  uint32_t order, ratio, count;
  uint32_t wide;  /* 64-bit registers */
  uint32_t shift; /* 32 - bits (left), or bits - 32 (right, wide) */
  uint32_t growth;
  uint64_t integ[FD_CIC_MAX_ORDER], comb[FD_CIC_MAX_ORDER];
} FD_Cic;

/**
 * Set up a decimator with zero registers.
 *
 * @param c       state
 * @param order   1 to `FD_CIC_MAX_ORDER`
 * @param ratio   $>= 1$
 * @param in_bits input width, $1 <= in_bits <= 32$; inputs must fit it
 * @retval FD_ERR_RANGE in_bits plus the growth exceeds 64
 */
FD_Status fd_cic_init(FD_Cic *c, uint32_t order, uint32_t ratio,
                      uint32_t in_bits);
/** Clear the registers. */
void fd_cic_reset(FD_Cic *c);
/** Decimate n samples; returns the outputs written, $n / ratio$ from a fresh
 * start. */
uint32_t fd_cic_decim_q15(FD_Cic *c, const fm_q15 *x, fm_q31 *y, uint32_t n);
/** Decimate n samples; returns the outputs written, $n / ratio$ from a fresh
 * start. */
uint32_t fd_cic_decim_q31(FD_Cic *c, const fm_q31 *x, fm_q31 *y, uint32_t n);
/** The gain left in the output, $ratio^order / 2^growth$ in $(0.5, 1]$,
 * Q2.30 ($2^30 = 1$). */
int32_t fd_cic_gain(const FD_Cic *c);

/** FIR at the output rate: $1 / |CIC|$ up to fc ($0 < fc < 0.5$), 0 above;
 * float, not timed. */
FD_Status fd_cic_comp_design(float *h, uint32_t ntaps, uint32_t order,
                             uint32_t ratio, float fc, FD_Window win);

/** @} */

#endif /* FD_RESAMPLE_H */
