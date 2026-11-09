/**
 * @file test_main.c
 * @brief Host-side test runner: owns the shared counters, summary, and main().
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "test_assert.h"

const char *test_filter = NULL;
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

/* --- fu_bits (test_fu_bits.c) --- */
void test_fu_counts_edges(void);
void test_fu_counts_random(void);
void test_fu_is_pow2(void);
void test_fu_reversal(void);
void test_fu_load_store(void);
void test_fu_sext(void);

/* --- fu_fmt (test_fu_fmt.c) --- */
void test_fu_fmt_u32(void);
void test_fu_fmt_i32(void);
void test_fu_fmt_hex32(void);
void test_fu_fmt_q16(void);
void test_fu_fmt_f32(void);
void test_fu_snprintf(void);

/* --- fu_crc (test_fu_crc.c) --- */
void test_fu_crc_check_values(void);
void test_fu_crc_vs_bitwise(void);
void test_fu_crc_chaining(void);

/* --- fu_ring (test_fu_ring.c) --- */
void test_fu_ring_vs_model(void);
void test_fu_queue_vs_model(void);
void test_fu_ring_from_dma(void);

/* --- fu_parse (test_fu_parse.c) --- */
void test_fu_parse_edges(void);
void test_fu_parse_q16_vs_exact(void);
void test_fu_parse_round_trips(void);
void test_fu_parse_q16_ties(void);

/* --- fu_cobs (test_fu_cobs.c) --- */
void test_fu_cobs_vectors(void);
void test_fu_cobs_random(void);
void test_fu_cobs_malformed(void);
void test_fu_cobs_rx(void);

/* --- fu_debounce (test_fu_debounce.c) --- */
void test_fu_debounce32_vs_reference(void);
void test_fu_debounce_vs_reference(void);
void test_fu_debounce_glitch(void);

/* --- fu_rand (test_fu_rand.c) --- */
void test_fu_rand_known_answers(void);
void test_fu_rand_below(void);

/* --- fm_fixed (test_fm_fixed.c) --- */
void test_fm_fixed_arith(void);
void test_fm_fixed_convert(void);

/* --- fm_sqrt (test_fm_sqrt.c) --- */
void test_fm_isqrt(void);
void test_fm_sqrt_fixed(void);

int main(int argc, char **argv) {
  test_filter = argc > 1 ? argv[1] : NULL;
  TEST_RUN(test_rng_reproducible);
  TEST_RUN(test_stats_tracks_max_mean_bias);
  TEST_RUN(test_ulp_err_f32);

  TEST_RUN(test_fx_clz_ctz);
  TEST_RUN(test_fx_rbit_rev);
  TEST_RUN(test_fx_ssat);
  TEST_RUN(test_fx_usat);
  TEST_RUN(test_fx_multiplies);

  TEST_RUN(test_fu_counts_edges);
  TEST_RUN(test_fu_counts_random);
  TEST_RUN(test_fu_is_pow2);
  TEST_RUN(test_fu_reversal);
  TEST_RUN(test_fu_load_store);
  TEST_RUN(test_fu_sext);

  TEST_RUN(test_fu_fmt_u32);
  TEST_RUN(test_fu_fmt_i32);
  TEST_RUN(test_fu_fmt_hex32);
  TEST_RUN(test_fu_fmt_q16);
  TEST_RUN(test_fu_fmt_f32);
  TEST_RUN(test_fu_snprintf);

  TEST_RUN(test_fu_crc_check_values);
  TEST_RUN(test_fu_crc_vs_bitwise);
  TEST_RUN(test_fu_crc_chaining);

  TEST_RUN(test_fu_ring_vs_model);
  TEST_RUN(test_fu_queue_vs_model);
  TEST_RUN(test_fu_ring_from_dma);

  TEST_RUN(test_fu_parse_edges);
  TEST_RUN(test_fu_parse_q16_vs_exact);
  TEST_RUN(test_fu_parse_round_trips);
  TEST_RUN(test_fu_parse_q16_ties);

  TEST_RUN(test_fu_cobs_vectors);
  TEST_RUN(test_fu_cobs_random);
  TEST_RUN(test_fu_cobs_malformed);
  TEST_RUN(test_fu_cobs_rx);

  TEST_RUN(test_fu_debounce32_vs_reference);
  TEST_RUN(test_fu_debounce_vs_reference);
  TEST_RUN(test_fu_debounce_glitch);

  TEST_RUN(test_fu_rand_known_answers);
  TEST_RUN(test_fu_rand_below);

  TEST_RUN(test_fm_fixed_arith);
  TEST_RUN(test_fm_fixed_convert);

  TEST_RUN(test_fm_isqrt);
  TEST_RUN(test_fm_sqrt_fixed);

  return TEST_SUMMARY();
}
