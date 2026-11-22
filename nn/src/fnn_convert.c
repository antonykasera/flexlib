/**
 * @file fnn_convert.c
 * @brief A model's inputs and outputs
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_convert.h"
#include "fnn_quant.h"
#include <string.h>

void fnn_quantize_q15_s8(const int16_t *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = fnn_clamp_s8(fnn_requant(x[i], mult, rsh), zp, -128, 127);
  }
}

void fnn_quantize_q31_s8(const int32_t *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = fnn_clamp_s8(fnn_requant(x[i], mult, rsh), zp, -128, 127);
  }
}

static FX_COLD int32_t huge(uint32_t b, int32_t mult) {
  if ((b & 0x7FFFFFFFU) > 0x7F800000U) {
    return INT32_MIN;
  }
  return mult == 0 ? 0 : ((b >> 31) != 0U ? INT32_MIN : INT32_MAX);
}

void fnn_quantize_f32_s8(const float *x, int8_t *y, uint32_t n, int32_t mult,
                         int32_t rsh, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t b;
    memcpy(&b, &x[i], 4U);
    int32_t e = (int32_t)((b >> 23) & 255U);
    int32_t m = (int32_t)((b & 0x7FFFFFU) | 0x800000U);
    m = (b >> 31) != 0U ? -m : m;
    /* The float is m 2^(e - 150): its exponent joins the multiplier's shift. */
    int32_t sh = rsh + 150 - e;
    int32_t v;
    if (sh > 30) {
      v = 0;
    } else if (FX_LIKELY(sh >= -31)) {
      v = fnn_requant(m, mult, sh);
    } else {
      v = huge(b, mult);
    }
    y[i] = fnn_clamp_s8(v, zp, -128, 127);
  }
}

FX_INLINE int16_t sat16(int32_t v) {
  return (int16_t)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
}

void fnn_dequantize_s8_q15(const int8_t *x, int16_t *y, uint32_t n,
                           int32_t mult, int32_t rsh, int32_t zp) {
  if (rsh < 0 && rsh >= -23) {
    /* A multiplier of 1/2 or more moves its shift onto the 9-bit input. */
    uint32_t up = (uint32_t)-rsh;
    for (uint32_t i = 0U; i < n; i++) {
      int32_t k = x[i] - zp;
      int64_t r = 1LL << 31;
#if FNN_RULE == FNN_ROUND_AWAY
      r += k >> 31;
#endif
      y[i] = sat16(
          (int32_t)(fx_smlal(r, (int32_t)((uint32_t)k << up), mult) >> 32));
    }
    return;
  }
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = sat16(fnn_requant(x[i] - zp, mult, rsh));
  }
}

static FX_COLD void dequantize_slow(const int8_t *x, float *y, uint32_t n,
                                    float s, int32_t zp) {
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = s * (float)(x[i] - zp);
  }
}

void fnn_dequantize_s8_f32(const int8_t *x, float *y, uint32_t n, float s,
                           int32_t zp) {
  uint32_t b;
  memcpy(&b, &s, 4U);
  uint32_t es = b >> 23;
  /* The integer path needs a normal scale whose products stay finite. */
  if (es == 0U || es > 245U) {
    dequantize_slow(x, y, n, s, zp);
    return;
  }
  uint32_t ms = (b & 0x7FFFFFU) | 0x800000U;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t k = x[i] - zp;
    uint32_t r = 0U;
    if (k != 0) {
      uint32_t p = ms * (uint32_t)(k < 0 ? -k : k);
      uint32_t drop = 8U - fx_clz(p);
      /* Rounds to 24 bits, ties to even. A carry raises the exponent. */
      uint32_t half = (1U << drop) >> 1;
      uint32_t up = half != 0U ? half - 1U + ((p >> drop) & 1U) : 0U;
      uint32_t m = (p + up) >> drop;
      r = ((es + drop - 1U) << 23) + m;
      r |= k < 0 ? 0x80000000U : 0U;
    }
    memcpy(&y[i], &r, 4U);
  }
}
