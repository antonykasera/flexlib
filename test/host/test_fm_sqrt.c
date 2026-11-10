/**
 * @file test_fm_sqrt.c
 * @brief Host tests for fm_sqrt.h against exact integer references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_sqrt.h"
#include "test_stats.h"

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

/* nearest: up iff 4n > (2s + 1)^2 */
static uint64_t ref_round_sqrt(uint64_t n) {
  uint64_t s = ref_isqrt64(n);
  return s + ((u128)4U * (u128)n > (u128)(2U * s + 1U) * (u128)(2U * s + 1U)
                  ? 1U
                  : 0U);
}

static void check64(uint64_t n) { ASSERT_EQ(fm_isqrt64(n), ref_isqrt64(n)); }

void test_fm_isqrt(void) {
  for (uint32_t x = 0U; x < 1000000U; x++) {
    ASSERT_EQ(fm_isqrt32(x), ref_isqrt64(x));
  }
  check64(0U);
  check64(UINT64_MAX);
  check64(UINT32_MAX);
  check64((uint64_t)UINT32_MAX * UINT32_MAX);
  check64((uint64_t)UINT32_MAX * UINT32_MAX - 1U);
  uint64_t st = 131U;
  for (int i = 0; i < 1000000; i++) {
    uint64_t r = test_rng_next(&st);
    uint64_t k = r >> (r & 31U); /* squares of every size, and neighbours */
    uint64_t k2 = (uint64_t)(uint32_t)k * (uint32_t)k;
    check64(k2);
    check64(k2 - 1U);
    check64(k2 + 1U);
    check64(r >> (r >> 58));
    uint32_t x = (uint32_t)r >> (r & 31U);
    ASSERT_EQ(fm_isqrt32(x), ref_isqrt64(x));
  }
}

void test_fm_sqrt_fixed(void) {
  for (int32_t x = -32768; x < 32768; x++) {
    int32_t want = x <= 0 ? 0 : (int32_t)ref_round_sqrt((uint64_t)x << 15);
    ASSERT_EQ(fm_sqrt_q15((fm_q15)x), want);
  }
  ASSERT_EQ(fm_sqrt_q31(INT32_MIN), 0);
  ASSERT_EQ(fm_sqrt_q31(INT32_MAX), 0x7FFFFFFF);
  ASSERT_EQ(fm_sqrt_q16(FM_Q16_ONE * 4), FM_Q16_ONE * 2);
  ASSERT_EQ(fm_rsqrt_q16(0), INT32_MAX);
  ASSERT_EQ(fm_rsqrt_q16(-5), INT32_MAX);
  ASSERT_EQ(fm_rsqrt_q16(1), 1 << 24); /* 1/sqrt(2^-16) = 256 */
  ASSERT_EQ(fm_rsqrt_q16(FM_Q16_ONE * 4), FM_Q16_ONE / 2);
  uint64_t st = 141U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    int32_t x = (int32_t)(u >> 1) >> (u & 31U);
    uint64_t ux = x <= 0 ? 0U : (uint64_t)x;
    ASSERT_EQ(fm_sqrt_q31(x), ux == 0U ? 0U : ref_round_sqrt(ux << 31));
    ASSERT_EQ(fm_sqrt_q16(x), ux == 0U ? 0U : ref_round_sqrt(ux << 16));
    if (x > 0) {
      /* 2^24 / sqrt(x) = sqrt(2^48 / x): nearest r has (2r - 1)^2 x <= 2^50 < (2r + 1)^2 x */
      uint64_t r = (uint64_t)fm_rsqrt_q16(x);
      ASSERT_TRUE((u128)(2U * r - 1U) * (u128)(2U * r - 1U) *
                      (u128)(uint64_t)x <=
                  ((u128)1U << 50));
      ASSERT_TRUE((u128)(2U * r + 1U) * (u128)(2U * r + 1U) *
                      (u128)(uint64_t)x >
                  ((u128)1U << 50));
      ASSERT_NEAR((double)r, 16777216.0 / sqrt((double)x), 0.5000001);
    }
  }
}
