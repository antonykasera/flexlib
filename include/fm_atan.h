/**
 * @file fm_atan.h
 * @brief atan2, asin and acos in Q15 and Q31, as binary angles
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Results are in the units fm_trig.h takes: a turn is 2^16 (q15) or 2^32 (q31).
 * atan2 gives (-1/2, 1/2] of a turn, with 1/2 as -2^15 or -2^31 (the same
 * angle); atan2(0, 0) is 0. asin gives [-1/4, 1/4], acos [0, 1/2] unsigned.
 */

#ifndef FM_ATAN_H
#define FM_ATAN_H

#include "fm_fixed.h"

/* atan2 within 0.65 LSB (0.615 on all 2^32 pairs); asin, acos within 0.75
 * (0.747 on all inputs) */
int16_t fm_atan2_q15(fm_q15 y, fm_q15 x);
int16_t fm_asin_q15(fm_q15 x);
uint16_t fm_acos_q15(fm_q15 x);

/* atan2 within 0.65 LSB (0.646 on 2^32 hashed pairs); asin, acos within 0.81
 * (0.800 on all 2^32 inputs) */
int32_t fm_atan2_q31(fm_q31 y, fm_q31 x);
int32_t fm_asin_q31(fm_q31 x);
uint32_t fm_acos_q31(fm_q31 x);

#endif /* FM_ATAN_H */
