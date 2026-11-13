/**
 * @file fm_sqrt.h
 * @brief Integer and fixed-point square roots, and 1/sqrt in Q16.16
 *
 * The fixed-point roots round to nearest.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_SQRT_H
#define FM_SQRT_H

#include "fm_fixed.h"

/** floor(sqrt(x)), exact. */
uint32_t fm_isqrt32(uint32_t x);
/** floor(sqrt(x)) of a 64-bit value, exact. */
uint32_t fm_isqrt64(uint64_t x);

/** sqrt(x); x <= 0 gives 0. */
fm_q15 fm_sqrt_q15(fm_q15 x);
/** sqrt(x); x <= 0 gives 0. */
fm_q31 fm_sqrt_q31(fm_q31 x);
/** sqrt(x); x <= 0 gives 0. */
fm_q16 fm_sqrt_q16(fm_q16 x);

/** 1 / sqrt(x); x <= 0 gives `INT32_MAX`. */
fm_q16 fm_rsqrt_q16(fm_q16 x);

#endif /* FM_SQRT_H */
