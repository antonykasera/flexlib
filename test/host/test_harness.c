/**
 * @file test_harness.c
 * @brief Self-tests for the harness
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "test_stats.h"
#include <math.h>

void test_rng_reproducible(void) {
  uint64_t s = 0U;
  /* splitmix64's published first output for seed 0 */
  ASSERT_EQ(test_rng_next(&s), 0xE220A8397B1DCDAFULL);

  uint64_t a = 42U;
  uint64_t b = 42U;
  for (int i = 0; i < 1000; i++) {
    ASSERT_EQ(test_rng_u32(&a), test_rng_u32(&b));
  }
}

void test_stats_tracks_max_mean_bias(void) {
  ErrStats s;
  stats_init(&s, "self", "lsb");
  stats_add(&s, 10.0, 1.0);
  stats_add(&s, 20.0, -3.0);
  stats_add(&s, 30.0, 2.0);
  ASSERT_EQ(s.n, 3);
  ASSERT_NEAR(s.max_abs, 3.0, 0.0);
  ASSERT_NEAR(s.worst_in, 20.0, 0.0);
  ASSERT_NEAR(stats_mean_abs(&s), 2.0, 1e-15);
  ASSERT_NEAR(stats_bias(&s), 0.0, 1e-15);

  stats_add(&s, 40.0, (double)NAN);
  ASSERT_TRUE(isinf(s.max_abs));
  ASSERT_NEAR(s.worst_in, 40.0, 0.0);
}

void test_ulp_err_f32(void) {
  ASSERT_NEAR(ulp_err_f32(nextafterf(1.0f, 2.0f), 1.0), 1.0, 0.0);
  ASSERT_NEAR(ulp_err_f32(1.0f, 1.0 + ldexp(1.0, -24)), -0.5, 0.0);
  ASSERT_NEAR(ulp_err_f32(-3.0f, -3.0), 0.0, 0.0);
  /* denormal spacing is 2^-149, including around zero */
  ASSERT_NEAR(ulp_err_f32(ldexpf(1.0f, -149), 0.0), 1.0, 0.0);
  ASSERT_NEAR(ulp_err_f32(ldexpf(3.0f, -149), ldexp(1.0, -148)), 1.0, 0.0);

  ASSERT_NEAR(ulp_err_f32(NAN, (double)NAN), 0.0, 0.0);
  ASSERT_NEAR(ulp_err_f32(INFINITY, HUGE_VAL), 0.0, 0.0);
  ASSERT_TRUE(isinf(ulp_err_f32(NAN, 1.0)));
  ASSERT_TRUE(isinf(ulp_err_f32(1.0f, (double)NAN)));
  ASSERT_TRUE(isinf(ulp_err_f32(INFINITY, 1.0)));
  ASSERT_TRUE(isinf(ulp_err_f32(-INFINITY, HUGE_VAL)));
}
