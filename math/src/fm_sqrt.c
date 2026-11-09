/**
 * @file fm_sqrt.c
 * @brief Square roots: integer, fixed point, and 1/sqrt in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_sqrt.h"
#include "fm_tables.h"
#include "fu_bits.h"

/* floor(sqrt(x)) for x in [2^30, 2^32): a table seed and one Newton step,
 * which lands on the floor or one above */
FX_INLINE uint32_t sqrt_norm32(uint32_t x) {
  uint32_t s = fm_sqrt_seed[(x >> 25) - 32U];
  s = (s + x / s) >> 1;
  s -= s >> 16; /* 65536 -> 65535, so s * s can't wrap */
  return s * s > x ? s - 1U : s;
}

/* floor(sqrt(x)) for x in [2^62, 2^64): isqrt32 of the top word, then a Newton
 * step on the remainder (32-bit divide) that never undershoots: floor or +1 */
FX_INLINE uint32_t sqrt_norm64(uint64_t x) {
  uint32_t h = sqrt_norm32((uint32_t)(x >> 32));
  uint64_t d = x - ((uint64_t)(h * h) << 32);
  uint64_t s = ((uint64_t)h << 16) + (uint32_t)(d >> 17) / h;
  uint32_t r = s > 0xFFFFFFFFU ? 0xFFFFFFFFU : (uint32_t)s;
  return (uint64_t)r * r > x ? r - 1U : r;
}

/* With x << 2k normalized, s = floor(sqrt(x) 2^k): the floor is s >> k and the
 * nearest is exactly (s + 2^(k-1)) >> k, as floor commutes with adding 2^(k-1)
 */

uint32_t fm_isqrt32(uint32_t x) {
  if (x == 0U) {
    return 0U;
  }
  uint32_t lz = fx_clz(x) & 30U;
  return sqrt_norm32(x << lz) >> (lz >> 1);
}

uint32_t fm_isqrt64(uint64_t x) {
  uint32_t hi = (uint32_t)(x >> 32);
  if (hi == 0U) {
    return fm_isqrt32((uint32_t)x);
  }
  uint32_t lz = fx_clz(hi) & 30U;
  return sqrt_norm64(x << lz) >> (lz >> 1);
}

/* nearest sqrt of n in [1, 2^62), so k >= 1 */
FX_INLINE uint32_t round_sqrt64(uint64_t n) {
  uint32_t lz = fu_clz64(n) & 62U;
  uint32_t k = lz >> 1;
  return (uint32_t)(((uint64_t)sqrt_norm64(n << lz) + (1U << (k - 1U))) >> k);
}

/* nearest sqrt of n in [1, 2^30), so k >= 1 */
FX_INLINE uint32_t round_sqrt32(uint32_t n) {
  uint32_t lz = fx_clz(n) & 30U;
  uint32_t k = lz >> 1;
  return (sqrt_norm32(n << lz) + (1U << (k - 1U))) >> k;
}

fm_q15 fm_sqrt_q15(fm_q15 x) {
  return x <= 0 ? 0 : (fm_q15)round_sqrt32((uint32_t)x << 15);
}

fm_q31 fm_sqrt_q31(fm_q31 x) {
  return x <= 0 ? 0 : (fm_q31)round_sqrt64((uint64_t)x << 31);
}

fm_q16 fm_sqrt_q16(fm_q16 x) {
  if (x < (1 << 14)) { /* below 1/4: 32 bits do */
    return x <= 0 ? 0 : (fm_q16)round_sqrt32((uint32_t)x << 16);
  }
  return (fm_q16)round_sqrt64((uint64_t)x << 16);
}

/* 2^24 / sqrt(x): 1/sqrt by a seed and two Newton steps (no divide) is within
 * one; the exact test (2r - 1)^2 x <= 2^50 < (2r + 1)^2 x settles it */
fm_q16 fm_rsqrt_q16(fm_q16 x) {
  if (x <= 0) {
    return INT32_MAX;
  }
  uint32_t k = fx_clz((uint32_t)x) & 30U;
  uint32_t xs = (uint32_t)x << k;
  uint32_t y = (uint32_t)fm_rsqrt_seed[(xs >> 25) - 32U] << 16;
  for (int i = 0; i < 2; i++) {
    uint32_t y2 = (uint32_t)(((uint64_t)y * y) >> 32);  /* (r/2)^2, Q30 */
    uint32_t p = (uint32_t)(((uint64_t)xs * y2) >> 32); /* t r^2, Q28 */
    y = (uint32_t)(((uint64_t)y * ((3U << 28) - p)) >> 29);
  }
  uint32_t sh = 22U - (k >> 1);
  uint32_t r = (y + (1U << (sh - 1U))) >> sh;
  const uint64_t lim = 1ULL << 50;
  uint64_t up = 2U * (uint64_t)r + 1U;
  if (up * up * (uint32_t)x <= lim) {
    r++;
  } else {
    uint64_t dn = 2U * (uint64_t)r - 1U;
    if (dn * dn * (uint32_t)x > lim) {
      r--;
    }
  }
  return (fm_q16)r;
}
