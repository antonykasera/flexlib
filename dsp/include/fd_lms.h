/**
 * @file fd_lms.h
 * @brief LMS adaptive filters: plain, normalised and sign-error, q15 / q31
 *
 * Weights are Q30 in 32 bits for both formats. The output is the exact sum
 * of the taps times the samples, rounded once and saturated; $e = d - y$,
 * saturated. The state holds the history plus a chunk of any size.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_LMS_H
#define FD_LMS_H

#include "fm_fixed.h"
#include <stdint.h>

/** Weight update. */
typedef enum {
  FD_LMS_PLAIN, /**< $w += mu e x$ */
  FD_LMS_NORM,  /**< $w += mu e x / x'x$, an exact division per sample */
  FD_LMS_SIGN,  /**< $w += mu sign(e) x$ */
} FD_LmsMode;

/** State length for ntaps taps and a chunk of $chunk >= 1$ samples. */
#define FD_LMS_STATE_LEN(ntaps, chunk) ((ntaps) + (chunk))

/** q15 LMS state; opaque. */
typedef struct {
  /** @privatesection */
  int32_t *w;
  fm_q15 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t pos;
  uint32_t mode;
  fm_q15 mu;
  int32_t step;
  int32_t res;
  uint64_t energy;
} FD_LmsQ15;

/**
 * Set up a filter with zero weights and history.
 *
 * @param f         state
 * @param w         ntaps weights; must outlive f
 * @param ntaps     $>= 1$
 * @param state     history buffer; must outlive f
 * @param state_len $> ntaps$, `FD_LMS_STATE_LEN(ntaps, chunk)`
 * @param mu        step size, $mu >= 0$
 * @param mode      update rule
 */
void fd_lms_init_q15(FD_LmsQ15 *f, int32_t *w, uint32_t ntaps, fm_q15 *state,
                     uint32_t state_len, fm_q15 mu, FD_LmsMode mode);

/** Filter n samples of x and adapt towards d; y and e (may be NULL) get the
 * output and the error. */
void fd_lms_q15(FD_LmsQ15 *f, const fm_q15 *x, const fm_q15 *d, fm_q15 *y,
                fm_q15 *e, uint32_t n);

/** The taps as q15, h[0] on the newest sample; applies the update owed by the
 * last sample. */
void fd_lms_coefs_q15(FD_LmsQ15 *f, fm_q15 *h);

/** Clear the history and keep the weights, with the update owed by the last
 * sample applied. */
void fd_lms_reset_q15(FD_LmsQ15 *f);

/** Change the step size, $mu >= 0$, from the next sample's update on. */
void fd_lms_set_mu_q15(FD_LmsQ15 *f, fm_q15 mu);

/** q31 LMS state; opaque. */
typedef struct {
  /** @privatesection */
  int32_t *w;
  fm_q31 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t pos;
  uint32_t mode;
  fm_q31 mu;
  int32_t step;
  uint64_t energy[2];
} FD_LmsQ31;

/** q31 form of fd_lms_init_q15(). */
void fd_lms_init_q31(FD_LmsQ31 *f, int32_t *w, uint32_t ntaps, fm_q31 *state,
                     uint32_t state_len, fm_q31 mu, FD_LmsMode mode);
/** Filter n samples of x and adapt towards d; y and e (may be NULL) get the
 * output and the error. */
void fd_lms_q31(FD_LmsQ31 *f, const fm_q31 *x, const fm_q31 *d, fm_q31 *y,
                fm_q31 *e, uint32_t n);
/** The taps as q31, h[0] on the newest sample; applies the update owed by the
 * last sample. */
void fd_lms_coefs_q31(FD_LmsQ31 *f, fm_q31 *h);
/** q31 form of fd_lms_reset_q15(). */
void fd_lms_reset_q31(FD_LmsQ31 *f);
/** q31 form of fd_lms_set_mu_q15(). */
void fd_lms_set_mu_q31(FD_LmsQ31 *f, fm_q31 mu);

#endif /* FD_LMS_H */
