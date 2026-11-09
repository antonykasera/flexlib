/**
 * @file fm_float.c
 * @brief Float functions on integer internals, for cores without an FPU
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "fm_internal.h"
#include <string.h>

#define F32_QNAN 0x7FC00000U

FX_INLINE uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

FX_INLINE float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* sqrt of m 2^(e - 150) as float bits, m8 = m << 8 with m in [2^23, 2^24).
 * floor(sqrt(hi 2^32)) holds 8 bits below the result's LSB, so the nearest is
 * (floor + 128) >> 8 exactly; hi 2^32 has a zero low word, so the remainder
 * step is 32-bit. Its s is the floor or one above, which only changes the
 * result when s & 0xFF == 0x80. */
FX_INLINE uint32_t sqrt_bits(uint32_t m8, int32_t e) {
  uint32_t hi = m8 >> ((uint32_t)e & 1U);
  uint32_t h = sqrt_norm32(hi);
  uint32_t s = (h << 16) + ((hi - h * h) << 15) / h;
  if ((s & 0xFFU) == 0x80U && fx_umull(s, s) > (uint64_t)hi << 32) {
    s--;
  }
  return ((uint32_t)((e + 125) >> 1) << 23) + ((s + 128U) >> 8);
}

/* +-0, subnormals, negatives, inf and NaN */
FX_COLD static float sqrtf_special(uint32_t b) {
  if ((b << 1) == 0U || b == 0x7F800000U) {
    return u2f(b);
  }
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return u2f(b | 0x00400000U);
  }
  if ((int32_t)b < 0) {
    return u2f(F32_QNAN);
  }
  uint32_t lz = fx_clz(b); /* subnormal: b 2^-149, lz >= 9 */
  return u2f(sqrt_bits(b << lz, 9 - (int32_t)lz));
}

float fm_sqrtf(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return sqrtf_special(b);
  }
  return u2f(sqrt_bits((b << 8) | 0x80000000U, (int32_t)(b >> 23)));
}
