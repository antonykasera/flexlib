/**
 * @file fnn_dense.h
 * @brief Fully connected layers
 *
 * Each output is $sum_i (x_i - z_x) w_{o,i} + b_o$, requantised. The input
 * zero point is folded into the bias once with fnn_fold_bias_s8(), and the
 * kernel then adds plain products. Outputs are bit-exact with TFLite's
 * reference kernel in the rounding mode the build selects.
 *
 * The weights can stay in TFLite's row order, or be reordered once by
 * fnn_dense_pack_s8() for a kernel about a fifth faster.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_DENSE_H
#define FNN_DENSE_H

#include "fnn_types.h"
#include <stdint.h>

/** A fully connected layer. */
typedef struct {
  const int8_t *w;  /**< weights, in rows or packed */
  const int32_t *b; /**< `n_out` biases from fnn_fold_bias_s8() */
  uint32_t n_in;    /**< inputs */
  uint32_t n_out;   /**< outputs */
  uint32_t packed;  /**< 1 when w comes from fnn_dense_pack_s8() */
  FNN_Quant q;      /**< one multiplier, or one per output */
  FNN_Out o;        /**< output zero point and clamp */
} FNN_Dense;

/** Run the layer on `n_in` inputs, writing `n_out` outputs. */
void fnn_dense_s8(const FNN_Dense *l, const int8_t *x, int8_t *y);

/**
 * Reorder `n_out` rows of `n_in` weights for the faster kernel. The order is
 * flexnn's own and may change between versions; keep the rows and pack again.
 *
 * @param w      `n_out` rows of `n_in` weights
 * @param n_in   inputs
 * @param n_out  outputs
 * @param packed the same number of bytes, 4-byte aligned; not w
 */
void fnn_dense_pack_s8(const int8_t *w, uint32_t n_in, uint32_t n_out,
                       int8_t *packed);

/** The order fnn_dense_pack_s8() writes, raised whenever it changes. */
#define FNN_LAYOUT_VERSION 1

/**
 * Fold the input zero point into the biases: `b_out[o] = b[o] - zx sum w[o]`.
 * Wrapping arithmetic keeps every sum the kernel forms exact whenever the
 * layer's true output sum fits int32.
 *
 * @param w     `n_out` rows of `n_in` weights, before packing
 * @param n_in  inputs
 * @param n_out outputs
 * @param b     the model's biases, or NULL for none
 * @param zx    input zero point
 * @param b_out `n_out` folded biases; may be b
 */
void fnn_fold_bias_s8(const int8_t *w, uint32_t n_in, uint32_t n_out,
                      const int32_t *b, int32_t zx, int32_t *b_out);

#endif /* FNN_DENSE_H */
