/**
 * @file fu_rand.c
 * @brief Pseudo-random numbers: xorshift32 and xoshiro128**
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_rand.h"

static uint64_t splitmix64(uint64_t *x) {
  uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void fu_xoshiro128_seed(FU_Xoshiro128 *r, uint64_t seed) {
  uint64_t a = splitmix64(&seed);
  uint64_t b = splitmix64(&seed);
  r->s[0] = (uint32_t)a;
  r->s[1] = (uint32_t)(a >> 32);
  r->s[2] = (uint32_t)b;
  r->s[3] = (uint32_t)(b >> 32);
}

void fu_xoshiro128_jump(FU_Xoshiro128 *r) {
  static const uint32_t jump[4] = {0x8764000BU, 0xF542D2D3U, 0x6FA035C3U,
                                   0x77F2DB5BU};
  uint32_t s0 = 0U, s1 = 0U, s2 = 0U, s3 = 0U;
  for (unsigned i = 0U; i < 4U; i++) {
    for (unsigned b = 0U; b < 32U; b++) {
      if ((jump[i] >> b & 1U) != 0U) {
        s0 ^= r->s[0];
        s1 ^= r->s[1];
        s2 ^= r->s[2];
        s3 ^= r->s[3];
      }
      (void)fu_xoshiro128ss(r);
    }
  }
  r->s[0] = s0;
  r->s[1] = s1;
  r->s[2] = s2;
  r->s[3] = s3;
}

/* redraw while the low half falls in the 2^32 mod n values that would make
 * some results more likely */
FX_COLD uint32_t fu_xorshift32_below_slow(FU_Xorshift32 *r, uint32_t n,
                                          uint64_t m) {
  uint32_t t = (0U - n) % n;
  while ((uint32_t)m < t) {
    m = (uint64_t)fu_xorshift32(r) * n;
  }
  return (uint32_t)(m >> 32);
}

FX_COLD uint32_t fu_xoshiro128_below_slow(FU_Xoshiro128 *r, uint32_t n,
                                          uint64_t m) {
  uint32_t t = (0U - n) % n;
  while ((uint32_t)m < t) {
    m = (uint64_t)fu_xoshiro128ss(r) * n;
  }
  return (uint32_t)(m >> 32);
}
