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
  uint32_t packed;  /**< 0 for rows, 1 from fnn_dense_pack_s8(),
                        `FNN_PACK_S4` or `FNN_PACK_S2` */
  int32_t zx;       /**< input zero point */
  FNN_Quant q;      /**< one multiplier, or one per output channel */
  FNN_Out o;        /**< output zero point and clamp */
} FNN_Conv;

/** Scratch bytes fnn_conv_s8() needs: one patch. */
#define FNN_CONV_SCRATCH(kh, kw, c_in) ((kh) * (kw) * (c_in))

/** @name Sub-byte weights
 * Weights in [-7, 7] packed two per byte, or in {-1, 0, 1} four per byte, with
 * the outputs the same weights give as int8.
 * @{ */
/** `packed` for int4 weights from fnn_conv_pack_s4(). */
#define FNN_PACK_S4 2U
/** `packed` for ternary weights from fnn_conv_pack_s2(). */
#define FNN_PACK_S2 3U
/** Bytes of `rows` packed int4 rows of n weights. */
#define FNN_S4_BYTES(n, rows) ((((rows) + 7U) / 8U) * (n) * 4U)
/** Bytes of `rows` packed ternary rows of n weights. */
#define FNN_S2_BYTES(n, rows) ((((rows) + 7U) / 8U) * (((n) + 1U) / 2U) * 4U)
/** Scratch bytes fnn_conv_s8() needs with sub-byte weights, 4-byte aligned. */
#define FNN_CONV_SUB_SCRATCH(kh, kw, c_in)                                     \
  (((2U * (kh) * (kw) * (c_in) + 3U) & ~3U) +                                  \
   4U * ((kh) * (kw) * (c_in) + 1U))

/**
 * Pack `rows` rows of n int4 weights for `FNN_PACK_S4`.
 *
 * @param w      `rows` rows of n weights, each in [-7, 7]
 * @param n      weights in a row
 * @param rows   rows
 * @param packed FNN_S4_BYTES(n, rows) bytes, 4-byte aligned
 * @return `FNN_ERR_RANGE` when a weight is out of range
 */
FNN_Status fnn_conv_pack_s4(const int8_t *w, uint32_t n, uint32_t rows,
                            int8_t *packed);

/**
 * Pack `rows` rows of n ternary weights for `FNN_PACK_S2`.
 *
 * @param w      `rows` rows of n weights, each -1, 0 or 1
 * @param n      weights in a row
 * @param rows   rows
 * @param packed FNN_S2_BYTES(n, rows) bytes, 4-byte aligned
 * @return `FNN_ERR_RANGE` when a weight is out of range
 */
FNN_Status fnn_conv_pack_s2(const int8_t *w, uint32_t n, uint32_t rows,
                            int8_t *packed);
/** @} */

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
 * @param scratch FNN_CONV_SCRATCH() bytes, or FNN_CONV_SUB_SCRATCH() bytes,
 *                4-byte aligned, with sub-byte weights
 */
void fnn_conv_s8(const FNN_Conv *l, const int8_t *x, int8_t *y,
                 int8_t *scratch);

#endif /* FNN_CONV_H */
