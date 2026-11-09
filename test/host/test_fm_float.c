/**
 * @file test_fm_float.c
 * @brief Host tests for fm_float.h against correctly rounded references (every
 *        float is in test/exhaustive/exhaustive_float.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "test_stats.h"
#include <string.h>

static uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

static float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* the double root rounded to float is correctly rounded: 53 >= 2 * 24 + 2 */
static uint32_t ref_sqrtf_bits(uint32_t b) {
  float x = u2f(b);
  if (isnan(x)) {
    return b | 0x00400000U;
  }
  if (x < 0.0f) {
    return 0x7FC00000U;
  }
  return f2u((float)sqrt((double)x));
}

static void check_sqrtf(uint32_t b) {
  ASSERT_EQ(f2u(fm_sqrtf(u2f(b))), ref_sqrtf_bits(b));
}

void test_fm_sqrtf(void) {
  ASSERT_EQ(f2u(fm_sqrtf(0.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_sqrtf(-0.0f)), 0x80000000U);
  ASSERT_EQ(f2u(fm_sqrtf(INFINITY)), 0x7F800000U);
  ASSERT_EQ(f2u(fm_sqrtf(-INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(-1.0f)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0x80000001U))), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0x7F800001U))), 0x7FC00001U); /* quieted */
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0xFFC01234U))), 0xFFC01234U);
  ASSERT_EQ(f2u(fm_sqrtf(1.0f)), f2u(1.0f));
  ASSERT_EQ(f2u(fm_sqrtf(4.0f)), f2u(2.0f));
  ASSERT_EQ(f2u(fm_sqrtf(0x1p-148f)), f2u(0x1p-74f));
  check_sqrtf(0x00000001U); /* 2^-149 */
  check_sqrtf(0x007FFFFFU);
  check_sqrtf(0x00800000U);
  check_sqrtf(0x7F7FFFFFU);
  for (uint32_t b = 0U; b < 0x00800000U; b += 7U) {
    check_sqrtf(b);
  }
  /* exact squares (12-bit roots, even exponent shifts) and their neighbours */
  uint64_t st = 151U;
  for (int i = 0; i < 100000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t s = (u >> 20) | 0x800U;
    uint32_t sb = f2u((float)(s * s)) - 0x20000000U + ((u & 63U) << 24);
    check_sqrtf(sb - 1U);
    check_sqrtf(sb);
    check_sqrtf(sb + 1U);
  }
  ErrStats es;
  stats_init(&es, "fm_sqrtf, random bits", "ulp");
  for (int i = 0; i < 1000000; i++) {
    uint32_t b = test_rng_u32(&st);
    check_sqrtf(b);
    float x = u2f(b & 0x7FFFFFFFU);
    if (!isnan(x)) {
      stats_add(&es, (double)x, ulp_err_f32(fm_sqrtf(x), sqrt((double)x)));
    }
  }
  ASSERT_STATS_MAX(&es, 0.5);
}
