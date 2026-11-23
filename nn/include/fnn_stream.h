/**
 * @file fnn_stream.h
 * @brief Causal 1-D layers run one input column at a time
 *
 * A model over a sliding window of a signal, say the last 128 IMU samples, can
 * run on each new sample without redoing the whole window. Each layer keeps
 * the last few columns of its input in an FNN_Stream and computes only the
 * output column the new one completes, one window per layer instead of all of
 * them. The outputs are the batch run's bit for bit.
 *
 * A column is one position along the width, the `c` values of all its
 * channels. fnn_stream_push() appends one to a layer's history and returns the
 * window of the last `n` once `n` have arrived, and then every `stride`. A
 * history longer than the window moves its columns less often.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_STREAM_H
#define FNN_STREAM_H

#include "fnn_conv.h"
#include "fnn_types.h"
#include <stdint.h>

/** A layer's input history; opaque. */
typedef struct {
  /** @privatesection */
  int8_t *buf;
  uint32_t len;
  uint32_t n;
  uint32_t c;
  uint32_t stride;
  uint32_t end;
  uint32_t due;
} FNN_Stream;

/**
 * Set up an empty history.
 *
 * @param s      the history
 * @param buf    `len c` bytes; must outlive s
 * @param len    columns buf holds, $>= n$
 * @param n      columns in a window, $>= 1$
 * @param c      values in a column, $>= 1$
 * @param stride columns between windows, $>= 1$
 * @return `FNN_ERR_SHAPE` when a size is out of range
 */
FNN_Status fnn_stream_init(FNN_Stream *s, int8_t *buf, uint32_t len,
                           uint32_t n, uint32_t c, uint32_t stride);

/** Forget every column. */
void fnn_stream_reset(FNN_Stream *s);

/** Append the column x; returns the window it completes, `n c` values, or
 * NULL. */
const int8_t *fnn_stream_push(FNN_Stream *s, const int8_t *x);

/**
 * Check that a convolution can run on a history: one row, a filter of one
 * row, no padding, and a history whose window and stride are the filter's.
 *
 * @return `FNN_ERR_SHAPE` or `FNN_ERR_RANGE` when the layer fails
 *         fnn_conv_check(), `FNN_ERR_SHAPE` when it does not fit s
 */
FNN_Status fnn_stream_conv1d_check(const FNN_Conv *l, const FNN_Stream *s);

/**
 * Append the column x to the convolution's input and compute the output
 * column it completes.
 *
 * @param l the layer, as the batch run has it
 * @param s its input history
 * @param x the new input column, `in.c` values
 * @param y the output column, `out.c` values; may be x
 * @return 1 when y was written, 0 when x completes no window
 */
uint32_t fnn_stream_conv1d_s8(const FNN_Conv *l, FNN_Stream *s,
                              const int8_t *x, int8_t *y);

#endif /* FNN_STREAM_H */
