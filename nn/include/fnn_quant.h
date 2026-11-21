/**
 * @file fnn_quant.h
 * @brief Requantisation: turning a layer's int32 sums into int8 outputs
 *
 * Each sum is multiplied by the layer's scale, $mult 2^{-32 - rsh}$, and
 * rounded once from the exact product (twice under `FNN_ROUND_GEMMLOWP`).
 * Then the output zero point is added and the result clamped. Every input has
 * a defined result: where TFLite's arithmetic would overflow int32, flexnn
 * saturates.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_QUANT_H
#define FNN_QUANT_H

#include "fnn_types.h"
#include "fx_arch.h"
#include <stdint.h>

/** @cond */
int32_t fnn_requant_wide(int32_t acc, int32_t mult, int32_t rsh);
/** @endcond */

/**
 * Scale a sum by $mult 2^{-32 - rsh}$ and round it as `FNN_ROUNDING` says.
 *
 * @param acc  any int32
 * @param mult $[0, 2^{31})$
 * @param rsh  $[-31, 30]$; below 0 (a scale of $1/2$ or more) it takes a
 *             slower path
 */
FX_INLINE int32_t fnn_requant(int32_t acc, int32_t mult, int32_t rsh) {
  if (FX_UNLIKELY(rsh < 0)) {
    return fnn_requant_wide(acc, mult, rsh);
  }
  uint32_t s = (uint32_t)rsh;
#if FNN_ROUNDING == FNN_ROUND_GEMMLOWP
  /* gemmlowp's doubling multiply rounds ties up, and its division by
   * 2^(rsh + 1) rounds them away from zero */
  int32_t v = (int32_t)(fx_smlal(1LL << 30, acc, mult) >> 31);
  uint32_t mask = (2U << s) - 1U;
  uint32_t thr = (mask >> 1) + (uint32_t)(v < 0);
  return (v >> (s + 1U)) + (int32_t)(((uint32_t)v & mask) > thr);
#else
  /* starting the sum at half a final step leaves the high word, shifted by
   * rsh, already rounded */
  uint64_t r =
      ((uint64_t)((1U << s) >> 1) << 32) | (s == 0U ? 0x80000000U : 0U);
#if FNN_ROUNDING == FNN_ROUND_AWAY
  /* one less makes negative ties round down */
  r += (uint64_t)(int64_t)(acc >> 31);
#endif
  return (int32_t)(fx_smlal((int64_t)r, acc, mult) >> 32) >> s;
#endif
}

/** Add the zero point to v, which can be any int32, and clamp the result to
 * $[lo, hi]$. */
FX_INLINE int8_t fnn_clamp_s8(int32_t v, int32_t zp, int32_t lo, int32_t hi) {
  v = v < lo - zp ? lo - zp : v;
  v = v > hi - zp ? hi - zp : v;
  return (int8_t)(v + zp);
}

/**
 * Requantise n sums into int8 outputs.
 *
 * @param acc int32 sums, channel-minor: `acc[i]` is channel `i % q->n`
 * @param y   n outputs
 * @param n   a multiple of `q->n`
 * @param q   multipliers
 * @param o   zero point and clamp
 */
void fnn_requant_s8(const int32_t *acc, int8_t *y, uint32_t n,
                    const FNN_Quant *q, const FNN_Out *o);

/**
 * Find the multiplier for a real scale the way TFLite's `QuantizeMultiplier`
 * does. A layer you build by hand then gets TFLite's numbers. Scales below
 * $2^{-32}$ give a zero multiplier and scales of $2^{30}$ or more the largest
 * one. Call it at setup rather than per sample: it uses floating point.
 *
 * @param scale $>= 0$, finite
 * @param mult  the multiplier
 * @param rsh   its shift
 * @return `FNN_ERR_QUANT` for a negative or non-finite scale
 */
FNN_Status fnn_quant_from_scale(double scale, int32_t *mult, int8_t *rsh);

/**
 * Convert TFLite's `(quantized_multiplier, shift)` pair to flexnn's form.
 *
 * @param tfl_mult $[0, 2^{31})$
 * @param tfl_shift $[-31, 30]$
 * @param mult  the multiplier
 * @param rsh   $-1 - shift$
 * @return `FNN_ERR_QUANT` when either is out of range
 */
FNN_Status fnn_quant_from_tflite(int32_t tfl_mult, int32_t tfl_shift,
                                 int32_t *mult, int8_t *rsh);

#endif /* FNN_QUANT_H */
