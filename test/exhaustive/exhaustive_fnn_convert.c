/**
 * @file exhaustive_fnn_convert.c
 * @brief fnn_quantize_f32_s8 on every float, and fnn_dequantize_s8_f32 on every
 * scale mantissa and every q - z, in the build's rounding mode
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_convert.h"
#include "fnn_quant.h"
#include <stdio.h>
#include <string.h>

static int64_t sat(int64_t v) {
  return v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : v);
}

/* Rounds the exact product the way the build's mode does. */
static int64_t ref(int64_t acc, int64_t mult, int32_t rsh) {
#if FNN_ROUNDING == FNN_ROUND_GEMMLOWP
  int64_t shift = -1 - rsh;
  int64_t x = sat(acc * (1LL << (shift > 0 ? shift : 0)));
  int64_t v = (x * mult + (1LL << 30)) >> 31;
  int64_t e = shift > 0 ? 0 : -shift;
  if (e == 0) {
    return v;
  }
  int64_t h = 1LL << (e - 1);
  return v < 0 ? -((-v + h) >> e) : (v + h) >> e;
#else
  int64_t ts = 32 + rsh;
  int64_t p = acc * mult, h = 1LL << (ts - 1);
#if FNN_ROUNDING == FNN_ROUND_AWAY
  if (p < 0) {
    return sat(-((-p + h) >> ts));
  }
#endif
  return sat((p + h) >> ts);
#endif
}

static int8_t ref_f32(uint32_t b, int32_t mult, int32_t rsh, int32_t zp) {
  uint32_t e = (b >> 23) & 255U, f = b & 0x7FFFFFU;
  if (e == 255U && f != 0U) {
    return -128;
  }
  int64_t m = (int64_t)(e != 0U ? f | 0x800000U : f);
  m = (b >> 31) != 0U ? -m : m;
  int32_t sh = rsh + 150 - (int32_t)(e != 0U ? e : 1U);
  int64_t v;
  if (mult == 0 || m == 0 || sh > 30) {
    v = 0;
  } else if (sh < -31) {
    v = m < 0 ? INT32_MIN : INT32_MAX;
  } else {
    v = ref(m, mult, sh);
  }
  v += zp;
  return (int8_t)(v < -128 ? -128 : (v > 127 ? 127 : v));
}

static unsigned long long quantize_all(double s, int32_t zp) {
  int32_t mult;
  int8_t rsh;
  if (fnn_quant_from_scale(1.0 / s, &mult, &rsh) != FNN_OK) {
    return 1U;
  }
  unsigned long long bad = 0U;
#pragma omp parallel for reduction(+ : bad) schedule(static)
  for (long long blk = 0; blk < (1LL << 20); blk++) {
    float x[4096];
    int8_t y[4096];
    uint32_t b0 = (uint32_t)blk << 12;
    for (uint32_t i = 0U; i < 4096U; i++) {
      uint32_t b = b0 + i;
      memcpy(&x[i], &b, 4U);
    }
    fnn_quantize_f32_s8(x, y, 4096U, mult, rsh, zp);
    for (uint32_t i = 0U; i < 4096U; i++) {
      bad += y[i] != ref_f32(b0 + i, mult, rsh, zp);
    }
  }
  return bad;
}

static unsigned long long dequantize_all(uint32_t es, uint32_t step) {
  unsigned long long bad = 0U;
#pragma omp parallel for reduction(+ : bad) schedule(static)
  for (long long f = 0; f < (1LL << 23); f += step) {
    int8_t x[256];
    float y[256];
    for (uint32_t i = 0U; i < 256U; i++) {
      x[i] = (int8_t)(i - 128U);
    }
    uint32_t sb = (es << 23) | (uint32_t)f;
    float s;
    memcpy(&s, &sb, 4U);
    for (int32_t zp = -128; zp <= 127; zp += 255) {
      fnn_dequantize_s8_f32(x, y, 256U, s, zp);
      for (uint32_t i = 0U; i < 256U; i++) {
        volatile float want = s * (float)(x[i] - zp);
        bad += memcmp(&y[i], (const void *)&want, 4U) != 0;
      }
    }
  }
  return bad;
}

int main(void) {
  unsigned long long q = quantize_all(1.0 / 256.0, 0) +
                         quantize_all(0.0371, -7) + quantize_all(3.7, 100);
  unsigned long long d = dequantize_all(127U, 1U) + dequantize_all(1U, 7U) +
                         dequantize_all(245U, 7U);
  printf("fnn_quantize_f32_s8, rounding mode %d: 3 x 2^32 floats, %llu wrong\n",
         FNN_ROUNDING, q);
  printf("fnn_dequantize_s8_f32: 2^23 scale mantissas x 511 values, %llu "
         "wrong\n",
         d);
  return q == 0U && d == 0U ? 0 : 1;
}
