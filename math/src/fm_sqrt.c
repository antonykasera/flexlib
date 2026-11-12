/**
 * @file fm_sqrt.c
 * @brief Square roots: integer, fixed point, and 1/sqrt in Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_sqrt.h"
#include "fm_internal.h"
#include "fm_tables.h"
#include "fu_bits.h"

/* s = floor(sqrt(x) 2^k), x << 2k normalized: nearest = (s + 2^(k-1)) >> k */

/* x != 0 */
FX_INLINE uint32_t isqrt32_nz(uint32_t x) {
  uint32_t lz = fx_clz(x) & 30U;
  return sqrt_norm32(x << lz) >> (lz >> 1);
}

uint32_t fm_isqrt32(uint32_t x) { return x == 0U ? 0U : isqrt32_nz(x); }

uint32_t fm_isqrt64(uint64_t x) {
  uint32_t hi = (uint32_t)(x >> 32);
  if (hi == 0U) {
    return x == 0U ? 0U : isqrt32_nz((uint32_t)x);
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

/* 2^24 / sqrt(x): seed, two Newton steps, then an exact test */
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
