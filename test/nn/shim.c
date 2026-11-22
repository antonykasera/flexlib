/**
 * @file shim.c
 * @brief flexnn's inline functions as real symbols, for the Python check to
 * call
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_conv.h"
#include "fnn_dense.h"
#include "fnn_dwconv.h"
#include "fnn_quant.h"

int32_t shim_rounding(void);
void shim_dense_s8(const int8_t *x, const int8_t *w, int8_t *packed,
                   uint32_t n_in, uint32_t n_out, const int32_t *b, int32_t zx,
                   double scale, int32_t zy, int32_t lo, int32_t hi,
                   uint32_t use_packed, int32_t *bf, int8_t *y);
void shim_conv_s8(const int8_t *x, const int8_t *w, int8_t *packed,
                  const uint32_t *dims, const int32_t *b, int32_t zx,
                  const double *scales, uint32_t nq, int32_t zy, int32_t lo,
                  int32_t hi, uint32_t use_packed, int32_t *bf, int8_t *scratch,
                  int8_t *y);
void shim_dwconv_s8(const int8_t *x, const int8_t *w, const uint32_t *dims,
                    const int32_t *b, int32_t zx, const double *scales,
                    uint32_t nq, int32_t zy, int32_t lo, int32_t hi,
                    int32_t *bf, int8_t *y);
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

/* dims holds the shapes, filter, strides and padding in FNN_Conv's order. */
void shim_conv_s8(const int8_t *x, const int8_t *w, int8_t *packed,
                  const uint32_t *dims, const int32_t *b, int32_t zx,
                  const double *scales, uint32_t nq, int32_t zy, int32_t lo,
                  int32_t hi, uint32_t use_packed, int32_t *bf, int8_t *scratch,
                  int8_t *y) {
  static int32_t mult[256];
  static int8_t rsh[256];
  for (uint32_t c = 0U; c < nq; c++) {
    (void)fnn_quant_from_scale(scales[c], &mult[c], &rsh[c]);
  }
  uint32_t k = dims[6] * dims[7] * dims[2];
  fnn_fold_bias_s8(w, k, dims[5], b, zx, bf);
  if (use_packed) {
    fnn_dense_pack_s8(w, k, dims[5], packed);
  }
  const FNN_Conv l = {use_packed ? packed : w,
                      bf,
                      {dims[0], dims[1], dims[2]},
                      {dims[3], dims[4], dims[5]},
                      dims[6],
                      dims[7],
                      dims[8],
                      dims[9],
                      dims[10],
                      dims[11],
                      use_packed,
                      zx,
                      {mult, rsh, nq},
                      {zy, lo, hi}};
  if (fnn_conv_check(&l) == FNN_OK) {
    fnn_conv_s8(&l, x, y, scratch);
  }
}

/* dims is laid out as for shim_conv_s8. */
void shim_dwconv_s8(const int8_t *x, const int8_t *w, const uint32_t *dims,
                    const int32_t *b, int32_t zx, const double *scales,
                    uint32_t nq, int32_t zy, int32_t lo, int32_t hi,
                    int32_t *bf, int8_t *y) {
  static int32_t mult[256];
  static int8_t rsh[256];
  for (uint32_t c = 0U; c < nq; c++) {
    (void)fnn_quant_from_scale(scales[c], &mult[c], &rsh[c]);
  }
  fnn_dwconv_fold_bias_s8(w, dims[6] * dims[7], dims[5], b, zx, bf);
  const FNN_DwConv l = {w,
                        bf,
                        {dims[0], dims[1], dims[2]},
                        {dims[3], dims[4], dims[5]},
                        dims[6],
                        dims[7],
                        dims[8],
                        dims[9],
                        dims[10],
                        dims[11],
                        zx,
                        {mult, rsh, nq},
                        {zy, lo, hi}};
  if (fnn_dwconv_check(&l) == FNN_OK) {
    fnn_dwconv_s8(&l, x, y);
  }
}
