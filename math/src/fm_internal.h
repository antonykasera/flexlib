/**
 * @file fm_internal.h
 * @brief Helpers shared by flexmath sources (not part of the API)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef FM_INTERNAL_H
#define FM_INTERNAL_H

#include "fx_arch.h"
#include <stdint.h>

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
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
