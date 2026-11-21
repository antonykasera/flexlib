/**
 * @file fd_biquad.h
 * @brief Biquad cascades (DF1) and their design
 *
 * Each section holds b0 b1 b2 a1 a2 with
 * $y = b0 x + b1 x1 + b2 x2 + a1 y1 + a2 y2$ (the CMSIS-DSP order and signs),
 * in Q2.30 (q31) or Q2.14 (q15); the b are stored divided by 2^bshift. Each
 * section's output is its exact sum rounded once (0.5 LSB) and saturated, for
 * any coefficients.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_BIQUAD_H
#define FD_BIQUAD_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/** State length for nsec sections. */
#define FD_BIQUAD_STATE_LEN(nsec) (3U * (nsec) + 2U)

/** q31 cascade state; opaque. */
typedef struct {
  /** @privatesection */
  const fm_q31 *c;
  fm_q31 *state;
  uint32_t nsec;
  uint32_t bshift;
  uint32_t wide; /* a section's sum |c| >= 4: wider sums for it */
  uint32_t ef;
} FD_BiquadQ31;

/** q15 cascade state; opaque. */
typedef struct {
  /** @privatesection */
  const fm_q15 *c;
  fm_q15 *state;
  uint32_t nsec;
  uint32_t bshift;
  uint32_t wide; /* a section's sum |c| >= 4: wider sums for it */
} FD_BiquadQ15;

/**
 * Set up a cascade with zero history.
 *
 * @param f      state
 * @param c      5 nsec coefficients, Q2.30; must outlive f
 * @param nsec   $>= 1$
 * @param bshift $0 <= bshift <= 31$
 * @param state  `FD_BIQUAD_STATE_LEN(nsec)` samples; must outlive f
 */
void fd_biquad_init_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                        uint32_t bshift, fm_q31 *state);
/** q15 form of fd_biquad_init_q31(): Q2.14, $0 <= bshift <= 15$. */
void fd_biquad_init_q15(FD_BiquadQ15 *f, const fm_q15 *c, uint32_t nsec,
                        uint32_t bshift, fm_q15 *state);

/** fd_biquad_init_q31() with first-order error feedback: the rounding noise
 * moves away from DC. */
void fd_biquad_init_ef_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                           uint32_t bshift, fm_q31 *state);

/** Clear the history. */
void fd_biquad_reset_q31(FD_BiquadQ31 *f);
/** Clear the history. */
void fd_biquad_reset_q15(FD_BiquadQ15 *f);
/** Filter n samples. */
void fd_biquad_q31(FD_BiquadQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n);
/** Filter n samples. */
void fd_biquad_q15(FD_BiquadQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n);

/**
 * @name Float
 * Each section's output is its exact sum of the held floats, rounded once:
 * within 0.5 ulp + $2^-32 sum |p|$. A NaN or inf, in or out, makes every later
 * output NaN until reset.
 * @{
 */

/** Float state length for nsec sections, in words. */
#define FD_BIQUAD_F32_STATE_LEN(nsec) (4U * (nsec) + 4U)

/** Float cascade state; opaque. */
typedef struct {
  /** @privatesection */
  const int32_t *cu;
  int32_t *state;
  uint32_t nsec;
  uint32_t bad;
} FD_BiquadF32;

/**
 * Set up a float cascade with zero history.
 *
 * @param f     state
 * @param c     5 nsec coefficients, finite; read only here
 * @param cu    10 nsec words for them unpacked; must outlive f
 * @param nsec  $>= 1$
 * @param state `FD_BIQUAD_F32_STATE_LEN(nsec)` words; must outlive f
 */
void fd_biquad_init_f32(FD_BiquadF32 *f, const float *c, int32_t *cu,
                        uint32_t nsec, int32_t *state);
/** Clear the history. */
void fd_biquad_reset_f32(FD_BiquadF32 *f);
/** Filter n samples; y may be x. */
void fd_biquad_f32(FD_BiquadF32 *f, const float *x, float *y, uint32_t n);

/** @} */

/**
 * @name Design
 * In float, not timed.
 * @{
 */

/** Section shapes for fd_biquad_design(). */
typedef enum {
  FD_BQ_LOWPASS,   /**< lowpass */
  FD_BQ_HIGHPASS,  /**< highpass */
  FD_BQ_BANDPASS,  /**< bandpass, 0 dB peak */
  FD_BQ_NOTCH,     /**< notch */
  FD_BQ_PEAK,      /**< peaking EQ */
  FD_BQ_LOWSHELF,  /**< low shelf */
  FD_BQ_HIGHSHELF, /**< high shelf */
  FD_BQ_ALLPASS,   /**< allpass */
} FD_BiquadType;

/** A designed section, the poles as $a1 = 2 - d1$ and $a2 = d2 - 1$: exact
 * near $z = 1$. */
typedef struct {
  float b0; /**< numerator */
  float b1; /**< numerator */
  float b2; /**< numerator */
  float d1; /**< $2 - a1$ */
  float d2; /**< $a2 + 1$ */
} FD_BiquadDesign;

/**
 * @brief One RBJ cookbook section.
 *
 * @param s       section out
 * @param type    shape
 * @param f0      centre or corner, cycles per sample, $0 < f0 < 0.5$
 * @param q       $> 0$
 * @param gain_db for peak and shelves; ignored otherwise
 * @retval FD_ERR_RANGE f0, q or type out of range
 */
FD_Status fd_biquad_design(FD_BiquadDesign *s, FD_BiquadType type, float f0,
                           float q, float gain_db);

/** Butterworth lowpass or highpass of even order: order / 2 sections, lowest
 * Q first; FD_ERR_RANGE for an odd order or another band. */
FD_Status fd_biquad_butter(FD_BiquadDesign *s, uint32_t order, FD_Band band,
                           float f0);

/**
 * @brief Round nsec sections to Q2.30 and pick the smallest bshift.
 *
 * @param s      nsec designed sections
 * @param nsec   sections
 * @param c      5 nsec coefficients out
 * @param bshift shift for fd_biquad_init_q31() out
 * @retval FD_ERR_UNSTABLE rounding put a pole on or past the unit circle; c
 *                         is written
 * @retval FD_ERR_RANGE    a b needs a shift past 15, or d1, d2 out of range
 */
FD_Status fd_biquad_quantize_q31(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q31 *c, uint32_t *bshift);
/** q15 form of fd_biquad_quantize_q31(): Q2.14. */
FD_Status fd_biquad_quantize_q15(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q15 *c, uint32_t *bshift);

/** @} */

#endif /* FD_BIQUAD_H */
