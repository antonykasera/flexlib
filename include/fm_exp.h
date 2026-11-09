/**
 * @file fm_exp.h
 * @brief Logarithms, exponentials and powers in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Logs of x <= 0 give INT32_MIN. Results past the Q16.16 range saturate, and
 * those below half an LSB round to 0.
 */

#ifndef FM_EXP_H
#define FM_EXP_H

#include "fm_fixed.h"

/* within 0.501 LSB on every input (log2 0.5000, ln and log10 0.5001, db
 * 0.5005) */
fm_q16 fm_log2_q16(fm_q16 x);
fm_q16 fm_ln_q16(fm_q16 x);
fm_q16 fm_log10_q16(fm_q16 x);
fm_q16 fm_db_q16(fm_q16 x); /* 20 log10 x, for amplitude ratios */

/* within 0.54 LSB on every input (exp2 0.531, exp 0.537) */
fm_q16 fm_exp2_q16(fm_q16 x);
fm_q16 fm_exp_q16(fm_q16 x);

/* x^y within 0.65 LSB + |y| 2^-32 of the result. x^0 = 1; 0^y is 0 for
 * y > 0 and saturates for y < 0; x < 0 gives 0. */
fm_q16 fm_pow_q16(fm_q16 x, fm_q16 y);

#endif /* FM_EXP_H */
