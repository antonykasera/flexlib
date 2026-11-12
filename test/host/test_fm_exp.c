/**
 * @file test_fm_exp.c
 * @brief Host tests for fm_exp.h against a long double reference
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_exp.h"
#include "test_stats.h"

#define ONE FM_Q16_ONE

static long double sat(long double v) {
  return v > 2147483647.0L ? 2147483647.0L : v;
}

void test_fm_log(void) {
  ASSERT_EQ(fm_log2_q16(ONE), 0);
  ASSERT_EQ(fm_log2_q16(2 * ONE), ONE);
  ASSERT_EQ(fm_log2_q16(1), -16 * ONE);
  ASSERT_EQ(fm_log2_q16(0), INT32_MIN);
  ASSERT_EQ(fm_ln_q16(-5), INT32_MIN);
  ASSERT_EQ(fm_log10_q16(10 * ONE), ONE);
  ASSERT_EQ(fm_db_q16(10 * ONE), 20 * ONE);
  ASSERT_EQ(fm_ln_q16(ONE), 0);
  ErrStats l2, ln, l10, db;
  stats_init(&l2, "fm_log2_q16", "lsb");
  stats_init(&ln, "fm_ln_q16", "lsb");
  stats_init(&l10, "fm_log10_q16", "lsb");
  stats_init(&db, "fm_db_q16", "lsb");
  uint64_t st = 181U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    int32_t x = (int32_t)(u >> 1) >> (u & 31U);
    if (x <= 0) {
      continue;
    }
    long double v = x / 65536.0L;
    stats_add(&l2, x, (double)(fm_log2_q16(x) - log2l(v) * 65536.0L));
    stats_add(&ln, x, (double)(fm_ln_q16(x) - logl(v) * 65536.0L));
    stats_add(&l10, x, (double)(fm_log10_q16(x) - log10l(v) * 65536.0L));
    stats_add(&db, x, (double)(fm_db_q16(x) - 20.0L * log10l(v) * 65536.0L));
  }
  ASSERT_STATS_MAX(&l2, 0.5001);
  ASSERT_STATS_MAX(&ln, 0.5001);
  ASSERT_STATS_MAX(&l10, 0.5002);
  ASSERT_STATS_MAX(&db, 0.501);
}

void test_fm_exp(void) {
  ASSERT_EQ(fm_exp2_q16(0), ONE);
  ASSERT_EQ(fm_exp2_q16(3 * ONE), 8 * ONE);
  ASSERT_EQ(fm_exp2_q16(-16 * ONE), 1);
  ASSERT_EQ(fm_exp2_q16(-17 * ONE), 1); /* half an LSB rounds up */
  ASSERT_EQ(fm_exp2_q16(-17 * ONE - 1), 0);
  ASSERT_EQ(fm_exp2_q16(15 * ONE), INT32_MAX);
  ASSERT_EQ(fm_exp2_q16(INT32_MIN), 0);
  ASSERT_EQ(fm_exp_q16(0), ONE);
  ASSERT_EQ(fm_exp_q16(11 * ONE), INT32_MAX);
  ASSERT_EQ(fm_exp_q16(INT32_MIN), 0);
  ErrStats e2, e;
  stats_init(&e2, "fm_exp2_q16", "lsb");
  stats_init(&e, "fm_exp_q16", "lsb");
  uint64_t st = 182U;
  for (int i = 0; i < 1000000; i++) {
    int32_t x = (int32_t)test_rng_u32(&st) >> 11; /* +-16 */
    long double v = x / 65536.0L;
    stats_add(&e2, x, (double)(fm_exp2_q16(x) - sat(exp2l(v) * 65536.0L)));
    stats_add(&e, x, (double)(fm_exp_q16(x) - sat(expl(v) * 65536.0L)));
  }
  ASSERT_STATS_MAX(&e2, 0.54);
  ASSERT_STATS_MAX(&e, 0.54);
}

/* within 0.65 LSB + |y| 2^-32 of the result */
void test_fm_pow(void) {
  ASSERT_EQ(fm_pow_q16(-ONE, 0), ONE);
  ASSERT_EQ(fm_pow_q16(0, ONE), 0);
  ASSERT_EQ(fm_pow_q16(0, -ONE), INT32_MAX);
  ASSERT_EQ(fm_pow_q16(-ONE, 2 * ONE), 0);
  ASSERT_EQ(fm_pow_q16(4 * ONE, ONE / 2), 2 * ONE);
  ASSERT_EQ(fm_pow_q16(2 * ONE, 3 * ONE), 8 * ONE);
  ASSERT_EQ(fm_pow_q16(ONE / 2, -2 * ONE), 4 * ONE);
  uint64_t st = 183U;
  double worst = 0.0;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t v = test_rng_u32(&st);
    int32_t x = (int32_t)(u >> 1) >> (v & 15U);
    int32_t y = (int32_t)v >> (8U + (u & 15U));
    if (x <= 0 || y == 0) {
      continue;
    }
    long double r = sat(powl(x / 65536.0L, y / 65536.0L) * 65536.0L);
    long double bound = 0.65L + fabsl(y / 65536.0L) * r / 4294967296.0L;
    long double err = fabsl(fm_pow_q16(x, y) - r);
    worst = (double)(err / bound) > worst ? (double)(err / bound) : worst;
    ASSERT_TRUE(err <= bound);
  }
  printf("  fm_pow_q16: worst error %.3f of its bound\n", worst);
}
