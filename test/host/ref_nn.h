/**
 * @file ref_nn.h
 * @brief Reference int8 arithmetic, written from the TFLite, gemmlowp and
 * CMSIS-NN formulas in their own `(multiplier, shift)` terms
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef REF_NN_H
#define REF_NN_H

#include <stdint.h>

/** The three rounding modes, numbered as in fnn_types.h. */
typedef enum {
  REF_ROUND_UP = 0,
  REF_ROUND_AWAY = 1,
  REF_ROUND_GEMMLOWP = 2
} RefRound;

/* acc mult 2^(shift - 31) rounded by mode, saturated where TFLite's result
 * is undefined */
int32_t ref_requant(int32_t acc, int32_t mult, int32_t shift, RefRound mode);

/* TFLite's CONV_2D: taps outside the input are skipped. */
void ref_conv2d(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t ic,
                const int8_t *w, uint32_t kh, uint32_t kw, uint32_t sh,
                uint32_t sw, uint32_t pt, uint32_t pl, uint32_t oh, uint32_t ow,
                uint32_t oc, const int32_t *b, int32_t zx, const int32_t *mult,
                const int32_t *shift, int per_channel, int32_t zy, int32_t lo,
                int32_t hi, RefRound mode, int8_t *y);

/* TFLite's DEPTHWISE_CONV_2D: output channel o reads input channel o / m. */
void ref_dwconv(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t ic,
                const int8_t *w, uint32_t kh, uint32_t kw, uint32_t sh,
                uint32_t sw, uint32_t pt, uint32_t pl, uint32_t oh, uint32_t ow,
                uint32_t oc, const int32_t *b, int32_t zx, const int32_t *mult,
                const int32_t *shift, int per_channel, int32_t zy, int32_t lo,
                int32_t hi, RefRound mode, int8_t *y);

/* TFLite's MAX_POOL_2D, or AVERAGE_POOL_2D when avg: taps outside the input
 * are skipped. */
void ref_pool(const int8_t *x, uint32_t ih, uint32_t iw, uint32_t c,
              uint32_t kh, uint32_t kw, uint32_t sh, uint32_t sw, uint32_t pt,
              uint32_t pl, uint32_t oh, uint32_t ow, int avg, int32_t lo,
              int32_t hi, int8_t *y);

/* TFLite's MEAN over height and width, for a scale of s_in / s_out */
void ref_mean(const int8_t *x, uint32_t h, uint32_t w, uint32_t c, int32_t zx,
              double scale, int32_t zy, int32_t lo, int32_t hi, RefRound mode,
              int8_t *y);

/* TFLite's ADD in REF_ROUND_GEMMLOWP; in the other modes both multipliers
 * at the larger one's shift and the exact sum rounded once */
void ref_add(const int8_t *x1, int32_t z1, double s1, const int8_t *x2,
             int32_t z2, double s2, double sy, int32_t zy, int32_t lo,
             int32_t hi, RefRound mode, uint32_t n, int8_t *y);

/* TFLite's QuantizeMultiplier on libm's frexp and round, shift capped at 30 */
void ref_quantize_multiplier(double scale, int32_t *mult, int32_t *shift);

/* TFLite's reference fully connected layer: weights [n_out][n_in], biases or
 * NULL, multipliers and shifts per output when per_channel */
void ref_fully_connected(const int8_t *x, uint32_t n_in, const int8_t *w,
                         uint32_t n_out, const int32_t *b, int32_t zx,
                         const int32_t *mult, const int32_t *shift,
                         int per_channel, int32_t zy, int32_t lo, int32_t hi,
                         RefRound mode, int8_t *y);

#endif /* REF_NN_H */
