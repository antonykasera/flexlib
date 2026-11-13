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

/**
 * @name Logarithms
 * Within 0.501 LSB; $x <= 0$ gives `INT32_MIN`.
 * @{
 */

/** $log2(x)$. */
fm_q16 fm_log2_q16(fm_q16 x);
/** $ln(x)$. */
fm_q16 fm_ln_q16(fm_q16 x);
/** $log10(x)$. */
fm_q16 fm_log10_q16(fm_q16 x);
/** $20 log10(x)$: an amplitude ratio in dB. */
fm_q16 fm_db_q16(fm_q16 x);

/** @} */

/**
 * @name Exponentials
 * Within 0.54 LSB; a result too large saturates, one too small rounds to 0.
 * @{
 */

/** $2^x$. */
fm_q16 fm_exp2_q16(fm_q16 x);
/** $e^x$. */
fm_q16 fm_exp_q16(fm_q16 x);

/** @} */

/** $x^y$, within 0.65 LSB plus $|y| 2^-32$ of the result. $y = 0$ gives 1,
 * $0^0$ included; otherwise $x < 0$ gives 0, and $0^y$ gives 0, or
 * `INT32_MAX` for $y < 0$. */
fm_q16 fm_pow_q16(fm_q16 x, fm_q16 y);

#endif /* FM_EXP_H */
