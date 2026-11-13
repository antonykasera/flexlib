/**
 * @file fd_types.h
 * @brief flexdsp complex sample types, status codes and filter shapes
 *
 * Complex samples are interleaved `{re, im}` pairs, the CMSIS-DSP layout.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_TYPES_H
#define FD_TYPES_H

#include "fm_fixed.h"

/** Complex q15; one aligned word. */
typedef struct FX_ALIGN(4) {
  fm_q15 re; /**< real part */
  fm_q15 im; /**< imaginary part */
} fd_cq15;

/** Complex q31. */
typedef struct {
  fm_q31 re; /**< real part */
  fm_q31 im; /**< imaginary part */
} fd_cq31;

/** Complex float. */
typedef struct {
  float re; /**< real part */
  float im; /**< imaginary part */
} fd_cf32;

_Static_assert(sizeof(fd_cq15) == 4U, "fd_cq15 is one word");
_Static_assert(sizeof(fd_cq31) == 8U, "fd_cq31 is two words");

/** Result of the init and design functions; processing functions return
 * nothing. */
typedef enum {
  FD_OK = 0,       /**< done */
  FD_ERR_GAIN,     /**< the coefficients' gain exceeds the kernel's headroom */
  FD_ERR_UNSTABLE, /**< quantised poles on or outside the unit circle */
  FD_ERR_RANGE,    /**< a parameter outside its stated range */
} FD_Status;

/** Filter band for the design functions. */
typedef enum {
  FD_LOWPASS,  /**< lowpass */
  FD_HIGHPASS, /**< highpass */
  FD_BANDPASS, /**< bandpass */
  FD_BANDSTOP, /**< bandstop */
} FD_Band;

/** Window shape for fd_window_q15() and the FIR design functions. */
typedef enum {
  FD_WIN_RECT,            /**< rectangular */
  FD_WIN_HANN,            /**< Hann */
  FD_WIN_HAMMING,         /**< Hamming */
  FD_WIN_BLACKMAN,        /**< Blackman */
  FD_WIN_BLACKMAN_HARRIS, /**< 4-term Blackman-Harris, -92 dB sidelobes */
  FD_WIN_NUTTALL,         /**< 4-term Nuttall, continuous first derivative */
  FD_WIN_FLATTOP,         /**< 5-term flat-top, for amplitude readings */
  FD_WIN_BARTLETT,        /**< Bartlett (triangular) */
} FD_Window;

#endif /* FD_TYPES_H */
