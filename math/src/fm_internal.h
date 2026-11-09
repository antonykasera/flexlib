/**
 * @file fm_internal.h
 * @brief Helpers shared by flexmath sources (not part of the API)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_INTERNAL_H
#define FM_INTERNAL_H

#include "fm_tables.h"
#include "fx_arch.h"
#include <stdint.h>

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
}

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

/* floor(m 2^16 / d) and remainder, d >= 2^31, m < d 2^16: divlu (Hacker's
 * Delight), two UDIV digits; a digit >= 2^16 always fails the product test */
FX_INLINE uint32_t udiv_norm(uint64_t m, uint32_t d, uint32_t *rem) {
  uint32_t nh = (uint32_t)(m >> 16);
  uint32_t n1 = (uint32_t)m & 0xFFFFU;
  uint32_t d1 = d >> 16;
  uint32_t d0 = d & 0xFFFFU;
  uint32_t q1 = nh / d1;
  uint32_t r = nh - q1 * d1;
  while (q1 * d0 > ((r << 16) | n1)) {
    q1--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  uint32_t u = ((nh << 16) | n1) - q1 * d;
  uint32_t q0 = u / d1;
  r = u - q0 * d1;
  while (q0 * d0 > (r << 16)) {
    q0--;
    r += d1;
    if (r > 0xFFFFU) {
      break;
    }
  }
  *rem = (u << 16) - q0 * d;
  return (q1 << 16) | q0;
}

#endif /* FM_INTERNAL_H */
