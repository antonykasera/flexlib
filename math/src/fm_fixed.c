/**
 * @file fm_fixed.c
 * @brief Float <-> fixed-point conversions from the bits, no soft-float
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_fixed.h"
#include <stdbool.h>
#include <string.h>

/* |x| * 2^frac = m << s (s = 0 - r >= 0): NaN -> 0, else clamped */
FX_INLINE int32_t shift_up(uint32_t b, uint32_t m, uint32_t r, int32_t lo,
                           int32_t hi) {
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return 0;
  }
  uint32_t s = 0U - r;
  if (s > 8U) { /* >= 2^31, or inf */
    return (int32_t)b < 0 ? lo : hi;
  }
  uint32_t mag = m << s;
  if ((int32_t)b < 0) {
    return mag > 0U - (uint32_t)lo ? lo : (int32_t)(0U - mag);
  }
  return mag > (uint32_t)hi ? hi : (int32_t)mag;
}

/* x * 2^frac to nearest (ties to even), clamped to [lo, hi] */
FX_INLINE int32_t to_fixed(float x, uint32_t frac, int32_t lo, int32_t hi,
                           bool sat16, bool up_first) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  uint32_t e = (b >> 23) & 0xFFU;
  uint32_t m = (b & 0x7FFFFFU) | 0x800000U; /* subnormals round to 0 below */
  uint32_t r = 150U - frac - e;
  if (up_first && (int32_t)r <= 0) {
    return shift_up(b, m, r, lo, hi);
  }
  if (r - 1U < 24U) {
    int32_t sgn = (int32_t)b >> 31;
    uint32_t mag = (m + (1U << (r - 1U)) - 1U + ((m >> r) & 1U)) >> r;
    int32_t v = ((int32_t)mag ^ sgn) - sgn;
    return sat16 ? FX_SSAT(v, 16) : v;
  }
  if ((int32_t)r > 0) { /* below one half */
    return 0;
  }
  return shift_up(b, m, r, lo, hi);
}

/* q * 2^-frac, rounded to 24 bits as int -> float hardware would */
FX_INLINE float to_float(int32_t q, uint32_t frac) {
  if (q == 0) {
    return 0.0f;
  }
  uint32_t sign = q < 0 ? 0x80000000U : 0U;
  uint32_t mag = q < 0 ? 0U - (uint32_t)q : (uint32_t)q;
  uint32_t lz = fx_clz(mag);
  uint32_t norm = mag << lz; /* top bit set */
  uint32_t m = norm >> 8;
  uint32_t rem = norm & 0xFFU;
  m += (rem > 0x80U ? 1U : 0U) | (rem == 0x80U ? m & 1U : 0U);
  uint32_t e = 127U + 31U - lz - frac;
  /* a mantissa rounded up to 2^24 carries into the exponent by itself */
  uint32_t b = sign | ((e << 23) + m - 0x800000U);
  float f;
  memcpy(&f, &b, sizeof f);
  return f;
}

fm_q15 fm_f32_to_q15(float x) {
  return (fm_q15)to_fixed(x, 15U, -32768, 32767, true, false);
}
fm_q31 fm_f32_to_q31(float x) {
  return to_fixed(x, 31U, INT32_MIN, INT32_MAX, false, true);
}
fm_q16 fm_f32_to_q16(float x) {
  return to_fixed(x, 16U, INT32_MIN, INT32_MAX, false, false);
}
float fm_q15_to_f32(fm_q15 q) { return to_float(q, 15U); }
float fm_q31_to_f32(fm_q31 q) { return to_float(q, 31U); }
float fm_q16_to_f32(fm_q16 q) { return to_float(q, 16U); }
