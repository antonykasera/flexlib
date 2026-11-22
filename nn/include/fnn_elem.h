/**
 * @file fnn_elem.h
 * @brief Element-wise layers
 *
 * fnn_add_s8() adds two int8 tensors of the same shape, each with its own
 * scale and zero point, and writes the sum at a third. In
 * `FNN_ROUND_GEMMLOWP` it is TFLite's ADD bit for bit: both inputs are
 * scaled to a common scale, rounded, added, and the sum scaled to the
 * output's and rounded again. The other modes scale both inputs at once and
 * round the exact sum a single time.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_ELEM_H
#define FNN_ELEM_H

#include "fnn_types.h"
#include <stdint.h>

/** An add layer. fnn_add_init() fills it; the multipliers and shifts mean
 * different things in each rounding mode. */
typedef struct {
  int32_t z1;   /**< first input's zero point */
  int32_t z2;   /**< second input's zero point */
  int32_t m[3]; /**< multipliers */
  int8_t sh[3]; /**< their shifts */
  FNN_Out o;    /**< output zero point and clamp */
} FNN_Add;

/**
 * Set up an add layer from its scales, the way TFLite does in
 * `FNN_ROUND_GEMMLOWP`. Call it at setup: it uses floating point.
 *
 * @param a  the layer
 * @param s1 first input's scale; $> 0$, finite
 * @param z1 first input's zero point
 * @param s2 second input's scale; $> 0$, finite
 * @param z2 second input's zero point
 * @param sy output scale; $> 0$, finite
 * @param o  output zero point and clamp
 * @return `FNN_ERR_QUANT` for a scale that is not positive and finite, or an
 *         output scale $2^{19}$ or more times smaller than the larger input
 *         scale; `FNN_ERR_RANGE` for a zero point or clamp outside int8
 */
FNN_Status fnn_add_init(FNN_Add *a, double s1, int32_t z1, double s2,
                        int32_t z2, double sy, FNN_Out o);

/** Add n values of x1 and x2 into y, which may be either input. */
void fnn_add_s8(const FNN_Add *a, const int8_t *x1, const int8_t *x2,
                int8_t *y, uint32_t n);

#endif /* FNN_ELEM_H */
