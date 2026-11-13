/**
 * @file fm_fixed.h
 * @brief Fixed point: Q15, Q31 and Q16.16 arithmetic and float conversion
 *
 * Results saturate instead of wrapping.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_FIXED_H
#define FM_FIXED_H

#include "fx_arch.h"
#include <stdint.h>

/** Q15: $[-1, 1)$ in steps of 2^-15. */
typedef int16_t fm_q15;
/** Q31: $[-1, 1)$ in steps of 2^-31. */
typedef int32_t fm_q31;
/** Q16.16: $[-32768, 32768)$ in steps of 2^-16. */
typedef int32_t fm_q16;

/** 1.0 in Q16.16. */
#define FM_Q16_ONE 65536

/**
 * @name Saturation
 * @{
 */

/** x clamped to [`INT16_MIN`, `INT16_MAX`]. */
FX_INLINE fm_q15 fm_sat_q15(int32_t x) { return (fm_q15)FX_SSAT(x, 16); }

/** x clamped to [`INT32_MIN`, `INT32_MAX`]. */
FX_INLINE fm_q31 fm_sat_q31(int64_t x) {
  return x > INT32_MAX ? INT32_MAX : (x < INT32_MIN ? INT32_MIN : (int32_t)x);
}

/** @} */

/**
 * @name Add and subtract
 * Saturated.
 * @{
 */

/** $a + b$. */
FX_INLINE fm_q15 fm_add_q15(fm_q15 a, fm_q15 b) { return fm_sat_q15(a + b); }
/** $a - b$. */
FX_INLINE fm_q15 fm_sub_q15(fm_q15 a, fm_q15 b) { return fm_sat_q15(a - b); }

/** $a + b$. */
FX_INLINE fm_q31 fm_add_q31(fm_q31 a, fm_q31 b) {
#if FX_HAVE_DSP
  return __qadd(a, b);
#else
  int32_t s; /* on overflow, saturate toward a's sign */
  return __builtin_add_overflow(a, b, &s) ? (a >> 31) ^ INT32_MAX : s;
#endif
}

/** $a - b$. */
FX_INLINE fm_q31 fm_sub_q31(fm_q31 a, fm_q31 b) {
#if FX_HAVE_DSP
  return __qsub(a, b);
#else
  int32_t s;
  return __builtin_sub_overflow(a, b, &s) ? (a >> 31) ^ INT32_MAX : s;
#endif
}

/** Q16.16 form of fm_add_q31(). */
FX_INLINE fm_q16 fm_add_q16(fm_q16 a, fm_q16 b) { return fm_add_q31(a, b); }
/** Q16.16 form of fm_sub_q31(). */
FX_INLINE fm_q16 fm_sub_q16(fm_q16 a, fm_q16 b) { return fm_sub_q31(a, b); }

/** @} */

/**
 * @name Multiply
 * Rounded to nearest, ties toward +inf, saturated: in Q15 and Q31,
 * $(-1) (-1)$ gives the largest value.
 * @{
 */

/** $a b$. */
FX_INLINE fm_q15 fm_mul_q15(fm_q15 a, fm_q15 b) {
  return fm_sat_q15((a * b + 0x4000) >> 15);
}

/** $a b$. */
FX_INLINE fm_q31 fm_mul_q31(fm_q31 a, fm_q31 b) {
  int64_t p = (int64_t)a * b + 0x40000000;
  /* p >> 62 is 1 only for -1 * -1 */
  return (p >> 62) == 1 ? INT32_MAX : (int32_t)(p >> 31);
}

/** $a b$. */
FX_INLINE fm_q16 fm_mul_q16(fm_q16 a, fm_q16 b) {
  int64_t p = (int64_t)a * b + 0x8000;
  int32_t r = (int32_t)(p >> 16);
  int32_t hi = (int32_t)(p >> 47);
  /* in range iff bits 47..63 of p all equal r's sign */
  return hi == (r >> 31) ? r : (hi >> 31) ^ INT32_MAX;
}

/** @} */

/**
 * @name Accumulate
 * @{
 */

/** $acc + a b$, exact, with acc in Q62. Nothing saturates: the sum must stay
 * in $[-2, 2)$. */
FX_INLINE int64_t fm_mac_q31(int64_t acc, fm_q31 a, fm_q31 b) {
  return acc + (int64_t)a * b;
}

/** A Q62 sum to Q31: rounded to nearest, ties toward +inf, saturated. */
FX_INLINE fm_q31 fm_q62_to_q31(int64_t acc) {
  return fm_sat_q31((acc >> 31) + ((acc >> 30) & 1));
}

/** @} */

/**
 * @name Float conversion
 * To Q: nearest, ties to even, saturated; NaN gives 0. To float: the nearest
 * float, ties to even, which for Q15 is exact.
 * @{
 */

/** float to Q15. */
fm_q15 fm_f32_to_q15(float x);
/** float to Q31. */
fm_q31 fm_f32_to_q31(float x);
/** float to Q16.16. */
fm_q16 fm_f32_to_q16(float x);
/** Q15 to float. */
float fm_q15_to_f32(fm_q15 q);
/** Q31 to float. */
float fm_q31_to_f32(fm_q31 q);
/** Q16.16 to float. */
float fm_q16_to_f32(fm_q16 q);

/** @} */

#endif /* FM_FIXED_H */
