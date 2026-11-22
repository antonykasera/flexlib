/**
 * @file fnn_softmax.c
 * @brief Softmax and argmax
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_softmax.h"
#include "fx_arch.h"
#include <math.h>

FNN_Status fnn_softmax_s8_build(double beta, double s_x,
                                uint32_t exp_tab[256]) {
  double k = beta * s_x;
  if (!(k > 0.0) || !isfinite(k)) {
    return FNN_ERR_QUANT;
  }
  for (uint32_t d = 0U; d < 256U; d++) {
    exp_tab[d] = (uint32_t)(ldexp(exp(-k * (double)d), 31) + 0.5);
  }
  return FNN_OK;
}

static FX_COLD uint32_t exact(uint32_t q, uint32_t e, uint64_t s) {
  uint64_t r = ((uint64_t)e << 8) + (s >> 1) - (uint64_t)q * s;
  return q + (r >= s ? 1U : 0U);
}

void fnn_softmax_s8(const int8_t *x, int8_t *y, uint32_t n,
                    const uint32_t exp_tab[256]) {
  FX_ASSERT(n >= 1U);
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    mx = x[i] > mx ? x[i] : mx;
  }
  /* Each score's distance from the maximum waits in y for the last pass. */
  uint8_t *d = (uint8_t *)y;
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t k = (uint32_t)(mx - x[i]);
    d[i] = (uint8_t)k;
    s += exp_tab[k];
  }

  /* s is at least 2^31, and shifted right by t it has 23 bits. */
  uint32_t hi = (uint32_t)(s >> 32);
  uint32_t t = hi != 0U ? 41U - fx_clz(hi) : 9U;
  uint32_t b = (uint32_t)(s >> t) + 1U, ht = (uint32_t)((s >> 1) >> t);
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t e = exp_tab[d[i]];
    uint32_t a = (e >> (t - 8U)) + ht;
    /* q can be one low; only a remainder close to b needs the exact check. */
    uint32_t q = a / b;
    if (FX_UNLIKELY(b - (a - q * b) <= 520U)) {
      q = exact(q, e, s);
    }
    y[i] = q > 255U ? 127 : (int8_t)((int32_t)q - 128);
  }
}

uint32_t fnn_argmax_s8(const int8_t *x, uint32_t n) {
  FX_ASSERT(n >= 1U);
  uint32_t k = 0U;
  int32_t mx = x[0];
  for (uint32_t i = 1U; i < n; i++) {
    if (x[i] > mx) {
      mx = x[i];
      k = i;
    }
  }
  return k;
}
