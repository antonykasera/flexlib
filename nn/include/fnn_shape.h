/**
 * @file fnn_shape.h
 * @brief Layers that only move values
 *
 * fnn_concat_s8() joins tensors along one axis. Seen from that axis, each
 * tensor is `outer` rows, and input i adds `len[i]` values to every row of the
 * output. TFLite requires the inputs of an int8 concatenation to share the
 * output's scale and zero point, so the values are copied as they are.
 * Reshaping and flattening move nothing and need no call.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_SHAPE_H
#define FNN_SHAPE_H

#include <stdint.h>

/**
 * Concatenate n tensors. Along the channels of NHWC tensors, outer is
 * `h w` and `len[i]` is input i's channels; along the height, outer is 1 and
 * `len[i]` is all of input i.
 *
 * @param x     n inputs, `outer len[i]` values each
 * @param len   values each input adds to a row
 * @param n     inputs
 * @param outer rows
 * @param y     the output, `outer` times the sum of `len` values; no input
 */
void fnn_concat_s8(const int8_t *const *x, const uint32_t *len, uint32_t n,
                   uint32_t outer, int8_t *y);

#endif /* FNN_SHAPE_H */
