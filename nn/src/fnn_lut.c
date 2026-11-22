/**
 * @file fnn_lut.c
 * @brief Activation functions as tables
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_lut.h"
#include "fx_arch.h"
#include <math.h>

FNN_Status fnn_lut_s8_build(double (*f)(double), double s_x, int32_t z_x,
                            double s_y, int32_t z_y, int8_t lut[256]) {
  if (!(s_x > 0.0) || !(s_y > 0.0) || !isfinite(s_x) || !isfinite(s_y)) {
    return FNN_ERR_QUANT;
  }
  for (int32_t q = -128; q < 128; q++) {
    double v = f(s_x * (double)(q - z_x)) / s_y + (double)z_y;
    /* The first test also sends NaN to -128. */
    v = !(v >= -128.0) ? -128.0 : (v > 127.0 ? 127.0 : v);
    /* Rounds to nearest, ties up. */
    lut[(uint8_t)q] = (int8_t)((int32_t)(v + 128.5) - 128);
  }
  return FNN_OK;
}

/* The barriers keep the loads grouped and the byte stores unmerged. */
#if FX_ARM
/* Thumb has 16-bit loads and stores only for r0-r7. */
#define LOW "+l"
#else
#define LOW "+r"
#endif
#define HOLD4(a, b, c, d) __asm__("" : LOW(a), LOW(b), LOW(c), LOW(d))
#define STORE(p, v)                                                            \
  do {                                                                         \
    *(p) = (uint8_t)(v);                                                       \
    __asm__("" ::: "memory");                                                  \
  } while (0)

FX_INLINE void lut4(const uint8_t *x, uint8_t *y, const uint8_t *t) {
  uint32_t a = x[0], b = x[1], c = x[2], d = x[3];
  HOLD4(a, b, c, d);
  a = t[a];
  b = t[b];
  c = t[c];
  d = t[d];
  HOLD4(a, b, c, d);
  STORE(y, a);
  STORE(y + 1, b);
  STORE(y + 2, c);
  STORE(y + 3, d);
}

void fnn_lut_s8(const int8_t *x, int8_t *y, uint32_t n, const int8_t lut[256]) {
  const uint8_t *u = (const uint8_t *)x, *t = (const uint8_t *)lut;
  uint8_t *v = (uint8_t *)y;
  const uint8_t *end = u + (n & ~15U);
  while (u != end) {
    lut4(u, v, t);
    lut4(u + 4, v + 4, t);
    lut4(u + 8, v + 8, t);
    lut4(u + 12, v + 12, t);
    u += 16;
    v += 16;
    /* Keeps the store offsets within 16-bit encodings. */
    FX_OPAQUE(v);
  }
  for (n &= 15U; n >= 4U; n -= 4U) {
    lut4(u, v, t);
    u += 4;
    v += 4;
  }
  while (n-- > 0U) {
    *v++ = t[*u++];
  }
}
