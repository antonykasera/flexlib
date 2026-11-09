/**
 * @file exhaustive_sqrt.c
 * @brief fm_isqrt32 on every 32-bit input; fm_sqrt_q31, fm_sqrt_q16 and
 *        fm_rsqrt_q16 on every positive input (`make exhaustive`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_sqrt.h"
#include <math.h>
#include <stdio.h>

__extension__ typedef unsigned __int128 u128;

static uint64_t ref_isqrt64(uint64_t n) {
  uint64_t s = (uint64_t)sqrtl((long double)n);
  while ((u128)s * (u128)s > n) {
    s--;
  }
  while ((u128)(s + 1U) * (u128)(s + 1U) <= n) {
    s++;
  }
  return s;
}

static uint64_t ref_round_sqrt(uint64_t n) {
  uint64_t s = ref_isqrt64(n);
  return s + ((u128)4U * (u128)n > (u128)(2U * s + 1U) * (u128)(2U * s + 1U)
                  ? 1U
                  : 0U);
}

int main(void) {
  unsigned long long b32 = 0U, b31 = 0U, b16 = 0U, br = 0U;

#pragma omp parallel for reduction(+ : b32, b31, b16, br) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t u = (uint32_t)i;
    b32 += fm_isqrt32(u) != (uint32_t)sqrt((double)u);
    if (u == 0U || u > 0x7FFFFFFFU) {
      continue;
    }
    int32_t x = (int32_t)u;
    b31 += (uint64_t)fm_sqrt_q31(x) != ref_round_sqrt((uint64_t)u << 31);
    b16 += (uint64_t)fm_sqrt_q16(x) != ref_round_sqrt((uint64_t)u << 16);
    uint64_t r = (uint64_t)fm_rsqrt_q16(x);
    br += !((u128)(2U * r - 1U) * (u128)(2U * r - 1U) * (u128)u <=
                ((u128)1U << 50) &&
            (u128)(2U * r + 1U) * (u128)(2U * r + 1U) * (u128)u >
                ((u128)1U << 50));
  }
  printf("exhaustive: fm_isqrt32 %llu bad (all 2^32), fm_sqrt_q31 %llu, "
         "fm_sqrt_q16 %llu, fm_rsqrt_q16 %llu (all 2^31 - 1 positive)\n",
         b32, b31, b16, br);
  return (b32 | b31 | b16 | br) == 0U ? 0 : 1;
}
