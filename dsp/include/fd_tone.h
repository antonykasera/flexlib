/**
 * @file fd_tone.h
 * @brief Goertzel single-bin detection, NCO and recursive oscillator
 *
 * Frequencies are phase steps per sample, a full turn being 2^32.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_TONE_H
#define FD_TONE_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/**
 * @name Goertzel
 * One DFT term at any frequency. Inputs enter the 32-bit state as x 2^shift
 * (q15) or x / 2^shift (q31). Keep n |x| / |sin w| under 2^31 in state
 * units: past it the state wraps.
 * @{
 */

/** Goertzel state; opaque. */
typedef struct {
  /** @privatesection */
  int32_t c;      /* 2 cos w, Q2.30 */
  int32_t cw, sw; /* cos w, sin w */
  int32_t s1, s2;
  uint32_t shift;
} FD_Goertzel;

/** Set up for phase step freq; shift <= 31, and <= 16 for q15 input. */
void fd_goertzel_init(FD_Goertzel *g, uint32_t freq, uint32_t shift);
/** Clear the state for a new block. */
void fd_goertzel_reset(FD_Goertzel *g);
/** Feed n samples. */
void fd_goertzel_q15(FD_Goertzel *g, const fm_q15 *x, uint32_t n);
/** Feed n samples. */
void fd_goertzel_q31(FD_Goertzel *g, const fm_q31 *x, uint32_t n);

/** Sum of x[k] e^(jw (N - 1 - k)) over the N inputs so far, in state
 * units. */
void fd_goertzel_dft(const FD_Goertzel *g, int64_t *re, int64_t *im);

/** Squared magnitude of fd_goertzel_dft(), saturated. */
uint64_t fd_goertzel_power(const FD_Goertzel *g);

/** @} */

/**
 * @name NCO
 * Sine and cosine of a 32-bit phase accumulator: no drift.
 * @{
 */

/** NCO state. */
typedef struct {
  uint32_t phase; /**< the next sample's phase */
  uint32_t inc;   /**< phase step */
  int32_t dinc;   /**< added to inc every sample: a linear chirp */
} FD_Nco;

/** Set up an NCO; dinc 0 for a fixed frequency. */
void fd_nco_init(FD_Nco *o, uint32_t phase, uint32_t inc, int32_t dinc);

/** fd_nco_q31()'s values rounded to q15; s or c may be NULL. */
void fd_nco_q15(FD_Nco *o, fm_q15 *s, fm_q15 *c, uint32_t n);
/** As fm_sin_q31(), within 1.11 LSB; s or c may be NULL. */
void fd_nco_q31(FD_Nco *o, fm_q31 *s, fm_q31 *c, uint32_t n);

/** e^(j phase) as (cos, sin) pairs, fd_nco_q31()'s values rounded. */
void fd_nco_cplx_q15(FD_Nco *o, fd_cq15 *y, uint32_t n);
/** e^(j phase) as (cos, sin) pairs, within 1.11 LSB. */
void fd_nco_cplx_q31(FD_Nco *o, fd_cq31 *y, uint32_t n);

/** @} */

/**
 * @name Recursive oscillator
 * A rotation per sample, renormalised. Over 10^6 samples the amplitude stays
 * within 10^-6 of 1 and the error under 10^-4 of full scale.
 * @{
 */

/** Oscillator state; opaque. */
typedef struct {
  /** @privatesection */
  int32_t c, s;   /* cos, sin of the phase, Q30 */
  int32_t cw, sw; /* the rotation, q31 */
  uint32_t count;
} FD_Osc;

/** Set up an oscillator at phase, stepping by inc. */
void fd_osc_init(FD_Osc *o, uint32_t phase, uint32_t inc);
/** n samples; s or c may be NULL. */
void fd_osc_q15(FD_Osc *o, fm_q15 *s, fm_q15 *c, uint32_t n);
/** n samples; s or c may be NULL. */
void fd_osc_q31(FD_Osc *o, fm_q31 *s, fm_q31 *c, uint32_t n);

/** @} */

#endif /* FD_TONE_H */
