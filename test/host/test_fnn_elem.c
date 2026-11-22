/**
 * @file test_fnn_elem.c
 * @brief Host tests for fnn_elem.h and fnn_shape.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_elem.h"
#include "fnn_quant.h"
#include "fnn_shape.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <string.h>

#define MODE REF_MODE

static int8_t x1[65536], x2[65536], y[65536 + 1], want[65536];

/* Adds every pair of int8 values. */
static unsigned all_pairs(double s1, int32_t z1, double s2, int32_t z2,
                          double sy, int32_t zy, int32_t lo, int32_t hi) {
  for (uint32_t i = 0U; i < 65536U; i++) {
    x1[i] = (int8_t)(i & 255U);
    x2[i] = (int8_t)(i >> 8);
  }
  FNN_Add a;
  unsigned bad = fnn_add_init(&a, s1, z1, s2, z2, sy, (FNN_Out){zy, lo, hi}) !=
                 FNN_OK;
  ref_add(x1, z1, s1, x2, z2, s2, sy, zy, lo, hi, MODE, 65536U, want);
  y[65536] = 55;
  fnn_add_s8(&a, x1, x2, y, 65536U);
  bad += y[65536] != 55;
  for (uint32_t i = 0U; i < 65536U; i++) {
    bad += y[i] != want[i];
  }
  return bad;
}

void test_fnn_add_pairs(void) {
  unsigned bad = 0U;
  /* Equal scales, and scales whose sums land on exact ties. */
  bad += all_pairs(0.05, -3, 0.05, -3, 0.05, 5, -128, 127);
  bad += all_pairs(0.05, 0, 0.05, 0, 0.1, 0, -128, 127);
  bad += all_pairs(0.25, 10, 0.125, -20, 1.0, -7, -128, 127);
  bad += all_pairs(0.1, 127, 0.03, -128, 0.07, 0, -128, 127);
  bad += all_pairs(0.02, 5, 0.3, 50, 0.4, -100, -100, 90);
  bad += all_pairs(1.0, -128, 1.0, 127, 1.0 / 3.0, 0, -128, 127);
  bad += all_pairs(0.001, 0, 0.5, 0, 0.002, 3, 3, 127);
  /* An output scale so large that every sum rounds to the zero point. */
  bad += all_pairs(0.01, 9, 0.02, -9, 1e7, -77, -128, 127);
  /* The second multiplier rounds up to 2^29, which puts odd sums on ties. */
  bad += all_pairs(1.0, 0, 0.5 - 0x1p-32, 0, 1.0, 0, -128, 127);
  ASSERT_EQ(bad, 0U);
}

void test_fnn_add_random(void) {
  uint64_t r = 0xADD1U;
  unsigned bad = 0U;
  for (unsigned rep = 0U; rep < 200U; rep++) {
    double s1 = 0.001 + (double)(test_rng_u32(&r) % 100000U) / 100000.0;
    double s2 = 0.001 + (double)(test_rng_u32(&r) % 100000U) / 100000.0;
    double sy = (s1 + s2) * (0.3 + (double)(test_rng_u32(&r) % 3000U) / 1000.0);
    int32_t z1 = (int32_t)(test_rng_u32(&r) % 256U) - 128;
    int32_t z2 = (int32_t)(test_rng_u32(&r) % 256U) - 128;
    int32_t zy = (int32_t)(test_rng_u32(&r) % 256U) - 128;
    uint32_t n = 1U + test_rng_u32(&r) % 300U;
    for (uint32_t i = 0U; i < n; i++) {
      x1[i] = (int8_t)test_rng_u32(&r);
      x2[i] = (int8_t)test_rng_u32(&r);
    }
    FNN_Add a;
    bad += fnn_add_init(&a, s1, z1, s2, z2, sy, (FNN_Out){zy, -128, 127}) !=
           FNN_OK;
    ref_add(x1, z1, s1, x2, z2, s2, sy, zy, -128, 127, MODE, n, want);
    /* In place over either input. */
    int8_t *out = rep % 3U == 0U ? x1 : (rep % 3U == 1U ? x2 : y);
    fnn_add_s8(&a, x1, x2, out, n);
    bad += memcmp(out, want, n) != 0;
  }
  ASSERT_EQ(bad, 0U);
}

#if FNN_RULE == FNN_ROUND_GEMMLOWP
/* With these multipliers the output is (d2 + 1) / 2 rounded down, as
 * gemmlowp's doubling multiply rounds ties up. */
void test_fnn_add_ties(void) {
  const FNN_Add a = {0, 0, {1 << 30, 1 << 11, 1 << 30}, {0, 0, 0},
                     {0, -128, 127}};
  static const int8_t z[4] = {0, 0, 0, 0}, d[4] = {1, -1, 3, -3};
  static const int8_t want4[4] = {1, 0, 2, -1};
  int8_t out[4];
  fnn_add_s8(&a, z, d, out, 4U);
  ASSERT_EQ(memcmp(out, want4, 4U), 0);
}
#else
void test_fnn_add_ties(void) {}
#endif

void test_fnn_add_init(void) {
  FNN_Add a;
  const FNN_Out o = {0, -128, 127};
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 0.1, o), FNN_OK);
  ASSERT_EQ(fnn_add_init(&a, 0.0, 0, 0.1, 0, 0.1, o), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, -0.1, 0, 0.1, o), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 1.0 / 0.0, o), FNN_ERR_QUANT);
  /* 2^19 times smaller than the larger input scale is too small. */
  ASSERT_EQ(fnn_add_init(&a, 1.0, 0, 0.5, 0, 0x1p-19, o), FNN_ERR_QUANT);
  ASSERT_EQ(fnn_add_init(&a, 1.0, 0, 0.5, 0, 0x1.0001p-19, o), FNN_OK);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 128, 0.1, 0, 0.1, o), FNN_ERR_RANGE);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, -129, 0.1, o), FNN_ERR_RANGE);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 0.1, (FNN_Out){128, -128, 127}),
            FNN_ERR_RANGE);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 0.1, (FNN_Out){0, -129, 127}),
            FNN_ERR_RANGE);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 0.1, (FNN_Out){0, -128, 128}),
            FNN_ERR_RANGE);
  ASSERT_EQ(fnn_add_init(&a, 0.1, 0, 0.1, 0, 0.1, (FNN_Out){0, 5, 4}),
            FNN_ERR_RANGE);
}

void test_fnn_concat(void) {
  uint64_t r = 0xCA7U;
  unsigned bad = 0U;
  for (unsigned rep = 0U; rep < 500U; rep++) {
    uint32_t n = 1U + test_rng_u32(&r) % 4U, outer = 1U + test_rng_u32(&r) % 20U;
    uint32_t len[4], total = 0U;
    const int8_t *in[4];
    for (uint32_t i = 0U; i < n; i++) {
      len[i] = test_rng_u32(&r) % 40U;
      /* Inputs at odd offsets, so the word copies run unaligned. */
      in[i] = x1 + 4096U * i + test_rng_u32(&r) % 4U;
      total += len[i];
    }
    for (uint32_t i = 0U; i < 4U * 4096U; i++) {
      x1[i] = (int8_t)test_rng_u32(&r);
    }
    int8_t *out = y + test_rng_u32(&r) % 4U;
    out[outer * total] = 55;
    fnn_concat_s8(in, len, n, outer, out);
    bad += out[outer * total] != 55;
    const int8_t *p = out;
    for (uint32_t o = 0U; o < outer; o++) {
      for (uint32_t i = 0U; i < n; i++, p += len[i - 1U]) {
        bad += memcmp(p, in[i] + o * len[i], len[i]) != 0;
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}
