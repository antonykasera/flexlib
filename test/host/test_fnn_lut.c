/**
 * @file test_fnn_lut.c
 * @brief Host tests for fnn_lut.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_lut.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

static double logistic(double v) { return 1.0 / (1.0 + exp(-v)); }
static double hswish(double v) {
  return v * fmin(fmax(v + 3.0, 0.0), 6.0) / 6.0;
}
static double half(double v) { return v / 2.0; }
static double nan_f(double v) { return v * (double)NAN; }
static double big(double v) { return v * (double)INFINITY; }

static unsigned check(double (*f)(double), double s_x, int32_t z_x, double s_y,
                      int32_t z_y) {
  int8_t lut[256];
  if (fnn_lut_s8_build(f, s_x, z_x, s_y, z_y, lut) != FNN_OK) {
    return 1U;
  }
  unsigned bad = 0U;
  for (int32_t q = -128; q < 128; q++) {
    double v = f(s_x * (double)(q - z_x)) / s_y + (double)z_y;
    int32_t got = lut[(uint8_t)q];
    if (isnan(v)) {
      bad += got != -128;
    } else if (v <= -128.0 || v >= 127.0) {
      bad += got != (v < 0.0 ? -128 : 127);
    } else {
      double d = (double)got - v;
      bad += !(d > -0.5 && d <= 0.5);
    }
  }
  return bad;
}

void test_fnn_lut_build(void) {
  ASSERT_EQ(check(tanh, 1.0 / 16.0, 0, 1.0 / 128.0, 0), 0U);
  ASSERT_EQ(check(tanh, 0.05, -7, 1.0 / 128.0, 0), 0U);
  ASSERT_EQ(check(logistic, 1.0 / 16.0, 0, 1.0 / 256.0, -128), 0U);
  ASSERT_EQ(check(logistic, 0.1, 30, 1.0 / 256.0, -128), 0U);
  ASSERT_EQ(check(hswish, 0.05, -10, 0.03, -100), 0U);
  ASSERT_EQ(check(exp, 0.02, 127, 0.1, -128), 0U);
  ASSERT_EQ(check(half, 1.0, 0, 1.0, 0), 0U);

  /* Halving makes a tie at every odd q, and each one rounds up. */
  int8_t lut[256];
  ASSERT_EQ(fnn_lut_s8_build(half, 1.0, 0, 1.0, 0, lut), FNN_OK);
  ASSERT_EQ(lut[(uint8_t)1], 1);
  ASSERT_EQ(lut[(uint8_t)-1], 0);
  ASSERT_EQ(lut[(uint8_t)-127], -63);
  ASSERT_EQ(lut[(uint8_t)127], 64);
  ASSERT_EQ(lut[(uint8_t)-128], -64);

  /* 127.5 clamps to 127, and -127.5 rounds up to -127. */
  ASSERT_EQ(fnn_lut_s8_build(half, 1.0, 0, 1.0 / 4.0, 0, lut), FNN_OK);
  ASSERT_EQ(lut[(uint8_t)-128], -128);
  ASSERT_EQ(lut[(uint8_t)-65], -128);
  ASSERT_EQ(lut[(uint8_t)-64], -128);
  ASSERT_EQ(lut[(uint8_t)-63], -126);
  ASSERT_EQ(lut[(uint8_t)63], 126);
  ASSERT_EQ(lut[(uint8_t)64], 127);
  ASSERT_EQ(lut[(uint8_t)127], 127);
  ASSERT_EQ(fnn_lut_s8_build(half, 1.0, 0, 1.0 / 255.0, 0, lut), FNN_OK);
  ASSERT_EQ(lut[1], 127);
  ASSERT_EQ(lut[(uint8_t)-1], -127);
  ASSERT_EQ(lut[(uint8_t)-2], -128);
}

void test_fnn_lut_build_edges(void) {
  int8_t lut[256];
  ASSERT_EQ(fnn_lut_s8_build(nan_f, 1.0, 0, 1.0, 0, lut), FNN_OK);
  unsigned bad = 0U;
  for (unsigned i = 0U; i < 256U; i++) {
    bad += lut[i] != -128;
  }
  ASSERT_EQ(bad, 0U);

  /* f gives minus infinity below the zero point, NaN at it, infinity above. */
  ASSERT_EQ(fnn_lut_s8_build(big, 1.0, 0, 1.0, 0, lut), FNN_OK);
  ASSERT_EQ(lut[(uint8_t)-128], -128);
  ASSERT_EQ(lut[(uint8_t)-1], -128);
  ASSERT_EQ(lut[0], -128);
  ASSERT_EQ(lut[1], 127);
  ASSERT_EQ(lut[127], 127);

  static const double badscale[] = {0.0, -1.0, (double)NAN, (double)INFINITY,
                                    -0.0};
  for (unsigned i = 0U; i < sizeof badscale / sizeof badscale[0]; i++) {
    lut[0] = 55;
    ASSERT_EQ(fnn_lut_s8_build(tanh, badscale[i], 0, 1.0, 0, lut),
              FNN_ERR_QUANT);
    ASSERT_EQ(fnn_lut_s8_build(tanh, 1.0, 0, badscale[i], 0, lut),
              FNN_ERR_QUANT);
    ASSERT_EQ(lut[0], 55);
  }
  ASSERT_EQ(fnn_lut_s8_build(tanh, 1e-300, 0, 1e300, 0, lut), FNN_OK);
}

void test_fnn_lut_apply(void) {
  static int8_t lut[256], x[1100], y[1100], z[1100];
  uint64_t r = 0x7AB1EU;
  for (unsigned i = 0U; i < 256U; i++) {
    lut[i] = (int8_t)test_rng_u32(&r);
  }
  for (unsigned i = 0U; i < sizeof x; i++) {
    x[i] = (int8_t)test_rng_u32(&r);
  }
  unsigned bad = 0U;
  for (uint32_t n = 0U; n <= 1030U; n = n < 40U ? n + 1U : n + 99U) {
    for (uint32_t off = 0U; off < 4U; off++) {
      for (unsigned i = 0U; i < sizeof y; i++) {
        y[i] = 77;
      }
      fnn_lut_s8(x + off, y + 4U - off, n, lut);
      for (uint32_t i = 0U; i < n; i++) {
        bad += y[4U - off + i] != lut[(uint8_t)x[off + i]];
      }
      for (uint32_t i = 0U; i < 4U - off; i++) {
        bad += y[i] != 77;
      }
      for (uint32_t i = 4U - off + n; i < sizeof y; i++) {
        bad += y[i] != 77;
      }

      for (uint32_t i = 0U; i < n; i++) {
        z[off + i] = x[off + i];
      }
      fnn_lut_s8(z + off, z + off, n, lut);
      for (uint32_t i = 0U; i < n; i++) {
        bad += z[off + i] != lut[(uint8_t)x[off + i]];
      }
    }
  }
  ASSERT_EQ(bad, 0U);

  for (int32_t q = -128; q < 128; q++) {
    x[q + 128] = (int8_t)q;
  }
  fnn_lut_s8(x, y, 256U, lut);
  bad = 0U;
  for (int32_t q = -128; q < 128; q++) {
    bad += y[q + 128] != lut[(uint8_t)q];
  }
  ASSERT_EQ(bad, 0U);
}
