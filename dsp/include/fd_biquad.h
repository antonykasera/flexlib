/**
 * @file fd_biquad.h
 * @brief Biquad cascades (DF1) and their design
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FD_BIQUAD_H
#define FD_BIQUAD_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* per section b0 b1 b2 a1 a2, y = b0 x + b1 x1 + b2 x2 + a1 y1 + a2 y2 (CMSIS
 * signs), Q2.30 (q31) or Q2.14 (q15); the b are stored divided by 2^bshift */
#define FD_BIQUAD_STATE_LEN(nsec) (3U * (nsec) + 2U)

typedef struct {
  const fm_q31 *c;
  fm_q31 *state;
  uint32_t nsec;
  uint32_t bshift;
  uint32_t wide; /* a section's sum |c| >= 4: wider sums for it */
  uint32_t ef;
} FD_BiquadQ31;

typedef struct {
  const fm_q15 *c;
  fm_q15 *state;
  uint32_t nsec;
  uint32_t bshift;
  uint32_t wide; /* a section's sum |c| >= 4: wider sums for it */
} FD_BiquadQ15;

/* each section's output is its exact sum rounded once (0.5 LSB, saturated) */
void fd_biquad_init_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                        uint32_t bshift, fm_q31 *state);
void fd_biquad_init_q15(FD_BiquadQ15 *f, const fm_q15 *c, uint32_t nsec,
                        uint32_t bshift, fm_q15 *state);

/* first-order error feedback: rounding noise moved away from DC */
void fd_biquad_init_ef_q31(FD_BiquadQ31 *f, const fm_q31 *c, uint32_t nsec,
                           uint32_t bshift, fm_q31 *state);

void fd_biquad_reset_q31(FD_BiquadQ31 *f);
void fd_biquad_reset_q15(FD_BiquadQ15 *f);
void fd_biquad_q31(FD_BiquadQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n);
void fd_biquad_q15(FD_BiquadQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n);

/* ---- design (float, not timed) ---- */

typedef enum {
  FD_BQ_LOWPASS,
  FD_BQ_HIGHPASS,
  FD_BQ_BANDPASS, /* 0 dB peak */
  FD_BQ_NOTCH,
  FD_BQ_PEAK,
  FD_BQ_LOWSHELF,
  FD_BQ_HIGHSHELF,
  FD_BQ_ALLPASS,
} FD_BiquadType;

/* a section with the poles as a1 = 2 - d1, a2 = d2 - 1: exact near z = 1 */
typedef struct {
  float b0, b1, b2;
  float d1, d2;
} FD_BiquadDesign;

/* RBJ cookbook; f0 in cycles per sample (0, 0.5), q > 0, gain for peak and
 * shelves */
FD_Status fd_biquad_design(FD_BiquadDesign *s, FD_BiquadType type, float f0,
                           float q, float gain_db);

/* Butterworth lowpass or highpass, even order: order / 2 sections */
FD_Status fd_biquad_butter(FD_BiquadDesign *s, uint32_t order, FD_Band band,
                           float f0);

/* rounds, picks the smallest bshift; FD_ERR_UNSTABLE for poles on or past the
 * unit circle after rounding */
FD_Status fd_biquad_quantize_q31(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q31 *c, uint32_t *bshift);
FD_Status fd_biquad_quantize_q15(const FD_BiquadDesign *s, uint32_t nsec,
                                 fm_q15 *c, uint32_t *bshift);

#endif /* FD_BIQUAD_H */
