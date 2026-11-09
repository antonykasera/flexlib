/**
 * @file fm_fixed.h
 * @brief Fixed point: Q15, Q31 and Q16.16 arithmetic and float conversion
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Results saturate instead of wrapping. Multiplies round to nearest (ties up).
 */

#ifndef FM_FIXED_H
#define FM_FIXED_H

#include "fx_arch.h"
#include <stdint.h>

typedef int16_t fm_q15; /* [-1, 1), 2^-15 steps */
typedef int32_t fm_q31; /* [-1, 1), 2^-31 steps */
typedef int32_t fm_q16; /* [-32768, 32768), 2^-16 steps */

#define FM_Q16_ONE 65536

FX_INLINE fm_q15 fm_sat_q15(int32_t x) { return (fm_q15)FX_SSAT(x, 16); }

FX_INLINE fm_q31 fm_sat_q31(int64_t x) {
  return x > INT32_MAX ? INT32_MAX : (x < INT32_MIN ? INT32_MIN : (int32_t)x);
}

FX_INLINE fm_q15 fm_add_q15(fm_q15 a, fm_q15 b) { return fm_sat_q15(a + b); }
FX_INLINE fm_q15 fm_sub_q15(fm_q15 a, fm_q15 b) { return fm_sat_q15(a - b); }

/* q31 and Q16.16. Without QADD the builtins use the V flag: 3 instructions
 * (ADDS, IT VS, EOR); an overflow saturates toward a's sign. */
FX_INLINE int32_t fm_add_q31(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qadd(a, b);
#else
  int32_t s;
  return __builtin_add_overflow(a, b, &s) ? (a >> 31) ^ INT32_MAX : s;
#endif
}

FX_INLINE int32_t fm_sub_q31(int32_t a, int32_t b) {
#if FX_HAVE_DSP
  return __qsub(a, b);
#else
  int32_t s;
  return __builtin_sub_overflow(a, b, &s) ? (a >> 31) ^ INT32_MAX : s;
#endif
}

FX_INLINE fm_q16 fm_add_q16(fm_q16 a, fm_q16 b) { return fm_add_q31(a, b); }
FX_INLINE fm_q16 fm_sub_q16(fm_q16 a, fm_q16 b) { return fm_sub_q31(a, b); }

FX_INLINE fm_q15 fm_mul_q15(fm_q15 a, fm_q15 b) {
  return fm_sat_q15((a * b + 0x4000) >> 15);
}

/* p >> 62 is 1 only for -1 * -1, the one product past the range */
FX_INLINE fm_q31 fm_mul_q31(fm_q31 a, fm_q31 b) {
  int64_t p = (int64_t)a * b + 0x40000000;
  return (p >> 62) == 1 ? INT32_MAX : (int32_t)(p >> 31);
}

/* in range iff bits 47..63 of the product all equal bit 47, r's sign */
FX_INLINE fm_q16 fm_mul_q16(fm_q16 a, fm_q16 b) {
  int64_t p = (int64_t)a * b + 0x8000;
  int32_t r = (int32_t)(p >> 16);
  int32_t hi = (int32_t)(p >> 47);
  return hi == (r >> 31) ? r : (hi >> 31) ^ INT32_MAX;
}

/* sums of q31 products in Q62 (SMLAL), then rounded back to q31 */
FX_INLINE int64_t fm_mac_q31(int64_t acc, fm_q31 a, fm_q31 b) {
  return acc + (int64_t)a * b;
}

FX_INLINE fm_q31 fm_q62_to_q31(int64_t acc) {
  return fm_sat_q31((acc >> 31) + ((acc >> 30) & 1));
}

/* Exact conversions from the bits, no soft-float. To Q: nearest, ties to
 * even, saturating, NaN -> 0. To float: rounded as a hardware conversion. */
fm_q15 fm_f32_to_q15(float x);
fm_q31 fm_f32_to_q31(float x);
fm_q16 fm_f32_to_q16(float x);
float fm_q15_to_f32(fm_q15 q);
float fm_q31_to_f32(fm_q31 q);
float fm_q16_to_f32(fm_q16 q);

#endif /* FM_FIXED_H */
