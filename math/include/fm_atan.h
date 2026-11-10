/**
 * @file fm_atan.h
 * @brief atan2, asin and acos in Q15 and Q31, as binary angles
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_ATAN_H
#define FM_ATAN_H

#include "fm_fixed.h"

/* Binary angles as in fm_trig.h; atan2 within 0.65 LSB, asin/acos 0.75 */
int16_t fm_atan2_q15(fm_q15 y, fm_q15 x);
int16_t fm_asin_q15(fm_q15 x);
uint16_t fm_acos_q15(fm_q15 x);

/* atan2 within 0.65 LSB, asin/acos 0.81; atan2(0, 0) is 0 */
int32_t fm_atan2_q31(fm_q31 y, fm_q31 x);
int32_t fm_asin_q31(fm_q31 x);
uint32_t fm_acos_q31(fm_q31 x);

#endif /* FM_ATAN_H */
