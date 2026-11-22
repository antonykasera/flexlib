/**
 * @file test_fnn_quant.c
 * @brief Host tests for fnn_quant.h, in the runner's rounding mode
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

#define MODE REF_MODE
#define COUNT(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))

static const int32_t accs[] = {0,
                               1,
                               -1,
                               2,
                               -2,
                               127,
                               -128,
                               255,
                               -256,
                               32767,
                               -32768,
                               1 << 20,
                               -(1 << 20),
                               INT32_MAX,
                               INT32_MIN,
                               INT32_MAX - 1,
                               INT32_MIN + 1};
static const int32_t mults[] = {0,          1,          1 << 30,  (1 << 30) + 1,
                                1518500250, 2147483646, INT32_MAX};

void test_fnn_requant_grid(void) {
  uint64_t r = 0x51A7U;
  for (int32_t rsh = -31; rsh <= 30; rsh++) {
    for (unsigned mi = 0U; mi < COUNT(mults) + 64U; mi++) {
      int32_t m =
          mi < COUNT(mults) ? mults[mi] : (int32_t)(test_rng_u32(&r) >> 1);
      for (unsigned ai = 0U; ai < COUNT(accs) + 64U; ai++) {
        int32_t a = ai < COUNT(accs)
                        ? accs[ai]
                        : (int32_t)test_rng_u32(&r) >> (test_rng_u32(&r) % 32U);
        ASSERT_EQ(fnn_requant(a, m, rsh), ref_requant(a, m, -1 - rsh, MODE));
      }
    }
  }
}

/* at a scale of 2^-9 these sums are exact ties, where the modes disagree */
void test_fnn_requant_ties(void) {
  static const struct {
    int32_t acc, want[3]; /* up, away, gemmlowp */
  } t[] = {
      {256, {1, 1, 1}},  {-256, {0, -1, -1}},  {255, {0, 0, 1}},
      {-255, {0, 0, 0}}, {768, {2, 2, 2}},     {-768, {-1, -2, -2}},
      {767, {1, 1, 2}},  {-767, {-1, -1, -1}}, {0, {0, 0, 0}},
  };
  for (unsigned i = 0U; i < COUNT(t); i++) {
    ASSERT_EQ(fnn_requant(t[i].acc, 1 << 30, 7), t[i].want[REF_MODE]);
  }
}

void test_fnn_requant_block(void) {
  static const int32_t m[3] = {1 << 30, 1518500250, INT32_MAX};
  static const int8_t sh[3] = {7, 12, -2};
  const FNN_Quant q = {m, sh, 3U};
  const FNN_Out o = {-5, -128, 100};
  const FNN_Quant q1 = {m + 1, sh + 1, 1U};
  int32_t acc[48];
  int8_t y[48];
  uint64_t r = 0xB10CU;
  for (unsigned rep = 0U; rep < 200U; rep++) {
    for (unsigned i = 0U; i < 48U; i++) {
      acc[i] = (int32_t)test_rng_u32(&r) >> (test_rng_u32(&r) % 24U);
    }
    fnn_requant_s8(acc, y, 48U, &q, &o);
    for (unsigned i = 0U; i < 48U; i++) {
      int64_t v =
          (int64_t)ref_requant(acc[i], m[i % 3U], -1 - sh[i % 3U], MODE) - 5;
      ASSERT_EQ(y[i], v < -128 ? -128 : (v > 100 ? 100 : v));
    }
    fnn_requant_s8(acc, y, 48U, &q1, &o);
    for (unsigned i = 0U; i < 48U; i++) {
      int64_t v = (int64_t)ref_requant(acc[i], m[1], -1 - sh[1], MODE) - 5;
      ASSERT_EQ(y[i], v < -128 ? -128 : (v > 100 ? 100 : v));
    }
  }
  fnn_requant_s8(acc, y, 0U, &q, &o);
}

void test_fnn_clamp(void) {
  ASSERT_EQ(fnn_clamp_s8(INT32_MAX, 127, -128, 127), 127);
  ASSERT_EQ(fnn_clamp_s8(INT32_MIN, -128, -128, 127), -128);
  ASSERT_EQ(fnn_clamp_s8(-3, -128, -128, 127), -128);
  ASSERT_EQ(fnn_clamp_s8(-3, 10, 10, 127), 10); /* fused ReLU */
  ASSERT_EQ(fnn_clamp_s8(50, 10, 10, 50), 50);  /* ReLU6 bound */
  ASSERT_EQ(fnn_clamp_s8(41, 10, 10, 50), 50);
  ASSERT_EQ(fnn_clamp_s8(39, 10, 10, 50), 49);
}

static void check_scale(double s) {
  int32_t m, want_m, want_sh;
  int8_t rsh;
  ASSERT_EQ(fnn_quant_from_scale(s, &m, &rsh), FNN_OK);
  ref_quantize_multiplier(s, &want_m, &want_sh);
  ASSERT_EQ(m, want_m);
  ASSERT_EQ(rsh, -1 - want_sh);
}

void test_fnn_quant_from_scale(void) {
  static const double s[] = {0.0,
                             1.0,
                             0.5,
                             0.25,
                             1.0 - 0x1p-40, /* rounds up to 2^31 */
                             0x1p-31,
                             0x1p-32,
                             0x1p-33, /* too small: zero */
                             0x1.fffffp-33,
                             0x1p30,
                             0x1p31, /* too large: the largest */
                             1e30,
                             3.0517578125e-05,
                             0.0039215686274509803,
                             1e-300,
                             5e-324};
  for (unsigned i = 0U; i < COUNT(s); i++) {
    check_scale(s[i]);
  }
  uint64_t r = 0x5CA1EU;
  for (unsigned i = 0U; i < 100000U; i++) {
    uint64_t u = test_rng_next(&r);
    check_scale(ldexp((double)(u >> 11) * 0x1p-53, (int)(u % 48U) - 40));
  }
  int32_t m;
  int8_t rsh;
  ASSERT_EQ(fnn_quant_from_scale(-1.0, &m, &rsh), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_quant_from_scale((double)NAN, &m, &rsh), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_quant_from_scale((double)INFINITY, &m, &rsh), FNN_ERR_QUANT);
}

void test_fnn_quant_from_tflite(void) {
  int32_t m;
  int8_t rsh;
  ASSERT_EQ(fnn_quant_from_tflite(1 << 30, -7, &m, &rsh), FNN_OK);
  ASSERT_EQ(m, 1 << 30);
  ASSERT_EQ(rsh, 6);
  ASSERT_EQ(fnn_quant_from_tflite(INT32_MAX, 30, &m, &rsh), FNN_OK);
  ASSERT_EQ(rsh, -31);
  ASSERT_EQ(fnn_quant_from_tflite(0, -31, &m, &rsh), FNN_OK);
  ASSERT_EQ(rsh, 30);
  ASSERT_EQ(fnn_quant_from_tflite(-1, 0, &m, &rsh), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_quant_from_tflite(1, 31, &m, &rsh), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_quant_from_tflite(1, -32, &m, &rsh), FNN_ERR_QUANT);
}
