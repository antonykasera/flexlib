/**
 * @file fnn_conv.h
 * @brief Convolution layers
 *
 * Each output pixel is a fully connected layer over the patch of input under
 * the filter: $sum (x - z_x) w + b$ over the filter's rows, columns and input
 * channels, requantised per output channel. Taps that fall in the padding
 * count as zero. Tensors are NHWC and the filter is TFLite's
 * `[c_out][kh][kw][c_in]`, which is a dense layer's rows of $kh kw c_in$
 * inputs: fnn_fold_bias_s8() and fnn_dense_pack_s8() take it as it is.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_CONV_H
#define FNN_CONV_H

#include "fnn_types.h"
#include <stdint.h>

/** A convolution layer. */
typedef struct {
  const int8_t *w;  /**< filter, `[c_out][kh][kw][c_in]` or packed */
  const int32_t *b; /**< `c_out` biases folded over the whole filter */
  FNN_Shape in;     /**< input shape */
  FNN_Shape out;    /**< output shape; `out.c` is `c_out` */
  uint32_t kh;      /**< filter rows */
  uint32_t kw;      /**< filter columns */
  uint32_t sh;      /**< vertical stride */
  uint32_t sw;      /**< horizontal stride */
  uint32_t pt;      /**< padding rows above the input */
  uint32_t pl;      /**< padding columns left of it */
  uint32_t packed;  /**< 1 when w comes from fnn_dense_pack_s8() */
  int32_t zx;       /**< input zero point */
  FNN_Quant q;      /**< one multiplier, or one per output channel */
  FNN_Out o;        /**< output zero point and clamp */
} FNN_Conv;

/** Scratch bytes fnn_conv_s8() needs: one patch. */
#define FNN_CONV_SCRATCH(kh, kw, c_in) ((kh) * (kw) * (c_in))

/**
 * Check a layer's shapes, strides and padding: the output must be what
 * TFLite's VALID or SAME padding, or any padding below the filter's size,
 * gives for the input.
 *
 * @return `FNN_ERR_SHAPE` when they do not fit together
 */
FNN_Status fnn_conv_check(const FNN_Conv *l);

/**
 * Run the layer.
 *
 * @param l       a layer fnn_conv_check() accepts
 * @param x       the input, `in.h in.w in.c` values
 * @param y       the output, `out.h out.w out.c` values; not x
 * @param scratch FNN_CONV_SCRATCH() bytes
 */
void fnn_conv_s8(const FNN_Conv *l, const int8_t *x, int8_t *y,
                 int8_t *scratch);

#endif /* FNN_CONV_H */
