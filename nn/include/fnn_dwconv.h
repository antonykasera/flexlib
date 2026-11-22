/**
 * @file fnn_dwconv.h
 * @brief Depthwise convolution layers
 *
 * A depthwise layer filters each input channel on its own. With a channel
 * multiplier m, output channel c reads input channel $c / m$:
 * $sum (x - z_x) w + b$ over the filter's rows and columns, requantised per
 * output channel. Taps that fall in the padding count as zero, as in TFLite.
 * Tensors are NHWC and the filter is TFLite's `[kh][kw][c_out]`.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_DWCONV_H
#define FNN_DWCONV_H

#include "fnn_types.h"
#include <stdint.h>

/** A depthwise convolution layer. */
typedef struct {
  const int8_t *w;  /**< filter, `[kh][kw][c_out]` */
  const int32_t *b; /**< `c_out` biases from fnn_dwconv_fold_bias_s8() */
  FNN_Shape in;     /**< input shape */
  FNN_Shape out;    /**< output shape; `out.c` is `in.c` times the multiplier */
  uint32_t kh;      /**< filter rows */
  uint32_t kw;      /**< filter columns */
  uint32_t sh;      /**< vertical stride */
  uint32_t sw;      /**< horizontal stride */
  uint32_t pt;      /**< padding rows above the input */
  uint32_t pl;      /**< padding columns left of it */
  int32_t zx;       /**< input zero point */
  FNN_Quant q;      /**< one multiplier, or one per output channel */
  FNN_Out o;        /**< output zero point and clamp */
} FNN_DwConv;

/**
 * Check a layer's shapes, strides and padding, as fnn_conv_check() does, and
 * that `out.c` is a multiple of `in.c`.
 *
 * @return `FNN_ERR_SHAPE` when they do not fit together
 */
FNN_Status fnn_dwconv_check(const FNN_DwConv *l);

/** Run the layer; y must not be x. */
void fnn_dwconv_s8(const FNN_DwConv *l, const int8_t *x, int8_t *y);

/**
 * Fold the input zero point into the biases:
 * $b_{out}[c] = b[c] - z_x sum w[t][c]$ over the filter's taps t.
 *
 * @param w     `taps` rows of `c` weights, `[kh][kw][c_out]`
 * @param taps  the filter's taps, $kh kw$
 * @param c     output channels
 * @param b     the model's biases, or NULL for none
 * @param zx    input zero point
 * @param b_out `c` folded biases; may be b
 */
void fnn_dwconv_fold_bias_s8(const int8_t *w, uint32_t taps, uint32_t c,
                             const int32_t *b, int32_t zx, int32_t *b_out);

#endif /* FNN_DWCONV_H */
