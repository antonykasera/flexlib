/**
 * @file fm_trig.c
 * @brief Sine and cosine of binary angles from a quarter-wave table
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_trig.h"
#include "fm_tables.h"
#include <stdbool.h>

/* |sin| in Q31 at p in [0, 2^14] (a quarter turn), linear between entries */
FX_INLINE uint32_t mag15(uint32_t p) {
  uint32_t i = p >> 6;
  uint32_t lo = fm_sin_tab[i];
  return lo + (((fm_sin_tab[i + 1U] - lo) * (p & 63U)) >> 6);
}

/* falling quarters mirror p */
FX_INLINE fm_q15 sin15(uint32_t a) {
  uint32_t p = a & 0x3FFFU;
  if ((a & 0x4000U) != 0U) {
    p = 0x4000U - p;
  }
  int32_t m = (int32_t)((mag15(p) + 0x8000U) >> 16);
  return (fm_q15)FX_SSAT((a & 0x8000U) != 0U ? -m : m, 16);
}

fm_q15 fm_sin_q15(uint16_t angle) { return sin15(angle); }

fm_q15 fm_cos_q15(uint16_t angle) { return sin15(angle + 0x4000U); }

#define SEG_D38 1686629713U /* pi / 512 in Q38: one table step */

/* sin (or cos) at p in [0, 2^30) in Q31, Taylor about table entry x_i */
FX_INLINE uint32_t mag31(uint32_t p, bool cosine) {
  uint32_t i = p >> 22;
  uint32_t s = fm_sin_tab[i];
  uint32_t c = fm_sin_tab[256U - i];
  uint32_t x = cosine ? c : s;
  uint32_t y = cosine ? s : c;
  uint32_t d = fx_umull_hi((p & 0x3FFFFFU) << 10, SEG_D38);
  uint32_t d2 = fx_umull_hi(d, d) << 2;                           /* Q46 */
  int32_t yd = (int32_t)fx_umull_hi(y, d);                        /* Q37 */
  int32_t yd6 = (int32_t)((((uint32_t)yd >> 14) * 43691U) >> 10); /* Q31 */
  if (cosine) {
    yd = -yd;
    yd6 = -yd6;
  }
  int32_t corr = (int32_t)(fx_umull_hi(d2, (x >> 1) + (uint32_t)yd6) >> 8);
  return x + (uint32_t)((yd - corr + 32) >> 6); /* one rounding */
}

/* quarter q: sin is +f, +g, -f, -g for q = 0..3 (f = sin, g = cos of p) */
FX_INLINE int32_t finish31(uint32_t m, uint32_t q) {
  if ((q & 2U) != 0U) {
    return (int32_t)(0U - m);
  }
  return m == 0x80000000U ? INT32_MAX : (int32_t)m;
}

fm_q31 fm_sin_q31(uint32_t angle) {
  uint32_t q = angle >> 30;
  return finish31(mag31(angle & 0x3FFFFFFFU, (q & 1U) != 0U), q);
}

fm_q31 fm_cos_q31(uint32_t angle) { return fm_sin_q31(angle + 0x40000000U); }

void fm_sincos_q31(uint32_t angle, fm_q31 *s, fm_q31 *c) {
  uint32_t q = angle >> 30;
  uint32_t p = angle & 0x3FFFFFFFU;
  uint32_t f = mag31(p, false);
  uint32_t g = mag31(p, true);
  bool odd = (q & 1U) != 0U;
  *s = finish31(odd ? g : f, q);
  *c = finish31(odd ? f : g, q + 1U);
}
