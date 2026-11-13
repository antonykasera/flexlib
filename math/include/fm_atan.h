/**
 * @file fm_atan.h
 * @brief atan2, asin and acos in Q15 and Q31, as binary angles
 *
 * Angles as in fm_trig.h: a full turn is 2^16 for the Q15 functions and
 * 2^32 for Q31. $atan2(0, 0) = 0$.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_ATAN_H
#define FM_ATAN_H

#include "fm_fixed.h"

/** $atan2(y, x)$ in $[-2^15, 2^15)$, within 0.65 LSB. */
int16_t fm_atan2_q15(fm_q15 y, fm_q15 x);
/** $asin(x)$ in $[-2^14, 2^14]$, within 0.75 LSB. */
int16_t fm_asin_q15(fm_q15 x);
/** $acos(x)$ in $[0, 2^15]$, within 0.75 LSB. */
uint16_t fm_acos_q15(fm_q15 x);

/** $atan2(y, x)$ in $[-2^31, 2^31)$, within 0.65 LSB. */
int32_t fm_atan2_q31(fm_q31 y, fm_q31 x);
/** $asin(x)$ in $[-2^30, 2^30]$, within 0.81 LSB. */
int32_t fm_asin_q31(fm_q31 x);
/** $acos(x)$ in $[0, 2^31]$, within 0.81 LSB. */
uint32_t fm_acos_q31(fm_q31 x);

#endif /* FM_ATAN_H */
