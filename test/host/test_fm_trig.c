/**
 * @file test_fm_trig.c
 * @brief Host tests for fm_trig.h against a long double reference
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_trig.h"
#include "test_stats.h"

static const long double TURN = 6.283185307179586476925286766559L;

/* sin of angle / 2^bits turns, scaled by 2^(bits - 1) and saturated at +1 */
static long double ref_sin(uint64_t angle, unsigned bits) {
  long double one = (long double)(1ULL << (bits - 1U));
  long double v = sinl(TURN * (long double)angle / (2.0L * one)) * one;
  return v > one - 1.0L ? one - 1.0L : v;
}

void test_fm_trig_q15(void) {
  ASSERT_EQ(fm_sin_q15(0), 0);
  ASSERT_EQ(fm_sin_q15(0x4000), INT16_MAX);
  ASSERT_EQ(fm_sin_q15(0x8000), 0);
  ASSERT_EQ(fm_sin_q15(0xC000), INT16_MIN);
  ASSERT_EQ(fm_cos_q15(0), INT16_MAX);
  ASSERT_EQ(fm_cos_q15(0x8000), INT16_MIN);
  ASSERT_EQ(fm_sin_q15(8333), 23482); /* the interpolation's one exact tie */
  ASSERT_EQ(fm_sin_q15(8333 + 0x8000), -23482);
  ErrStats s, c;
  stats_init(&s, "fm_sin_q15, all angles", "lsb");
  stats_init(&c, "fm_cos_q15, all angles", "lsb");
  for (uint32_t a = 0U; a < 65536U; a++) {
    fm_q15 sv = fm_sin_q15((uint16_t)a);
    fm_q15 cv = fm_cos_q15((uint16_t)a);
    stats_add(&s, a, (double)(sv - ref_sin(a, 16U)));
    stats_add(&c, a, (double)(cv - ref_sin((a + 0x4000U) & 0xFFFFU, 16U)));
    fm_q15 ss, cc;
    fm_sincos_q15((uint16_t)a, &ss, &cc);
    ASSERT_EQ(ss, sv);
    ASSERT_EQ(cc, cv);
    if (sv != INT16_MAX && sv != INT16_MIN) { /* odd, but for +-1 */
      ASSERT_EQ(fm_sin_q15((uint16_t)(0U - a)), -sv);
    }
  }
  ASSERT_STATS_MAX(&s, 0.66);
  ASSERT_STATS_MAX(&c, 0.66);
}

static void check31(uint32_t a, ErrStats *s) {
  fm_q31 sv = fm_sin_q31(a);
  fm_q31 cv = fm_cos_q31(a);
  stats_add(s, a, (double)((long double)sv - ref_sin(a, 32U)));
  stats_add(s, a, (double)((long double)cv - ref_sin(a + 0x40000000ULL, 32U)));
  fm_q31 ss, cc;
  fm_sincos_q31(a, &ss, &cc);
  ASSERT_EQ(ss, sv);
  ASSERT_EQ(cc, cv);
}

void test_fm_trig_q31(void) {
  ASSERT_EQ(fm_sin_q31(0U), 0);
  ASSERT_EQ(fm_sin_q31(0x40000000U), INT32_MAX);
  ASSERT_EQ(fm_sin_q31(0x80000000U), 0);
  ASSERT_EQ(fm_sin_q31(0xC0000000U), INT32_MIN);
  ASSERT_EQ(fm_cos_q31(0U), INT32_MAX);
  ErrStats s;
  stats_init(&s, "fm_sin_q31 / fm_cos_q31", "lsb");
  for (uint32_t q = 0U; q < 4U; q++) { /* around each quarter and segment */
    for (uint32_t k = 0U; k < 256U; k++) {
      uint32_t base = (q << 30) + (k << 22);
      for (uint32_t e = 0U; e < 8U; e++) {
        check31(base + e, &s);
        check31(base - e - 1U, &s);
      }
    }
  }
  uint64_t st = 161U;
  for (int i = 0; i < 1000000; i++) {
    check31(test_rng_u32(&st), &s);
  }
  ASSERT_STATS_MAX(&s, 1.11);
}
