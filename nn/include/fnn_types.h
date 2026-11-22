/**
 * @file fnn_types.h
 * @brief Status codes, rounding modes and a layer's output parameters
 *
 * A flexnn tensor is int8 with a scale and a zero point: a value q stands for
 * $s (q - z)$, as in TFLite. Every layer ends by turning its int32 sums
 * into int8: FNN_Quant holds the multiplier of each channel, and FNN_Out the
 * zero point and the clamp.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FNN_TYPES_H
#define FNN_TYPES_H

#include <stdint.h>

/** @name Rounding modes
 * You pick one for the whole build by defining `FNN_ROUNDING`. They only
 * differ in how a sum is requantised, and never by more than 1 in a layer.
 * @{ */
/** Round once, ties up. The default, and what CMSIS-NN does when built with
 * `CMSIS_NN_USE_SINGLE_ROUNDING`. */
#define FNN_ROUND_UP 0
/** Round once, ties away from zero, as the TFLite interpreter's reference
 * fully connected kernel does. */
#define FNN_ROUND_AWAY 1
/** Round twice, the way gemmlowp does: CMSIS-NN's default build, TFLite Micro
 * on Cortex-M, and the TFLite interpreter's reference convolution. */
#define FNN_ROUND_GEMMLOWP 2
/** Round each layer as the TFLite interpreter's reference kernel for it does:
 * fully connected layers as `FNN_ROUND_AWAY`, every other layer as
 * `FNN_ROUND_GEMMLOWP`. A whole model then gives the interpreter's integers. */
#define FNN_ROUND_TFLITE 3
/** @} */

#ifndef FNN_ROUNDING
/** The rounding mode this build uses. */
#define FNN_ROUNDING FNN_ROUND_UP
#endif

/** What the setup and check functions return. Kernels cannot fail and return
 * nothing. */
typedef enum {
  FNN_OK = 0,    /**< done */
  FNN_ERR_SHAPE, /**< a size or shape out of range */
  FNN_ERR_RANGE, /**< a value out of range */
  FNN_ERR_QUANT, /**< a scale or multiplier flexnn cannot represent */
} FNN_Status;

/** A layer's output multipliers, one for the whole tensor or one per
 * channel. Channel c is scaled by $mult_c 2^{-32 - rsh_c}$. */
typedef struct {
  const int32_t *mult; /**< $[0, 2^{31})$, from fnn_quant_from_scale() */
  const int8_t *rsh;   /**< $[-31, 30]$, from fnn_quant_from_scale() */
  uint32_t n;          /**< 1, or the channel count */
} FNN_Quant;

/** The output's zero point and clamp. A fused ReLU raises lo to the zero
 * point, and a ReLU6 also lowers hi. */
typedef struct {
  int32_t zp; /**< output zero point, $[-128, 127]$ */
  int32_t lo; /**< smallest output, $[-128, hi]$ */
  int32_t hi; /**< largest output, $[lo, 127]$ */
} FNN_Out;

/** A tensor's height, width and channels, stored height-major (NHWC). */
typedef struct {
  uint32_t h; /**< rows */
  uint32_t w; /**< columns */
  uint32_t c; /**< channels */
} FNN_Shape;

#endif /* FNN_TYPES_H */
