/**
 * @file test_fm_atan.c
 * @brief Host tests for fm_atan.h against a long double reference (the larger
 *        sweeps are in test/exhaustive/exhaustive_atan.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_atan.h"
#include "test_stats.h"

static const long double TURN = 6.283185307179586476925286766559L;

/* got - ref in LSB, both binary angles of 2^bits per turn, taken mod a turn */
static double ang_err(long double got, long double ref_rad, unsigned bits) {
  long double turn = (long double)(1ULL << bits);
  long double e = got - ref_rad / TURN * turn;
  e -= turn * floorl(e / turn + 0.5L);
  return (double)e;
}

void test_fm_atan2_q15(void) {
  ASSERT_EQ(fm_atan2_q15(0, 0), 0);
  ASSERT_EQ(fm_atan2_q15(0, 1), 0);
  ASSERT_EQ(fm_atan2_q15(1, 0), 0x4000);
  ASSERT_EQ(fm_atan2_q15(-1, 0), -0x4000);
  ASSERT_EQ(fm_atan2_q15(0, -1), INT16_MIN); /* pi */
  ASSERT_EQ(fm_atan2_q15(100, 100), 0x2000);
  ASSERT_EQ(fm_atan2_q15(INT16_MIN, INT16_MIN), -0x6000);
  ErrStats s;
  stats_init(&s, "fm_atan2_q15, every 37th y, 11th x", "lsb");
  for (int32_t y = -32768; y < 32768; y += 37) {
    for (int32_t x = -32768; x < 32768; x += 11) {
      int16_t a = fm_atan2_q15((fm_q15)y, (fm_q15)x);
      stats_add(&s, y, ang_err(a, atan2l(y, x), 16U));
      if (y != INT16_MIN && (y != 0 || x >= 0)) { /* odd in y */
        ASSERT_EQ(fm_atan2_q15((fm_q15)-y, (fm_q15)x), (int16_t)-a);
      }
    }
  }
  ASSERT_STATS_MAX(&s, 0.65);
}

void test_fm_asin_q15(void) {
  ASSERT_EQ(fm_asin_q15(0), 0);
  ASSERT_EQ(fm_asin_q15(INT16_MIN), -0x4000);
  ASSERT_EQ(fm_acos_q15(INT16_MIN), 0x8000);
  ASSERT_EQ(fm_acos_q15(0), 0x4000);
  ASSERT_EQ(fm_asin_q15(16384), 0x1555); /* asin 1/2 = 1/12 turn */
  ErrStats s, c;
  stats_init(&s, "fm_asin_q15, all inputs", "lsb");
  stats_init(&c, "fm_acos_q15, all inputs", "lsb");
  for (int32_t x = -32768; x < 32768; x++) {
    stats_add(&s, x, ang_err(fm_asin_q15((fm_q15)x), asinl(x / 32768.0L), 16U));
    stats_add(&c, x, ang_err(fm_acos_q15((fm_q15)x), acosl(x / 32768.0L), 16U));
  }
  ASSERT_STATS_MAX(&s, 0.75);
  ASSERT_STATS_MAX(&c, 0.75);
}

void test_fm_atan_q31(void) {
  ASSERT_EQ(fm_atan2_q31(0, 0), 0);
  ASSERT_EQ(fm_atan2_q31(1, 0), 0x40000000);
  ASSERT_EQ(fm_atan2_q31(0, -5), INT32_MIN);
  ASSERT_EQ(fm_atan2_q31(INT32_MIN, INT32_MIN), -0x60000000);
  ASSERT_EQ(fm_atan2_q31(INT32_MAX, INT32_MAX), 0x20000000);
  ASSERT_EQ(fm_asin_q31(INT32_MIN), -0x40000000);
  ASSERT_EQ(fm_acos_q31(INT32_MIN), 0x80000000U);
  ErrStats a, s;
  stats_init(&a, "fm_atan2_q31", "lsb");
  stats_init(&s, "fm_asin_q31 / fm_acos_q31", "lsb");
  uint64_t st = 171U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t v = test_rng_u32(&st);
    int32_t y = (int32_t)u >> (v & 31U);
    int32_t x = (int32_t)v >> (u & 31U);
    if ((i & 3) == 1) { /* near the diagonal */
      x = (int32_t)((uint32_t)y + (u >> 28) - 8U);
    } else if ((i & 3) == 2) { /* just below a table point k / 128 */
      x = (int32_t)(v | 0x40000000U) >> 1;
      y = (int32_t)((int64_t)x * (int32_t)(u >> 25) / 128) - (int32_t)(v & 3U);
    }
    stats_add(&a, y, ang_err(fm_atan2_q31(y, x), atan2l(y, x), 32U));
    int32_t z = (int32_t)u;
    stats_add(&s, z, ang_err(fm_asin_q31(z), asinl(z / 2147483648.0L), 32U));
    stats_add(&s, z, ang_err(fm_acos_q31(z), acosl(z / 2147483648.0L), 32U));
  }
  ASSERT_STATS_MAX(&a, 0.65);
  ASSERT_STATS_MAX(&s, 0.81);
}
