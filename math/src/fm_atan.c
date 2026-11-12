/**
 * @file fm_atan.c
 * @brief atan2, asin and acos as binary angles, from one atan table
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_atan.h"
#include "fm_internal.h"
#include "fm_sqrt.h"
#include "fm_tables.h"
#include <stdbool.h>

/* atan(mn / mx) in 2^-16 turns, 0 <= mn <= mx < 2^16, mx > 0 */
FX_INLINE uint32_t atan15(uint32_t mn, uint32_t mx) {
  uint32_t t = ((mn << 16) + (mx >> 1)) / mx;
  uint32_t k = t >> 9;
  uint32_t lo = fm_atan_tab[k];
  uint32_t v = lo + ((((fm_atan_tab[k + 1U] - lo) >> 3) * (t & 511U)) >> 6);
  return (v + 0x20000U) >> 18;
}

#define ATAN_K 341782638U /* 2^30 / pi: a Q39 tangent to Q38 turns */

/* atan(mn / mx) in 2^-32 turns, 0 <= mn < mx */
FX_INLINE uint32_t atan31(uint32_t mn, uint32_t mx) {
  uint32_t s = fx_clz(mx);
  mx <<= s;
  mn <<= s;
  uint32_t k = ((mn >> 16) << 7) / (mx >> 16); /* k, or one off */
  int64_t n = ((int64_t)mn << 7) - (int64_t)fx_umull(k, mx);
  if (n < 0) {
    k--;
    n += mx;
  } else if (n >= mx) {
    k++;
    n -= mx;
  }
  uint64_t d = ((uint64_t)mx << 7) + fx_umull(k, mn);
  uint32_t sd = (d >> 39) != 0U ? 8U : 7U;
  uint32_t r;
  uint32_t e = udiv_norm((uint64_t)n << (23U - sd), (uint32_t)(d >> sd), &r);
  uint32_t th = (uint32_t)(fx_umlal(1ULL << 31, e, ATAN_K) >> 32); /* Q38 */
  uint32_t e16 = e >> 16;
  uint32_t e2 = ((e16 * 21845U) >> 16) * e16; /* e^2 / 3, Q46 */
  uint32_t cube = (fx_umull_hi(th, e2) + 0x2000U) >> 14;
  return (fm_atan_tab[k] + ((th - cube) >> 4) + 2U) >> 2;
}

/* the octant back to the full turn */
FX_INLINE uint32_t unfold(uint32_t th, bool swap, bool xneg, bool yneg,
                          uint32_t quarter) {
  th = swap ? quarter - th : th;
  th = xneg ? 2U * quarter - th : th;
  return yneg ? 0U - th : th;
}

FX_INLINE uint32_t atan2_15(uint32_t ay, uint32_t ax, bool xneg, bool yneg) {
  bool swap = ay > ax;
  uint32_t mn = swap ? ax : ay;
  uint32_t mx = ax ^ ay ^ mn;
  if (mx == 0U) {
    return 0U;
  }
  return unfold(atan15(mn, mx), swap, xneg, yneg, 0x4000U);
}

FX_INLINE uint32_t atan2_31(uint32_t ay, uint32_t ax, bool xneg, bool yneg) {
  bool swap = ay > ax;
  uint32_t mn = swap ? ax : ay;
  uint32_t mx = ax ^ ay ^ mn;
  if (mx == 0U) {
    return 0U;
  }
  uint32_t th = mn == mx ? 0x20000000U : atan31(mn, mx);
  return unfold(th, swap, xneg, yneg, 0x40000000U);
}

int16_t fm_atan2_q15(fm_q15 y, fm_q15 x) {
  return (int16_t)atan2_15(mag(y), mag(x), x < 0, y < 0);
}

int32_t fm_atan2_q31(fm_q31 y, fm_q31 x) {
  return (int32_t)atan2_31(mag(y), mag(x), x < 0, y < 0);
}

/* asin x = atan2(x, sqrt(1 - x^2)), the root rounded to the nearest LSB */
FX_INLINE uint32_t asin15(fm_q15 x) {
  uint32_t ax = mag(x);
  uint32_t n = (1U << 30) - ax * ax;
  uint32_t c = fm_isqrt32(n);
  c += n - c * c > c ? 1U : 0U;
  return atan2_15(ax, c, false, x < 0);
}

FX_INLINE uint32_t asin31(fm_q31 x) {
  uint32_t ax = mag(x);
  uint64_t n = (1ULL << 62) - (uint64_t)ax * ax;
  uint32_t c = fm_isqrt64(n);
  c += n - (uint64_t)c * c > c ? 1U : 0U;
  return atan2_31(ax, c, false, x < 0);
}

int16_t fm_asin_q15(fm_q15 x) { return (int16_t)asin15(x); }

uint16_t fm_acos_q15(fm_q15 x) { return (uint16_t)(0x4000U - asin15(x)); }

int32_t fm_asin_q31(fm_q31 x) { return (int32_t)asin31(x); }

uint32_t fm_acos_q31(fm_q31 x) { return 0x40000000U - asin31(x); }
