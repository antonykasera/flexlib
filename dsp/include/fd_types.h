/**
 * @file fd_types.h
 * @brief flexdsp complex sample types and status codes
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FD_TYPES_H
#define FD_TYPES_H

#include "fm_fixed.h"

/* interleaved re, im (the CMSIS-DSP layout); a q15 pair is one aligned word */
typedef struct FX_ALIGN(4) {
  fm_q15 re;
  fm_q15 im;
} fd_cq15;

typedef struct {
  fm_q31 re;
  fm_q31 im;
} fd_cq31;

typedef struct {
  float re;
  float im;
} fd_cf32;

_Static_assert(sizeof(fd_cq15) == 4U, "fd_cq15 is one word");
_Static_assert(sizeof(fd_cq31) == 8U, "fd_cq31 is two words");

/* from init and design functions; the processing functions return nothing */
typedef enum {
  FD_OK = 0,
  FD_ERR_GAIN,     /* the coefficients' gain exceeds the kernel's headroom */
  FD_ERR_UNSTABLE, /* quantised poles on or outside the unit circle */
  FD_ERR_RANGE,    /* a parameter outside its stated range */
} FD_Status;

typedef enum { FD_LOWPASS, FD_HIGHPASS, FD_BANDPASS, FD_BANDSTOP } FD_Band;

typedef enum {
  FD_WIN_RECT,
  FD_WIN_HANN,
  FD_WIN_HAMMING,
  FD_WIN_BLACKMAN,
  FD_WIN_BLACKMAN_HARRIS, /* 4-term, -92 dB sidelobes */
} FD_Window;

#endif /* FD_TYPES_H */
