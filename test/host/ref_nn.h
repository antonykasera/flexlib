/**
 * @file ref_nn.h
 * @brief Reference int8 arithmetic, written from the TFLite, gemmlowp and
 * CMSIS-NN formulas in their own `(multiplier, shift)` terms
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REF_NN_H
#define REF_NN_H

#include <stdint.h>

/** The three rounding modes, numbered as in fnn_types.h. */
typedef enum {
  REF_ROUND_UP = 0,
  REF_ROUND_AWAY = 1,
  REF_ROUND_GEMMLOWP = 2
} RefRound;

/* acc mult 2^(shift - 31) rounded by mode, saturated where TFLite's result
 * is undefined */
int32_t ref_requant(int32_t acc, int32_t mult, int32_t shift, RefRound mode);

/* TFLite's QuantizeMultiplier on libm's frexp and round, shift capped at 30 */
void ref_quantize_multiplier(double scale, int32_t *mult, int32_t *shift);

#endif /* REF_NN_H */
