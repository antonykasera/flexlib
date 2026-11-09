/**
 * @file test_fm_fixed.c
 * @brief Host tests for fm_fixed.h against exact references (the whole input
 *        spaces are in test/exhaustive/exhaustive_fixed.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_fixed.h"
#include "test_stats.h"

__extension__ typedef __int128 i128;

static int64_t clamp64(i128 x, int64_t lo, int64_t hi) {
  return x < lo ? lo : (x > hi ? hi : (int64_t)x);
}

/* round(x * 2^frac), ties to even, clamped; NaN -> 0 */
static int64_t ref_to_fixed(float x, int frac, int64_t lo, int64_t hi) {
  if (x != x) {
    return 0;
  }
  double d = nearbyint(ldexp((double)x, frac));
  return d < (double)lo ? lo : (d > (double)hi ? hi : (int64_t)d);
}

static float f32_bits(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

static uint32_t bits_f32(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

void test_fm_fixed_arith(void) {
  static const int32_t e31[] = {0, 1, -1, INT32_MAX, INT32_MIN, INT32_MAX - 1,
                                INT32_MIN + 1, 0x40000000, -0x40000000};
  for (size_t i = 0U; i < 9U; i++) {
    for (size_t j = 0U; j < 9U; j++) {
      int32_t a = e31[i];
      int32_t b = e31[j];
      ASSERT_EQ(fm_add_q31(a, b), clamp64((i128)a + b, INT32_MIN, INT32_MAX));
      ASSERT_EQ(fm_sub_q31(a, b), clamp64((i128)a - b, INT32_MIN, INT32_MAX));
    }
  }
  ASSERT_EQ(fm_mul_q15(-32768, -32768), 32767);
  ASSERT_EQ(fm_mul_q31(INT32_MIN, INT32_MIN), INT32_MAX);
  ASSERT_EQ(fm_mul_q31(INT32_MIN, INT32_MAX), -INT32_MAX);
  ASSERT_EQ(fm_mul_q16(FM_Q16_ONE, -FM_Q16_ONE), -FM_Q16_ONE);
  ASSERT_EQ(fm_mul_q16(0x7FFFFFFF, 0x7FFFFFFF), INT32_MAX);
  ASSERT_EQ(fm_mul_q16(INT32_MIN, 0x7FFFFFFF), INT32_MIN);

  uint64_t st = 111U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t v = test_rng_u32(&st);
    /* shifts reach small magnitudes and the edges equally */
    int32_t a = (int32_t)u >> (v & 15U);
    int32_t b = (int32_t)v >> (u & 15U);
    ASSERT_EQ(fm_add_q31(a, b), clamp64((i128)a + b, INT32_MIN, INT32_MAX));
    ASSERT_EQ(fm_sub_q31(a, b), clamp64((i128)a - b, INT32_MIN, INT32_MAX));
    i128 p = (i128)a * b;
    ASSERT_EQ(fm_mul_q31(a, b),
              clamp64((p + ((i128)1 << 30)) >> 31, INT32_MIN, INT32_MAX));
    ASSERT_EQ(fm_mul_q16(a, b),
              clamp64((p + 0x8000) >> 16, INT32_MIN, INT32_MAX));
    int64_t acc = (int64_t)a * (int64_t)(b >> 2);
    acc = fm_mac_q31(acc, a, b);
    ASSERT_EQ(acc, (int64_t)a * (b >> 2) + (int64_t)a * b);
    ASSERT_EQ(fm_q62_to_q31(acc),
              clamp64(((i128)acc + ((i128)1 << 30)) >> 31, INT32_MIN,
                      INT32_MAX));
  }
}

void test_fm_fixed_convert(void) {
  for (int32_t q = -32768; q < 32768; q++) {
    ASSERT_TRUE(fm_q15_to_f32((fm_q15)q) == (float)q / 32768.0f);
  }
  static const uint32_t edges[] = {
      0x00000000U, 0x80000000U, 0x00000001U, 0x3F800000U, 0xBF800000U,
      0x3F7FFFFFU, 0x3E800000U, 0x37800000U, 0x37000000U, 0x36FFFFFFU,
      0x37C00000U, 0x4F000000U, 0xCF000000U, 0x4EFFFFFFU, 0x47000000U,
      0xC7000000U, 0x7F800000U, 0xFF800000U, 0x7FC00000U, 0xFFFFFFFFU};
  for (size_t i = 0U; i < sizeof edges / sizeof edges[0]; i++) {
    float x = f32_bits(edges[i]);
    ASSERT_EQ(fm_f32_to_q15(x), ref_to_fixed(x, 15, -32768, 32767));
    ASSERT_EQ(fm_f32_to_q31(x), ref_to_fixed(x, 31, INT32_MIN, INT32_MAX));
    ASSERT_EQ(fm_f32_to_q16(x), ref_to_fixed(x, 16, INT32_MIN, INT32_MAX));
  }
  uint64_t st = 121U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    /* exponents around each format's range, both signs */
    float x = f32_bits((u & 0x807FFFFFU) | (100U + (u >> 23) % 60U) << 23);
    ASSERT_EQ(fm_f32_to_q15(x), ref_to_fixed(x, 15, -32768, 32767));
    ASSERT_EQ(fm_f32_to_q31(x), ref_to_fixed(x, 31, INT32_MIN, INT32_MAX));
    ASSERT_EQ(fm_f32_to_q16(x), ref_to_fixed(x, 16, INT32_MIN, INT32_MAX));
    int32_t q = (int32_t)u >> (u & 15U);
    ASSERT_EQ(bits_f32(fm_q31_to_f32(q)),
              bits_f32((float)ldexp((double)q, -31)));
    ASSERT_EQ(bits_f32(fm_q16_to_f32(q)),
              bits_f32((float)ldexp((double)q, -16)));
  }
}
