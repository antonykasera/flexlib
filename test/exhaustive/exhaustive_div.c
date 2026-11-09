/**
 * @file exhaustive_div.c
 * @brief fm_div_q15 on every pair, fm_recip_q16 on every input, fm_div_q31 and
 *        fm_div_q16 on 2^32 hashed pairs (`make exhaustive`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_div.h"
#include <stdio.h>

static int64_t ref_div(int64_t a, int64_t b, unsigned frac, int64_t lo,
                       int64_t hi) {
  if (b == 0) {
    return a > 0 ? hi : (a < 0 ? lo : 0);
  }
  uint64_t ua = (uint64_t)(a < 0 ? -a : a);
  uint64_t ub = (uint64_t)(b < 0 ? -b : b);
  uint64_t n = ua << frac;
  uint64_t q = n / ub + (2U * (n % ub) >= ub ? 1U : 0U);
  int64_t v = (a < 0) != (b < 0) ? -(int64_t)q : (int64_t)q;
  return v < lo ? lo : (v > hi ? hi : v);
}

static uint64_t mix(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

int main(void) {
  unsigned long long b15 = 0U, b31 = 0U, b16 = 0U, br = 0U;

#pragma omp parallel for reduction(+ : b15, b31, b16, br) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t u = (uint32_t)i;
    int32_t a15 = (int16_t)(u >> 16);
    int32_t b15v = (int16_t)u;
    b15 += fm_div_q15((fm_q15)a15, (fm_q15)b15v) !=
           ref_div(a15, b15v, 15U, INT16_MIN, INT16_MAX);
    br += fm_recip_q16((int32_t)u) !=
          ref_div(FM_Q16_ONE, (int32_t)u, 16U, INT32_MIN, INT32_MAX);
    /* every magnitude for both, and quotients near 1 */
    uint64_t h = mix((uint64_t)i);
    int32_t a = (int32_t)(uint32_t)h >> ((h >> 32) & 31U);
    int32_t b = (int32_t)(uint32_t)(h >> 37) >> ((h >> 59) & 31U);
    if ((h & 0x10000000U) != 0U) {
      a = (int32_t)((uint32_t)b - ((h >> 8) & 0xFFU));
    }
    b31 += fm_div_q31(a, b) != ref_div(a, b, 31U, INT32_MIN, INT32_MAX);
    b16 += fm_div_q16(a, b) != ref_div(a, b, 16U, INT32_MIN, INT32_MAX);
  }
  printf("exhaustive: fm_div_q15 %llu bad (all 2^32 pairs), fm_recip_q16 %llu "
         "(all 2^32), fm_div_q31 %llu, fm_div_q16 %llu (2^32 hashed pairs)\n",
         b15, br, b31, b16);
  return (b15 | br | b31 | b16) == 0U ? 0 : 1;
}
