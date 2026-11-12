/**
 * @file fm_exp.h
 * @brief Logarithms, exponentials and powers in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FM_EXP_H
#define FM_EXP_H

#include "fm_fixed.h"

/* within 0.501 LSB; x <= 0 gives INT32_MIN */
fm_q16 fm_log2_q16(fm_q16 x);
fm_q16 fm_ln_q16(fm_q16 x);
fm_q16 fm_log10_q16(fm_q16 x);
fm_q16 fm_db_q16(fm_q16 x); /* 20 log10 x */

/* within 0.54 LSB; saturates high, rounds to 0 low */
fm_q16 fm_exp2_q16(fm_q16 x);
fm_q16 fm_exp_q16(fm_q16 x);

/* within 0.65 LSB + |y| 2^-32; x < 0 gives 0, 0^y saturates for y < 0 */
fm_q16 fm_pow_q16(fm_q16 x, fm_q16 y);

#endif /* FM_EXP_H */
