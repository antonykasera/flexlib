/**
 * @file fd_fft_f32.c
 * @brief Float FFTs through the q31 transforms, block floating point
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "fd_fft.h"
#include <math.h>

static FX_COLD void spoil(float *x, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    x[i] = NAN;
  }
}

/* m floats into w at their exponent; 0 for a NaN or inf */
static int32_t in(const float *x, int32_t *w, uint32_t m, int32_t *e) {
  *e = fd_bfp_exp(x, m);
  if (FX_UNLIKELY(*e == FD_BFP_BAD)) {
    return 0;
  }
  fd_bfp_to_q31(x, w, m, *e);
  return 1;
}

typedef int32_t (*Q31Fn)(int32_t *w, uint32_t n);

static int32_t c_fwd(int32_t *w, uint32_t n) {
  return fd_cfft_q31((fd_cq31 *)(void *)w, n);
}
static int32_t c_inv(int32_t *w, uint32_t n) {
  return fd_icfft_q31((fd_cq31 *)(void *)w, n);
}

static void fft_run(float *x, int32_t *w, uint32_t n, uint32_t m, Q31Fn f) {
  int32_t e;
  if (!in(x, w, m, &e)) {
    spoil(x, m);
    return;
  }
  fd_bfp_to_f32(w, x, m, f(w, n) + e);
}

void fd_cfft_f32(fd_cf32 *x, uint32_t n, fd_cq31 *work) {
  fft_run((float *)(void *)x, (int32_t *)(void *)work, n, 2U * n, c_fwd);
}

void fd_icfft_f32(fd_cf32 *x, uint32_t n, fd_cq31 *work) {
  fft_run((float *)(void *)x, (int32_t *)(void *)work, n, 2U * n, c_inv);
}

void fd_rfft_f32(float *x, uint32_t n, fm_q31 *work) {
  fft_run(x, work, n, n, fd_rfft_q31);
}

void fd_irfft_f32(float *x, uint32_t n, fm_q31 *work) {
  fft_run(x, work, n, n, fd_irfft_q31);
}
