/**
 * @file fnn_convert.h
 * @brief A model's inputs and outputs: q15, q31 and float to int8 and back
 *
 * Your sensor data arrives as q15, q31 or float, and a model's first layer
 * takes int8: the quantise functions convert it, $round(x / s) + z$ for the
 * input's scale s and zero point z. The dequantise functions turn the last
 * layer's int8 outputs back into $s (q - z)$.
 *
 * The scale reaches the integer functions as a multiplier from
 * fnn_quant_from_scale(): for a q15 input pass $2^{-15} / s$, for q31
 * $2^{-31} / s$, for float $1 / s$, and for a q15 output $2^{15} s$. They round
 * as `FNN_ROUNDING` says and saturate.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_CONVERT_H
#define FNN_CONVERT_H

#include "fnn_types.h"
#include <stdint.h>

/** Quantise n q15 values with the multiplier for $2^{-15} / s$. */
void fnn_quantize_q15_s8(const int16_t *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp);

/** Quantise n q31 values with the multiplier for $2^{-31} / s$. */
void fnn_quantize_q31_s8(const int32_t *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp);

/**
 * Quantise n floats with the multiplier for $1 / s$, rounding once from the
 * exact product. Infinities saturate, and a NaN gives -128.
 */
void fnn_quantize_f32_s8(const float *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp);

/** Dequantise n values to q15 with the multiplier for $2^{15} s$. */
void fnn_dequantize_s8_q15(const int8_t *x, int16_t *y, uint32_t n,
                           int32_t mult, int32_t rsh, int32_t zp);

/**
 * Dequantise n values to float: $s (q - z)$, rounded once to nearest, ties to
 * even, exactly as the float multiply gives it.
 *
 * @param x  n values
 * @param y  n floats
 * @param n  any count
 * @param s  the scale, $> 0$ and finite
 * @param zp the zero point
 */
void fnn_dequantize_s8_f32(const int8_t *x, float *y, uint32_t n, float s,
                           int32_t zp);

#endif /* FNN_CONVERT_H */
