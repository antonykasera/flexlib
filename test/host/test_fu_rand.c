/**
 * @file test_fu_rand.c
 * @brief Host tests for fu_rand.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_rand.h"
#include "test_stats.h"
#include <stdbool.h>

void test_fu_rand_known_answers(void) {
  FU_Xorshift32 x;
  fu_xorshift32_seed(&x, 0U); /* 0 becomes Marsaglia's 2463534242 */
  static const uint32_t xs[] = {723471715U, 2497366906U, 2064144800U,
                                2008045182U, 3532304609U};
  for (size_t i = 0U; i < 5U; i++) {
    ASSERT_EQ(fu_xorshift32(&x), xs[i]);
  }

  FU_Xoshiro128 r = {{1U, 2U, 3U, 4U}};
  static const uint32_t xo[] = {0x2D00U,     0x0U,        0x5A7080U,
                                0x4389D80U,  0x79199D9BU, 0x61963B24U};
  for (size_t i = 0U; i < 6U; i++) {
    ASSERT_EQ(fu_xoshiro128ss(&r), xo[i]);
  }

  fu_xoshiro128_seed(&r, 12345U); /* splitmix64 */
  ASSERT_EQ(r.s[0], 0xA9D111A0U);
  ASSERT_EQ(r.s[1], 0x22118258U);
  ASSERT_EQ(r.s[2], 0xF713F8EDU);
  ASSERT_EQ(r.s[3], 0x346EDCE5U);
  ASSERT_EQ(fu_xoshiro128ss(&r), 0x89F4BEFDU);

  /* checked against the 128 x 128 transition matrix raised to 2^64 */
  FU_Xoshiro128 j = {{1U, 2U, 3U, 4U}};
  fu_xoshiro128_jump(&j);
  ASSERT_EQ(j.s[0], 0xA9765206U);
  ASSERT_EQ(j.s[1], 0x797AA168U);
  ASSERT_EQ(j.s[2], 0x5B62E331U);
  ASSERT_EQ(j.s[3], 0x02ABD971U);
}

/* chi-square over n buckets */
static void check_uniform(uint32_t n, bool xorshift) {
  static uint32_t count[1000];
  memset(count, 0, sizeof count);
  FU_Xorshift32 x;
  FU_Xoshiro128 r;
  fu_xorshift32_seed(&x, 99U + n);
  fu_xoshiro128_seed(&r, 99U + n);
  uint32_t draws = 1000U * n;
  for (uint32_t i = 0U; i < draws; i++) {
    uint32_t v = xorshift ? fu_xorshift32_below(&x, n)
                          : fu_xoshiro128_below(&r, n);
    ASSERT_TRUE(v < n);
    count[v < n ? v : 0U]++;
  }
  double chi = 0.0;
  for (uint32_t v = 0U; v < n; v++) {
    double d = (double)count[v] - 1000.0;
    chi += d * d / 1000.0;
  }
  double df = (double)(n - 1U);
  ASSERT_TRUE(chi < df + 7.0 * sqrt(2.0 * df) + 20.0);
}

void test_fu_rand_below(void) {
  static const uint32_t ns[] = {1U, 2U, 3U, 7U, 10U, 100U, 1000U};
  for (size_t i = 0U; i < sizeof ns / sizeof ns[0]; i++) {
    check_uniform(ns[i], false);
    check_uniform(ns[i], true);
  }
  FU_Xoshiro128 r;
  fu_xoshiro128_seed(&r, 7U);
  uint32_t threes = 0U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t v = fu_xoshiro128_below(&r, 0xC0000000U);
    ASSERT_TRUE(v < 0xC0000000U);
    threes += v % 3U == 0U ? 1U : 0U;
  }
  ASSERT_TRUE(threes > 330000U && threes < 337000U);
}
