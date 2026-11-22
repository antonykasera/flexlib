/**
 * @file shim.c
 * @brief flexnn's inline functions as real symbols, for the Python check to
 * call
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dense.h"
#include "fnn_quant.h"

int32_t shim_rounding(void);
void shim_dense_s8(const int8_t *x, const int8_t *w, int8_t *packed,
                   uint32_t n_in, uint32_t n_out, const int32_t *b, int32_t zx,
                   double scale, int32_t zy, int32_t lo, int32_t hi,
                   uint32_t use_packed, int32_t *bf, int8_t *y);
int8_t shim_requant_s8(int32_t acc, int32_t mult, int32_t rsh, int32_t zp,
                       int32_t lo, int32_t hi);

int32_t shim_rounding(void) { return FNN_ROUNDING; }

int8_t shim_requant_s8(int32_t acc, int32_t mult, int32_t rsh, int32_t zp,
                       int32_t lo, int32_t hi) {
  return fnn_clamp_s8(fnn_requant(acc, mult, rsh), zp, lo, hi);
}

void shim_dense_s8(const int8_t *x, const int8_t *w, int8_t *packed,
                   uint32_t n_in, uint32_t n_out, const int32_t *b, int32_t zx,
                   double scale, int32_t zy, int32_t lo, int32_t hi,
                   uint32_t use_packed, int32_t *bf, int8_t *y) {
  int32_t mult;
  int8_t rsh;
  (void)fnn_quant_from_scale(scale, &mult, &rsh);
  fnn_fold_bias_s8(w, n_in, n_out, b, zx, bf);
  if (use_packed) {
    fnn_dense_pack_s8(w, n_in, n_out, packed);
  }
  const FNN_Dense l = {
      use_packed ? packed : w, bf,          n_in, n_out, use_packed,
      {&mult, &rsh, 1U},       {zy, lo, hi}};
  fnn_dense_s8(&l, x, y);
}
