/**
 * @file fnn_softmax.h
 * @brief Softmax and argmax, the end of a classifier
 *
 * A classifier's last layer gives one int8 score per class. fnn_argmax_s8()
 * picks the class, and fnn_softmax_s8() turns the scores into probabilities
 * in TFLite's int8 softmax format: a scale of 1/256 and a zero point of -128.
 * Under `FNN_ROUND_UP` and `FNN_ROUND_AWAY` they are the exact softmax rounded
 * once. Under `FNN_ROUND_GEMMLOWP` and `FNN_ROUND_TFLITE` they are TFLite's
 * integers.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_SOFTMAX_H
#define FNN_SOFTMAX_H

#include "fnn_types.h"
#include <stdint.h>

/**
 * Fill the table of exponentials softmax uses. Entry d is
 * $2^{31} e^{-beta s d}$ rounded to the nearest integer, ties up, or TFLite's
 * fixed-point value of it, 0 past TFLite's input radius. It uses floating
 * point: call it at setup.
 *
 * @param beta    the model's softmax beta, usually 1
 * @param s_x     the input scale
 * @param exp_tab 256 entries
 * @return `FNN_ERR_QUANT` unless $beta s$ is positive and finite, and in
 *         TFLite's arithmetic above $2^{-26}$
 */
FNN_Status fnn_softmax_s8_build(double beta, double s_x, uint32_t exp_tab[256]);

/**
 * Softmax of n scores. With $e_i$ the table entry for $max x - x_i$, output i
 * is $256 e_i / sum e$ rounded once, ties up, less 128 and clamped to 127.
 * That is within half a step plus $(n + 1) 2^{-32}$ of the exact softmax,
 * except that a probability above 255.5 / 256 gives 127. In TFLite's
 * arithmetic it is TFLite's sum, reciprocal and rounding.
 *
 * @param x       n scores
 * @param y       n outputs; may be x
 * @param n       at least 1, and at most 4095 in TFLite's arithmetic
 * @param exp_tab from fnn_softmax_s8_build()
 */
void fnn_softmax_s8(const int8_t *x, int8_t *y, uint32_t n,
                    const uint32_t exp_tab[256]);

/** The index of the largest of n values, n at least 1; the first of equal
 * ones. */
uint32_t fnn_argmax_s8(const int8_t *x, uint32_t n);

#endif /* FNN_SOFTMAX_H */
