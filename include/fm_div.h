/**
 * @file fm_div.h
 * @brief Division in Q15, Q31 and Q16.16, and 1/x in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_DIV_H
#define FM_DIV_H

#include "fm_fixed.h"

/* a / b to the nearest LSB (ties away from zero), saturated. a / 0 saturates
 * toward a's sign; 0 / 0 is 0. */
fm_q15 fm_div_q15(fm_q15 a, fm_q15 b);
fm_q31 fm_div_q31(fm_q31 a, fm_q31 b);
fm_q16 fm_div_q16(fm_q16 a, fm_q16 b);

/* 1 / x, rounded the same way; 0 gives INT32_MAX */
fm_q16 fm_recip_q16(fm_q16 x);

#endif /* FM_DIV_H */
