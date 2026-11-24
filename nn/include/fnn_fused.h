/**
 * @file fnn_fused.h
 * @brief Layer pairs run together to save memory
 *
 * Each pair runs a row at a time and never holds its first layer's output
 * whole. The outputs are the two layers' bit for bit.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_FUSED_H
#define FNN_FUSED_H

#include "fnn_conv.h"
#include "fnn_dwconv.h"
#include "fnn_pool.h"
#include "fnn_types.h"
#include <stdint.h>

/** Scratch bytes fnn_dwpw_s8() needs: kh rows of the input and one row of the
 * depthwise output. */
#define FNN_DWPW_SCRATCH(kh, w, c_in, c_dw) ((kh) * (w) * (c_in) + (w) * (c_dw))

/** FNN_DWPW_SCRATCH() and the pointwise layer's scratch, when its weights are
 * sub-byte. */
#define FNN_DWPW_SUB_SCRATCH(kh, w, c_in, c_dw)                                \
  (FNN_DWPW_SCRATCH(kh, w, c_in, c_dw) + 3U + FNN_CONV_SUB_SCRATCH(1U, 1U, c_dw))

/**
 * Check that two layers can run as one: a depthwise layer of stride 1 whose
 * output has its input's size, then a 1 x 1 convolution of stride 1 over it.
 *
 * @return `FNN_ERR_SHAPE` or `FNN_ERR_RANGE` when either layer fails its own
 *         check, `FNN_ERR_SHAPE` when they do not fit together
 */
FNN_Status fnn_dwpw_check(const FNN_DwConv *dw, const FNN_Conv *pw);

/**
 * Run a depthwise layer and the pointwise layer after it.
 *
 * @param dw      the depthwise layer
 * @param pw      the pointwise layer
 * @param x       the input, `dw->in` values
 * @param y       the output, `pw->out` values; may be x when a row of y is no
 *                longer than a row of x
 * @param scratch FNN_DWPW_SCRATCH() bytes, or FNN_DWPW_SUB_SCRATCH() with a
 *                sub-byte pointwise layer; not x or y
 */
void fnn_dwpw_s8(const FNN_DwConv *dw, const FNN_Conv *pw, const int8_t *x,
                 int8_t *y, int8_t *scratch);

/** Scratch bytes fnn_convpool_s8() needs: pkh rows of the convolution's output,
 * w by c_out, and its kh x kw x c_in patch. */
#define FNN_CONVPOOL_SCRATCH(pkh, w, c_out, kh, kw, c_in)                      \
  ((pkh) * (w) * (c_out) + (kh) * (kw) * (c_in))

/**
 * Check that two layers can run as one: a convolution, then a max pool over
 * its output.
 *
 * @return `FNN_ERR_SHAPE` or `FNN_ERR_RANGE` when either layer fails its own
 *         check, `FNN_ERR_RANGE` for sub-byte weights, `FNN_ERR_SHAPE` when
 *         they do not fit together
 */
FNN_Status fnn_convpool_check(const FNN_Conv *c, const FNN_Pool *p);

/**
 * Run a convolution and the max pool after it.
 *
 * @param c       the convolution
 * @param p       the max pool
 * @param x       the input, `c->in` values
 * @param y       the output, `p->out` values; not x
 * @param scratch FNN_CONVPOOL_SCRATCH() bytes; not x or y
 */
void fnn_convpool_s8(const FNN_Conv *c, const FNN_Pool *p, const int8_t *x,
                     int8_t *y, int8_t *scratch);

#endif /* FNN_FUSED_H */
