/**
 * @file fd_bfp.c
 * @brief float blocks <-> q31 mantissas with one block exponent, from the bits
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "fx_arch.h"
#include <string.h>

/* memcpy: in place*/
FX_INLINE uint32_t ld(const void *p, uint32_t i) {
  uint32_t b;
  memcpy(&b, (const uint32_t *)p + i, sizeof b);
  return b;
}

FX_INLINE void st(void *p, uint32_t i, uint32_t b) {
  memcpy((uint32_t *)p + i, &b, sizeof b);
}

int32_t fd_bfp_exp(const float *x, uint32_t n) {
  uint32_t top = 0U;
  FX_UNROLL(4)
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t a = ld(x, i) << 1;
    top = a > top ? a : top;
  }
  uint32_t ex = top >> 24;
  if (ex == 255U) {
    return FD_BFP_BAD;
  }
  return (int32_t)(ex + (ex == 0U ? 1U : 0U)) - 126;
}

static FX_COLD uint32_t to_q31_sub(uint32_t b, int32_t e) {
  uint32_t m = b & 0x7FFFFFU;
  int32_t r = e + 118;
  if (r <= 0) {
    return m << (uint32_t)-r;
  }
  uint32_t s = r < 25 ? (uint32_t)r : 25U;
  return (m + (1U << (s - 1U)) - 1U + ((m >> s) & 1U)) >> s;
}

void fd_bfp_to_q31(const float *x, int32_t *q, uint32_t n, int32_t e) {
  FX_UNROLL(2)
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t b = ld(x, i);
    uint32_t ex = (b >> 23) & 0xFFU;
    uint32_t m = (b & 0x7FFFFFU) | 0x800000U;
    int32_t r = e + 119 - (int32_t)ex;
    uint32_t mag;
    if (FX_UNLIKELY(ex == 0U)) {
      mag = to_q31_sub(b, e);
    } else if (r <= 0) {
      mag = m << (uint32_t)-r;
    } else {
      uint32_t s = r < 25 ? (uint32_t)r : 25U;
      mag = (m + (1U << (s - 1U)) - 1U + ((m >> s) & 1U)) >> s;
    }
    uint32_t sgn = (uint32_t)((int32_t)b >> 31);
    st(q, i, (mag ^ sgn) - sgn);
  }
}

FX_COLD uint32_t fd_bfp_edge(uint32_t sign, uint32_t norm, uint32_t sticky,
                             int32_t bx) {
  if (bx > 254) {
    return sign | 0x7F800000U;
  }
  uint32_t s = (uint32_t)(10 - bx);
  if (s > 41U) {
    return sign;
  }
  uint64_t v = ((uint64_t)norm << 1) | sticky;
  return sign |
         (uint32_t)((v + (1ULL << (s - 1U)) - 1U + ((v >> s) & 1U)) >> s);
}

void fd_bfp_to_f32(const int32_t *q, float *y, uint32_t n, int32_t e) {
  FX_UNROLL(2)
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t v = ld(q, i);
    uint32_t s = (uint32_t)((int32_t)v >> 31);
    uint32_t mag = (v ^ s) - s;
    uint32_t lz = fx_clz(mag); /* 32 for 0: masked below */
    uint32_t norm = mag << (lz & 31U);
    int32_t bx = 127 + e - (int32_t)lz;
    uint32_t m = ((norm >> 1) + 0x3FU + ((norm >> 8) & 1U)) >> 7;
    uint32_t b = (v & 0x80000000U) | (((uint32_t)(bx - 1) << 23) + m);
    if (FX_UNLIKELY((uint32_t)(bx - 1) >= 254U) && mag != 0U) {
      b = fd_bfp_edge(v & 0x80000000U, norm, 0U, bx);
    }
    st(y, i, mag != 0U ? b : 0U);
  }
}
