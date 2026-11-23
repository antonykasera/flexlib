/**
 * @file fnn_quant.c
 * @brief Requantisation and multipliers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_quant.h"
#include <math.h>

static int32_t sat32(int64_t v) {
  return v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : (int32_t)v);
}

FX_COLD int32_t fnn_requant_wide_gemmlowp(int32_t acc, int32_t mult,
                                          int32_t rsh) {
  int64_t x = (int64_t)acc * (1LL << (-1 - rsh));
  return (int32_t)(fx_smlal(1LL << 30, sat32(x), mult) >> 31);
}

FX_COLD int32_t fnn_requant_wide_up(int32_t acc, int32_t mult, int32_t rsh) {
  uint32_t ts = (uint32_t)(32 + rsh);
  return sat32(fx_smlal(1LL << (ts - 1U), acc, mult) >> ts);
}

FX_COLD int32_t fnn_requant_wide_away(int32_t acc, int32_t mult, int32_t rsh) {
  uint32_t ts = (uint32_t)(32 + rsh);
  int64_t r = (1LL << (ts - 1U)) - (int64_t)(acc < 0);
  return sat32(fx_smlal(r, acc, mult) >> ts);
}

void fnn_requant_s8(const int32_t *acc, int8_t *y, uint32_t n,
                    const FNN_Quant *q, const FNN_Out *o) {
  /* Local copies, or GCC reloads them after every int8 store. */
  const int32_t *mult = q->mult;
  const int8_t *rsh = q->rsh;
  uint32_t nc = q->n;
  int32_t zp = o->zp, lo = o->lo, hi = o->hi;
  FX_ASSERT(nc > 0U && n % nc == 0U);
  for (uint32_t i = 0U; i < n; i += nc) {
    for (uint32_t c = 0U; c < nc; c++) {
      int32_t v = fnn_requant(acc[i + c], mult[c], rsh[c]);
      y[i + c] = fnn_clamp_s8(v, zp, lo, hi);
    }
  }
}

FNN_Status fnn_quant_from_scale(double scale, int32_t *mult, int8_t *rsh) {
  if (!(scale >= 0.0) || !isfinite(scale)) {
    return FNN_ERR_QUANT;
  }
  /* Brings scale into [0.5, 1) like frexp, by exact halving and doubling. */
  double q = scale;
  int32_t e = 0;
  while (q >= 1.0) {
    q *= 0.5;
    e++;
  }
  while (q > 0.0 && q < 0.5) {
    q *= 2.0;
    e--;
  }
  /* TFLite's round() takes halves away from zero. */
  int64_t m = (int64_t)(q * 2147483648.0 + 0.5);
  if (m == 1LL << 31) {
    m >>= 1;
    e++;
  }
  if (scale == 0.0 || e < -31) {
    m = 0;
    e = 0;
  } else if (e > 30) {
    m = INT32_MAX;
    e = 30;
  }
  *mult = (int32_t)m;
  *rsh = (int8_t)(-1 - e);
  return FNN_OK;
}

FNN_Status fnn_quant_from_tflite(int32_t tfl_mult, int32_t tfl_shift,
                                 int32_t *mult, int8_t *rsh) {
  if (tfl_mult < 0 || tfl_shift < -31 || tfl_shift > 30) {
    return FNN_ERR_QUANT;
  }
  *mult = tfl_mult;
  *rsh = (int8_t)(-1 - tfl_shift);
  return FNN_OK;
}
