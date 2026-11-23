/**
 * @file fnn_pool.h
 * @brief Pooling and mean layers
 *
 * A pooling layer slides a window over each channel and keeps the largest
 * value under it, or their average. Taps that fall in the padding are left
 * out, as in TFLite, so a window on the border averages fewer values. The
 * output has the input's scale and zero point. A mean layer averages each
 * channel over the whole input and requantises the result, as TFLite's MEAN
 * over height and width does. Tensors are NHWC.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_POOL_H
#define FNN_POOL_H

#include "fnn_types.h"
#include <stdint.h>

/** A pooling layer. A window the size of the input gives global pooling. */
typedef struct {
  FNN_Shape in;  /**< input shape */
  FNN_Shape out; /**< output shape; `out.c` is `in.c` */
  uint32_t kh;   /**< window rows */
  uint32_t kw;   /**< window columns */
  uint32_t sh;   /**< vertical stride */
  uint32_t sw;   /**< horizontal stride */
  uint32_t pt;   /**< padding rows above the input */
  uint32_t pl;   /**< padding columns left of it */
  int32_t lo;    /**< smallest output, $[-128, hi]$ */
  int32_t hi;    /**< largest output, $[lo, 127]$ */
} FNN_Pool;

/**
 * Check a layer's shapes, strides and padding, as fnn_conv_check() does, and
 * its clamp.
 *
 * @return `FNN_ERR_SHAPE` when the shapes do not fit together, `FNN_ERR_RANGE`
 *         when the clamp is not in int8
 */
FNN_Status fnn_pool_check(const FNN_Pool *p);

/** Keep the largest value under each window, then clamp it. y may be x when
 * pt and pl are 0. */
void fnn_maxpool_s8(const FNN_Pool *p, const int8_t *x, int8_t *y);

/** Average the values under each window, rounding half away from zero as
 * TFLite does, then clamp. y may be x when pt and pl are 0. */
void fnn_avgpool_s8(const FNN_Pool *p, const int8_t *x, int8_t *y);

/** A mean over height and width, with one output per channel. */
typedef struct {
  FNN_Shape in; /**< input shape */
  int32_t zx;   /**< input zero point */
  int32_t mult; /**< from fnn_mean_quant() */
  int8_t rsh;   /**< from fnn_mean_quant() */
  FNN_Out o;    /**< output zero point and clamp */
} FNN_Mean;

/**
 * Find the multiplier that scales a sum of n values to their mean, the way
 * TFLite does: the multiplier for scale, divided by n and truncated.
 *
 * @param scale the input scale over the output scale; $>= 0$, finite
 * @param n     the values in each mean, `in.h in.w`
 * @param mult  the multiplier
 * @param rsh   its shift
 * @return `FNN_ERR_QUANT` for a negative or non-finite scale, `FNN_ERR_SHAPE`
 *         when n is 0
 */
FNN_Status fnn_mean_quant(double scale, uint32_t n, int32_t *mult,
                          int8_t *rsh);

/**
 * Check a mean layer's shape and zero point.
 *
 * @return `FNN_ERR_SHAPE` for an empty input, `FNN_ERR_RANGE` when zx is not
 *         in int8
 */
FNN_Status fnn_mean_check(const FNN_Mean *m);

/** Average each channel over the input and requantise it; y holds `in.c`
 * values. */
void fnn_mean_s8(const FNN_Mean *m, const int8_t *x, int8_t *y);

#endif /* FNN_POOL_H */
