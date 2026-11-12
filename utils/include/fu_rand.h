/**
 * @file fu_rand.h
 * @brief Pseudo-random numbers: xorshift32 and xoshiro128**, not for crypto
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_RAND_H
#define FU_RAND_H

#include "fx_arch.h"
#include <stdint.h>

/* Marsaglia 13/17/5: period 2^32 - 1, weak low bits */
typedef struct {
  uint32_t s;
} FU_Xorshift32;

/* seed 0 becomes 2463534242 */
FX_INLINE void fu_xorshift32_seed(FU_Xorshift32 *r, uint32_t seed) {
  r->s = seed != 0U ? seed : 2463534242U;
}

FX_INLINE uint32_t fu_xorshift32(FU_Xorshift32 *r) {
  uint32_t s = r->s;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  r->s = s;
  return s;
}

/* xoshiro128** 1.1: period 2^128 - 1 */
typedef struct {
  uint32_t s[4];
} FU_Xoshiro128;

/* state from splitmix64(seed) */
void fu_xoshiro128_seed(FU_Xoshiro128 *r, uint64_t seed);

FX_INLINE uint32_t fu_xoshiro128ss(FU_Xoshiro128 *r) {
  uint32_t *s = r->s;
  uint32_t out = fx_rotl32(s[1] * 5U, 7U) * 9U;
  uint32_t t = s[1] << 9;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = fx_rotl32(s[3], 11U);
  return out;
}

/* advance 2^64 steps (non-overlapping streams) */
void fu_xoshiro128_jump(FU_Xoshiro128 *r);

/* rare path of the _below functions */
uint32_t fu_xorshift32_below_slow(FU_Xorshift32 *r, uint32_t n, uint64_t m);
uint32_t fu_xoshiro128_below_slow(FU_Xoshiro128 *r, uint32_t n, uint64_t m);

/* unbiased uniform in [0, n), n > 0 (Lemire) */
FX_INLINE uint32_t fu_xorshift32_below(FU_Xorshift32 *r, uint32_t n) {
  FX_ASSERT(n != 0U);
  uint64_t m = (uint64_t)fu_xorshift32(r) * n;
  if ((uint32_t)m < n) {
    return fu_xorshift32_below_slow(r, n, m);
  }
  return (uint32_t)(m >> 32);
}

FX_INLINE uint32_t fu_xoshiro128_below(FU_Xoshiro128 *r, uint32_t n) {
  FX_ASSERT(n != 0U);
  uint64_t m = (uint64_t)fu_xoshiro128ss(r) * n;
  if ((uint32_t)m < n) {
    return fu_xoshiro128_below_slow(r, n, m);
  }
  return (uint32_t)(m >> 32);
}

#endif /* FU_RAND_H */
