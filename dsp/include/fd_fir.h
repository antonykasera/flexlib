/**
 * @file fd_fir.h
 * @brief FIR filters, decimators, interpolators and windowed-sinc design
 *
 * `h[0]` multiplies the newest sample. Every output is the exact sum rounded
 * once (0.5 LSB) and saturated, for any coefficients: init picks 32-bit (q15)
 * or 64-bit (q31) sums when sum |h| < 2 (an interpolator: each branch's),
 * wider ones otherwise. The state
 * holds the history plus a chunk of any size; a block runs in chunk-sized
 * pieces, and a larger chunk moves the history less often.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_FIR_H
#define FD_FIR_H

#include "fd_types.h"
#include "fm_fixed.h"
#include <stdint.h>

/** State length for ntaps taps and a chunk of `chunk` samples. */
#define FD_FIR_STATE_LEN(ntaps, chunk) ((ntaps) - 1U + (chunk))

/** FIR state; opaque. */
typedef struct {
  /** @privatesection */
  const fm_q15 *h;
  fm_q15 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t w;
  uint32_t wide; /* sum |h| >= 2: 64-bit sums */
} FD_FirQ15;

/** FIR state; opaque. */
typedef struct {
  /** @privatesection */
  const fm_q31 *h;
  fm_q31 *state;
  uint32_t ntaps;
  uint32_t len;
  uint32_t w;
  uint32_t wide; /* sum |h| >= 2: 96-bit sums */
} FD_FirQ31;

/**
 * Set up a filter with zero history.
 *
 * @param f         state
 * @param h         ntaps coefficients; must outlive f
 * @param ntaps     >= 1
 * @param state     history buffer; must outlive f
 * @param state_len >= ntaps, `FD_FIR_STATE_LEN(ntaps, chunk)`
 */
void fd_fir_init_q15(FD_FirQ15 *f, const fm_q15 *h, uint32_t ntaps,
                     fm_q15 *state, uint32_t state_len);
/** q31 form of fd_fir_init_q15(). */
void fd_fir_init_q31(FD_FirQ31 *f, const fm_q31 *h, uint32_t ntaps,
                     fm_q31 *state, uint32_t state_len);
/** Clear the history. */
void fd_fir_reset_q15(FD_FirQ15 *f);
/** Clear the history. */
void fd_fir_reset_q31(FD_FirQ31 *f);
/** Filter n samples. */
void fd_fir_q15(FD_FirQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n);
/** Filter n samples. */
void fd_fir_q31(FD_FirQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n);
/** Filter one sample. */
fm_q15 fd_fir_sample_q15(FD_FirQ15 *f, fm_q15 x);
/** Filter one sample. */
fm_q31 fd_fir_sample_q31(FD_FirQ31 *f, fm_q31 x);

/**
 * @name Decimators
 * Every m-th output of the FIR, for any n.
 * @{
 */

/** Decimator state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ15 fir;
  uint32_t m;
  uint32_t phase;
} FD_FirDecimQ15;

/** Decimator state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ31 fir;
  uint32_t m;
  uint32_t phase;
} FD_FirDecimQ31;

/** fd_fir_init_q15() for a decimator by m >= 1. */
void fd_fir_decim_init_q15(FD_FirDecimQ15 *d, const fm_q15 *h, uint32_t ntaps,
                           uint32_t m, fm_q15 *state, uint32_t state_len);
/** fd_fir_init_q31() for a decimator by m >= 1. */
void fd_fir_decim_init_q31(FD_FirDecimQ31 *d, const fm_q31 *h, uint32_t ntaps,
                           uint32_t m, fm_q31 *state, uint32_t state_len);
/** Decimate n samples; returns the outputs written, n / m when n is a
 * multiple of m. */
uint32_t fd_fir_decim_q15(FD_FirDecimQ15 *d, const fm_q15 *x, fm_q15 *y,
                          uint32_t n);
