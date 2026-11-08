/**
 * @file test_main.c
 * @brief Host-side test runner: owns the shared counters, summary, and main().
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Each module's tests live in test_<mod>.c as `void test_xxx(void)`. Declare
 * them here and add a TEST_RUN line in main().
 */

#include "test_assert.h"

unsigned tests_run = 0;
unsigned tests_failed = 0;
unsigned checks_failed = 0;

int test_summary(void) {
  printf("\n%u tests, %u failed, %u checks failed\n", tests_run, tests_failed,
         checks_failed);
  return tests_failed == 0 ? 0 : 1;
}

/* --- harness (test_harness.c) --- */
void test_rng_reproducible(void);
void test_stats_tracks_max_mean_bias(void);
void test_ulp_err_f32(void);

/* --- arch (test_fx_arch.c) --- */
void test_fx_clz_ctz(void);
void test_fx_rbit_rev(void);
void test_fx_ssat(void);
void test_fx_usat(void);
void test_fx_multiplies(void);

int main(void) {
  TEST_RUN(test_rng_reproducible);
  TEST_RUN(test_stats_tracks_max_mean_bias);
  TEST_RUN(test_ulp_err_f32);

  TEST_RUN(test_fx_clz_ctz);
  TEST_RUN(test_fx_rbit_rev);
  TEST_RUN(test_fx_ssat);
  TEST_RUN(test_fx_usat);
  TEST_RUN(test_fx_multiplies);

  return TEST_SUMMARY();
}
