/**
 * @file fd_bfp.h
 * @brief float blocks <-> q31 mantissas with one block exponent (internal)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FD_BFP_H
#define FD_BFP_H

#include "fx_arch.h"
#include <stdint.h>
#include <string.h>

#define FD_BFP_BAD INT32_MAX

/* e with every |x| < 2^e, at least -125; FD_BFP_BAD for a NaN or inf */
int32_t fd_bfp_exp(const float *x, uint32_t n);

/* x 2^(31 - e), ties to even; needs |x| < 2^e; may be in place */
void fd_bfp_to_q31(const float *x, int32_t *q, uint32_t n, int32_t e);

/* q 2^(e - 31), ties to even; may be in place */
void fd_bfp_to_f32(const int32_t *q, float *y, uint32_t n, int32_t e);

uint32_t fd_bfp_edge(uint32_t sign, uint32_t norm, uint32_t sticky, int32_t bx);

/* a 2^e, |a| < 2^63, ties to even */
FX_INLINE float fd_bfp_f32_of_s64(int64_t a, int32_t e) {
  uint32_t sign = (uint32_t)((uint64_t)a >> 32) & 0x80000000U;
  uint64_t mag = a < 0 ? 0U - (uint64_t)a : (uint64_t)a;
  uint32_t hi = (uint32_t)(mag >> 32);
  uint32_t lo = (uint32_t)mag;
  uint32_t norm;
  uint32_t sticky;
  int32_t bx;
  if (FX_LIKELY(hi != 0U)) {
    uint32_t lz = fx_clz(hi);
    norm = (hi << lz) | (lo >> (32U - lz));
    sticky = (lo << lz) != 0U ? 1U : 0U;
    bx = 190 + e - (int32_t)lz;
  } else {
    if (lo == 0U) {
      return 0.0f;
    }
    uint32_t lz = fx_clz(lo);
    norm = lo << lz;
    sticky = 0U;
    bx = 158 + e - (int32_t)lz;
  }
  uint32_t m = norm >> 8;
  m += (((norm & 0xFFU) | sticky) + (m & 1U) + 0x7FU) >> 8;
  /* a carry to 2^24 bumps the exponent */
  uint32_t b = sign | (((uint32_t)(bx - 1) << 23) + m);
  if (FX_UNLIKELY((uint32_t)(bx - 1) >= 254U)) {
    b = fd_bfp_edge(sign, norm, sticky, bx);
  }
  float f;
  memcpy(&f, &b, sizeof f);
  return f;
}

#endif /* FD_BFP_H */