/** Decimate n samples; returns the outputs written, n / m when n is a
 * multiple of m. */
uint32_t fd_fir_decim_q31(FD_FirDecimQ31 *d, const fm_q31 *x, fm_q31 *y,
                          uint32_t n);

/** @} */

/**
 * @name Interpolators
 * Zero-stuffing by l, then h, run as l polyphase branches.
 * @{
 */

/** State length for an interpolator by l. */
#define FD_FIR_INTERP_STATE_LEN(ntaps, l, chunk)                               \
  (((ntaps) + (l) - 1U) / (l) - 1U + (chunk))

/** Interpolator state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ15 fir; /* over the branches: ntaps = ceil(ntaps / l) */
  uint32_t l;
  uint32_t ntaps;
} FD_FirInterpQ15;

/** Interpolator state; opaque. */
typedef struct {
  /** @privatesection */
  FD_FirQ31 fir;
  uint32_t l;
  uint32_t ntaps;
} FD_FirInterpQ31;

/**
 * Set up an interpolator with zero history.
 *
 * @param p         state
 * @param h         ntaps coefficients at l times the input rate; must
 *                  outlive p
 * @param ntaps     >= 1
 * @param l         >= 1
 * @param state     history buffer; must outlive p
 * @param state_len >= `FD_FIR_INTERP_STATE_LEN(ntaps, l, 1)`
 */
void fd_fir_interp_init_q15(FD_FirInterpQ15 *p, const fm_q15 *h, uint32_t ntaps,
                            uint32_t l, fm_q15 *state, uint32_t state_len);
/** q31 form of fd_fir_interp_init_q15(). */
void fd_fir_interp_init_q31(FD_FirInterpQ31 *p, const fm_q31 *h, uint32_t ntaps,
                            uint32_t l, fm_q31 *state, uint32_t state_len);
/** Interpolate n samples into n l outputs; y must not overlap x. */
void fd_fir_interp_q15(FD_FirInterpQ15 *p, const fm_q15 *x, fm_q15 *y,
                       uint32_t n);
/** Interpolate n samples into n l outputs; y must not overlap x. */
void fd_fir_interp_q31(FD_FirInterpQ31 *p, const fm_q31 *x, fm_q31 *y,
                       uint32_t n);

/** @} */

/**
 * @name Design
 * In float, not timed.
 * @{
 */

/**
 * @brief Windowed-sinc filter with unit gain at DC (lowpass, bandstop),
 * Nyquist (highpass) or the band centre (bandpass).
 *
 * @param h     ntaps coefficients out
 * @param ntaps >= 1; odd for highpass and bandstop
 * @param band  filter type
 * @param f1    cutoff, cycles per sample, 0 < f1 < 0.5
 * @param f2    upper edge for bandpass and bandstop, f1 < f2 < 0.5
 * @param win   window
 * @retval FD_ERR_RANGE a frequency out of range, an even length for
 *                      highpass or bandstop, or a zero gain
 */
FD_Status fd_fir_design(float *h, uint32_t ntaps, FD_Band band, float f1,
                        float f2, FD_Window win);

/** Round to q15; FD_ERR_RANGE when a coefficient saturates. */
FD_Status fd_fir_quantize_q15(const float *h, fm_q15 *q, uint32_t ntaps);
/** Round to q31; FD_ERR_RANGE when a coefficient saturates. */
FD_Status fd_fir_quantize_q31(const float *h, fm_q31 *q, uint32_t ntaps);

/** sum |h| in Q16.16, saturated; 2 or more selects the wide sums. */
fm_q16 fd_fir_gain_q15(const fm_q15 *h, uint32_t ntaps);
/** sum |h| in Q16.16, saturated; 2 or more selects the wide sums. */
fm_q16 fd_fir_gain_q31(const fm_q31 *h, uint32_t ntaps);

/** @} */

#endif /* FD_FIR_H */
