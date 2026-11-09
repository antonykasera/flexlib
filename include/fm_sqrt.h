/**
 * @file fm_sqrt.h
 * @brief Square roots: integer (exact floor) and fixed point (correctly
 *        rounded), and 1/sqrt in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_SQRT_H
#define FM_SQRT_H

#include "fm_fixed.h"

uint32_t fm_isqrt32(uint32_t x); /* floor(sqrt(x)) */
uint32_t fm_isqrt64(uint64_t x);

/* sqrt to the nearest LSB; negative inputs give 0 */
fm_q15 fm_sqrt_q15(fm_q15 x);
fm_q31 fm_sqrt_q31(fm_q31 x);
fm_q16 fm_sqrt_q16(fm_q16 x);

/* 1/sqrt(x) to the nearest LSB (at most 256.0); x <= 0 gives INT32_MAX */
fm_q16 fm_rsqrt_q16(fm_q16 x);

#endif /* FM_SQRT_H */
