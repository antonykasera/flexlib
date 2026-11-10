/**
 * @file test_fm_div.c
 * @brief Host tests for fm_div.h against exact integer references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_div.h"
#include "test_stats.h"

/* a 2^frac / b, nearest with ties away from zero, clamped to [lo, hi] */
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

static void check15(int32_t a, int32_t b) {
  ASSERT_EQ(fm_div_q15((fm_q15)a, (fm_q15)b),
            ref_div(a, b, 15U, INT16_MIN, INT16_MAX));
}

static void check31(int32_t a, int32_t b) {
  ASSERT_EQ(fm_div_q31(a, b), ref_div(a, b, 31U, INT32_MIN, INT32_MAX));
  ASSERT_EQ(fm_div_q16(a, b), ref_div(a, b, 16U, INT32_MIN, INT32_MAX));
}

/* every magnitude, either sign */
static int32_t any_mag(uint64_t *st) {
  uint32_t u = test_rng_u32(st);
  return (int32_t)u >> (u & 31U);
}

void test_fm_div_q15(void) {
  ASSERT_EQ(fm_div_q15(1, 0), INT16_MAX);
  ASSERT_EQ(fm_div_q15(-1, 0), INT16_MIN);
  ASSERT_EQ(fm_div_q15(0, 0), 0);
  ASSERT_EQ(fm_div_q15(INT16_MIN, INT16_MIN), INT16_MAX);
  ASSERT_EQ(fm_div_q15(16384, INT16_MIN), -16384);
  ASSERT_EQ(fm_div_q15(1, 3), 10923);
  ASSERT_EQ(fm_div_q15(-1, 3), -10923);
  static const int32_t edge[] = {INT16_MIN, INT16_MIN + 1, -3, -2, -1, 0, 1, 2,
                                 3,         INT16_MAX};
  for (unsigned e = 0U; e < sizeof edge / sizeof edge[0]; e++) {
    for (int32_t x = INT16_MIN; x <= INT16_MAX; x++) {
      check15(edge[e], x);
      check15(x, edge[e]);
    }
  }
  uint64_t st = 151U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    check15((int16_t)u >> (u >> 28), (int16_t)(u >> 16));
  }
}

static uint64_t pow_mod(uint64_t x, unsigned k, uint64_t m) {
  uint64_t r = 1U;
  for (; k != 0U; k >>= 1, x = x * x % m) {
    r = (k & 1U) != 0U ? r * x % m : r;
  }
  return r;
}

/* a with a 2^frac = r (mod b), b odd: fractions just off one half */
static int32_t num_for_rem(uint32_t b, unsigned frac, uint32_t r) {
  uint64_t inv = pow_mod((b + 1U) / 2U, frac, b); /* 2^-frac mod b */
  return (int32_t)(r * inv % b);
}

void test_fm_div_near_half(void) {
  ASSERT_EQ(fm_div_q16(1, 1 << 17), 1); /* Q16.16 has exact ties */
  ASSERT_EQ(fm_div_q16(-1, 1 << 17), -1);
  ASSERT_EQ(fm_div_q16(3, 1 << 17), 2);
  ASSERT_EQ(fm_div_q16(5, -(1 << 18)), -1);
  ASSERT_EQ(fm_div_q16(65537, 1 << 17), 32769);
  ASSERT_EQ(fm_div_q16(-196611, 1 << 17), -98306);
  uint64_t st = 154U;
  for (int i = 0; i < 200000; i++) {
    uint32_t b = (test_rng_u32(&st) >> (1U + ((unsigned)i & 15U))) | 1U;
    for (uint32_t r = b / 2U - 1U; r <= b / 2U + 2U; r++) {
      int32_t a = num_for_rem(b, 31U, r % b);
      check31(a, (int32_t)b);
      check31(-a, (int32_t)b);
      a = num_for_rem(b, 16U, r % b);
      check31(a, (int32_t)b);
      check31(a, -(int32_t)b);
    }
  }
}

void test_fm_div_q31_q16(void) {
  ASSERT_EQ(fm_div_q31(INT32_MIN, INT32_MIN), INT32_MAX);
  ASSERT_EQ(fm_div_q31(INT32_MIN, INT32_MAX), INT32_MIN);
  ASSERT_EQ(fm_div_q31(1, 0), INT32_MAX);
  ASSERT_EQ(fm_div_q31(-1, 0), INT32_MIN);
  ASSERT_EQ(fm_div_q31(0, 0), 0);
  ASSERT_EQ(fm_div_q16(3 * FM_Q16_ONE, -2 * FM_Q16_ONE), -3 * FM_Q16_ONE / 2);
  ASSERT_EQ(fm_div_q16(INT32_MAX, FM_Q16_ONE), INT32_MAX);
  ASSERT_EQ(fm_div_q16(INT32_MIN, FM_Q16_ONE), INT32_MIN);
  ASSERT_EQ(fm_div_q16(INT32_MIN, -FM_Q16_ONE), INT32_MAX);
  static const int32_t edge[] = {
      INT32_MIN, INT32_MIN + 1, -65537, -65536, -32769,   -1, 0,
      1,         32767,         65535,  65536,  INT32_MAX};
  uint64_t st = 152U;
  for (unsigned e = 0U; e < sizeof edge / sizeof edge[0]; e++) {
    for (int i = 0; i < 20000; i++) {
      int32_t x = any_mag(&st);
      check31(edge[e], x);
      check31(x, edge[e]);
      check31(x, (int32_t)((uint32_t)edge[e] + (test_rng_u32(&st) & 7U) - 3U));
    }
  }
  for (int i = 0; i < 2000000; i++) {
    int32_t a = any_mag(&st);
    int32_t b = any_mag(&st);
    check31(a, b);
    check31((int32_t)((uint32_t)b - ((uint32_t)a & 3U)), b); /* near 1 */
  }
}

void test_fm_recip_q16(void) {
  ASSERT_EQ(fm_recip_q16(0), INT32_MAX);
  ASSERT_EQ(fm_recip_q16(1), INT32_MAX);
  ASSERT_EQ(fm_recip_q16(-1), INT32_MIN);
  ASSERT_EQ(fm_recip_q16(2), INT32_MAX);
  ASSERT_EQ(fm_recip_q16(-2), INT32_MIN);
  ASSERT_EQ(fm_recip_q16(3), 1431655765);
  ASSERT_EQ(fm_recip_q16(FM_Q16_ONE), FM_Q16_ONE);
  ASSERT_EQ(fm_recip_q16(INT32_MIN), -2);
  uint64_t st = 153U;
  for (int i = 0; i < 2000000; i++) {
    int32_t x = any_mag(&st);
    ASSERT_EQ(fm_recip_q16(x),
              ref_div(FM_Q16_ONE, x, 16U, INT32_MIN, INT32_MAX));
  }
}
