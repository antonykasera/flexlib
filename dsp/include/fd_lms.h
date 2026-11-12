/**
 * @file fd_lms.h
 * @brief LMS adaptive filters: plain, normalised and sign-error, q15 / q31
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_LMS_H
#define FD_LMS_H

#include "fm_fixed.h"
#include <stdint.h>

typedef enum {
  FD_LMS_PLAIN, /* w += mu e x */
  FD_LMS_NORM,  /* w += mu e x / (x'x + 2^-30) */
  FD_LMS_SIGN,  /* w += mu sign(e) x */
} FD_LmsMode;

/* state holds ntaps + chunk samples; a larger chunk moves the history less */
#define FD_LMS_STATE_LEN(ntaps, chunk) ((ntaps) + (chunk))

/* Weights are Q30 in [-1, 1) plus 2^14, so tap k is w[k] >> 15 rounded to
 * nearest; the update adds step x exactly and saturates. The output is the
 * exact sum of the taps times the samples, rounded once and saturated; e = d -
 * y, saturated. step = mu e plus what the last step's rounding left off
 * (NORM: mu e over x'x of the window, at most 1 - 2^-15;
 * SIGN: mu sign(e)), each rounded once. */
typedef struct {
  int32_t *w;
  fm_q15 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t pos;    /* samples in state; the newest at pos - 1 */
  uint32_t mode;   /* FD_LmsMode */
  fm_q15 mu;       /* >= 0 */
  int32_t step;    /* the update owed by the last sample */
  int32_t res;     /* PLAIN: what rounding left off mu e, carried on */
  uint64_t energy; /* NORM: x'x over the last ntaps samples */
} FD_LmsQ15;

/* w holds ntaps weights, set to zero; state_len >= ntaps + 1 */
void fd_lms_init_q15(FD_LmsQ15 *f, int32_t *w, uint32_t ntaps, fm_q15 *state,
                     uint32_t state_len, fm_q15 mu, FD_LmsMode mode);

/* filters x and adapts towards d; y and e (may be NULL) get the output and
 * the error */
void fd_lms_q15(FD_LmsQ15 *f, const fm_q15 *x, const fm_q15 *d, fm_q15 *y,
                fm_q15 *e, uint32_t n);

/* the taps as q15 (h[0] on the newest sample), the owed update applied */
void fd_lms_coefs_q15(FD_LmsQ15 *f, fm_q15 *h);

/* q31: weights Q30 in [-1, 1), updated by round(step x) to Q30 with
 * saturation; the output the exact sum of weights times samples, rounded
 * once and saturated; step = mu e (NORM: over x'x + 2^-62, at most 1 -
 * 2^-31; SIGN: mu sign(e)), rounded once */
typedef struct {
  int32_t *w;
  fm_q31 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t pos;
  uint32_t mode;
  fm_q31 mu; /* >= 0 */
  int32_t step;
  uint64_t energy[2]; /* NORM: x'x, 128 bits (hi, lo) */
} FD_LmsQ31;

void fd_lms_init_q31(FD_LmsQ31 *f, int32_t *w, uint32_t ntaps, fm_q31 *state,
                     uint32_t state_len, fm_q31 mu, FD_LmsMode mode);
void fd_lms_q31(FD_LmsQ31 *f, const fm_q31 *x, const fm_q31 *d, fm_q31 *y,
                fm_q31 *e, uint32_t n);
void fd_lms_coefs_q31(FD_LmsQ31 *f, fm_q31 *h);

#endif /* FD_LMS_H */
