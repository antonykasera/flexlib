/**
 * @file shim.c
 * @brief flexnn's inline functions as real symbols, for the Python check to
 * call
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_quant.h"

int32_t shim_rounding(void);
int8_t shim_requant_s8(int32_t acc, int32_t mult, int32_t rsh, int32_t zp,
                       int32_t lo, int32_t hi);

int32_t shim_rounding(void) { return FNN_ROUNDING; }

int8_t shim_requant_s8(int32_t acc, int32_t mult, int32_t rsh, int32_t zp,
                       int32_t lo, int32_t hi) {
  return fnn_clamp_s8(fnn_requant(acc, mult, rsh), zp, lo, hi);
}
