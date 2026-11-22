/**
 * @file fnn_lut.h
 * @brief Activation functions as 256-entry tables
 *
 * An int8 activation has only 256 possible inputs. Any function of one value,
 * tanh, the logistic function, hard-swish or one of your own, then becomes a
 * table you build once and apply with one load per value.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_LUT_H
#define FNN_LUT_H

#include "fnn_types.h"
#include <stdint.h>

/**
 * Fill a table with f, from an input scale and zero point to an output
 * scale and zero point. Each entry is $f(s_x (q - z_x)) / s_y + z_y$ rounded to
 * the nearest integer, ties up, and clamped to int8; a NaN gives -128. It
 * uses floating point: call it at setup.
 *
 * @param f   the function, for example `tanh` from math.h
 * @param s_x input scale, $> 0$
 * @param z_x input zero point
 * @param s_y output scale, $> 0$
 * @param z_y output zero point
 * @param lut 256 entries, the one for input q at `(uint8_t)q`
 * @return `FNN_ERR_QUANT` for a scale that is not positive and finite
 */
FNN_Status fnn_lut_s8_build(double (*f)(double), double s_x, int32_t z_x,
                            double s_y, int32_t z_y, int8_t lut[256]);

/** Map n values through a table; x and y may be the same. The table holds the
 * output for input q at `(uint8_t)q`, the order TFLite uses. */
void fnn_lut_s8(const int8_t *x, int8_t *y, uint32_t n, const int8_t lut[256]);

#endif /* FNN_LUT_H */
