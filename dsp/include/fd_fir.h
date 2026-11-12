/**
 * @file fd_fir.h
 * @brief FIR filters, decimators, interpolators and windowed-sinc design
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FIR_H
#define FD_FIR_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/* h[0] multiplies the newest sample; outputs exact sums rounded once (0.5 LSB,
 * saturated) for any coefficients. state holds ntaps - 1 + chunk samples. */
#define FD_FIR_STATE_LEN(ntaps, chunk) ((ntaps) - 1U + (chunk))

typedef struct {
  const fm_q15 *h;
  fm_q15 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t w;
  uint32_t wide; /* sum |h| >= 2: 64-bit sums */
} FD_FirQ15;

typedef struct {
  const fm_q31 *h;
  fm_q31 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t w;
  uint32_t wide; /* sum |h| >= 2: 96-bit sums */
} FD_FirQ31;

/* state_len >= ntaps >= 1; a larger chunk moves the history less often */
void fd_fir_init_q15(FD_FirQ15 *f, const fm_q15 *h, uint32_t ntaps,
                     fm_q15 *state, uint32_t state_len);
void fd_fir_init_q31(FD_FirQ31 *f, const fm_q31 *h, uint32_t ntaps,
                     fm_q31 *state, uint32_t state_len);
void fd_fir_reset_q15(FD_FirQ15 *f);
void fd_fir_reset_q31(FD_FirQ31 *f);
void fd_fir_q15(FD_FirQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n);
void fd_fir_q31(FD_FirQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n);
fm_q15 fd_fir_sample_q15(FD_FirQ15 *f, fm_q15 x);
fm_q31 fd_fir_sample_q31(FD_FirQ31 *f, fm_q31 x);

/* ---- decimate by m: every m-th output of the FIR, any n ---- */

typedef struct {
  FD_FirQ15 fir;
  uint32_t m;
  uint32_t phase;
} FD_FirDecimQ15;

typedef struct {
  FD_FirQ31 fir;
  uint32_t m;
  uint32_t phase;
} FD_FirDecimQ31;

void fd_fir_decim_init_q15(FD_FirDecimQ15 *d, const fm_q15 *h, uint32_t ntaps,
                           uint32_t m, fm_q15 *state, uint32_t state_len);
void fd_fir_decim_init_q31(FD_FirDecimQ31 *d, const fm_q31 *h, uint32_t ntaps,
                           uint32_t m, fm_q31 *state, uint32_t state_len);
/* returns the outputs written: n / m when n is a multiple of m */
uint32_t fd_fir_decim_q15(FD_FirDecimQ15 *d, const fm_q15 *x, fm_q15 *y,
                          uint32_t n);
uint32_t fd_fir_decim_q31(FD_FirDecimQ31 *d, const fm_q31 *x, fm_q31 *y,
                          uint32_t n);

/* ---- interpolate by l: zero-stuffing then h, as l polyphase branches ---- */

/* state holds FD_FIR_INTERP_STATE_LEN(ntaps, l, chunk) samples */
#define FD_FIR_INTERP_STATE_LEN(ntaps, l, chunk)                               \
  (((ntaps) + (l) - 1U) / (l) - 1U + (chunk))

typedef struct {
  FD_FirQ15 fir; /* over the branches: ntaps = ceil(ntaps / l) */
  uint32_t l;
  uint32_t ntaps;
} FD_FirInterpQ15;

typedef struct {
  FD_FirQ31 fir;
  uint32_t l;
  uint32_t ntaps;
} FD_FirInterpQ31;

void fd_fir_interp_init_q15(FD_FirInterpQ15 *p, const fm_q15 *h, uint32_t ntaps,
                            uint32_t l, fm_q15 *state, uint32_t state_len);
void fd_fir_interp_init_q31(FD_FirInterpQ31 *p, const fm_q31 *h, uint32_t ntaps,
                            uint32_t l, fm_q31 *state, uint32_t state_len);
/* writes n l outputs; y must not overlap x */
void fd_fir_interp_q15(FD_FirInterpQ15 *p, const fm_q15 *x, fm_q15 *y,
                       uint32_t n);
void fd_fir_interp_q31(FD_FirInterpQ31 *p, const fm_q31 *x, fm_q31 *y,
                       uint32_t n);

/* ---- design (float, not timed) ---- */

/* windowed sinc, f in cycles per sample (0, 0.5), f2 for band types; unit gain
 * at DC, Nyquist or the band centre; highpass / bandstop need odd ntaps */
FD_Status fd_fir_design(float *h, uint32_t ntaps, FD_Band band, float f1,
                        float f2, FD_Window win);

/* rounded to nearest; FD_ERR_RANGE when a coefficient saturates */
FD_Status fd_fir_quantize_q15(const float *h, fm_q15 *q, uint32_t ntaps);
FD_Status fd_fir_quantize_q31(const float *h, fm_q31 *q, uint32_t ntaps);

/* sum |h| in Q16.16 (saturated): >= 2 selects the wide sums */
fm_q16 fd_fir_gain_q15(const fm_q15 *h, uint32_t ntaps);
fm_q16 fd_fir_gain_q31(const fm_q31 *h, uint32_t ntaps);

#endif /* FD_FIR_H */
