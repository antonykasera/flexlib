/**
 * @file test_fd_bfp.c
 * @brief Host tests for the block floating point conversions (fd_bfp.h)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_bfp.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

#define N 64U

static float f_of(uint32_t u) {
  float f;
  memcpy(&f, &u, sizeof f);
  return f;
}

static int32_t ref_exp(const float *x, uint32_t n) {
  int32_t e = -125;
  for (uint32_t i = 0U; i < n; i++) {
    int k = 0;
    if (x[i] != 0.0f) {
      (void)frexp((double)x[i], &k);
      e = k > e ? k : e;
    }
  }
  return e;
}

/* blocks of mixed magnitudes: loud values next to ones 2^-30 below */
void test_fd_bfp_blocks(void) {
  uint64_t r = 0xBF9U;
  float x[N], y[N];
  int32_t q[N];
  for (unsigned blk = 0U; blk < 20000U; blk++) {
    uint32_t base = 1U + test_rng_u32(&r) % 254U;
    uint32_t spread = test_rng_u32(&r) % 40U;
    for (uint32_t i = 0U; i < N; i++) {
      uint32_t ex = base - test_rng_u32(&r) % (spread + 1U);
      ex = (int32_t)ex < 0 ? 0U : ex; /* subnormals near the bottom */
      x[i] = f_of((test_rng_u32(&r) & 0x807FFFFFU) | (ex << 23));
    }
    if (blk % 7U == 0U) {
      x[blk % N] = 0.0f;
    }
    int32_t e = fd_bfp_exp(x, N);
    ASSERT_EQ(e, ref_exp(x, N));

    fd_bfp_to_q31(x, q, N, e);
    for (uint32_t i = 0U; i < N; i++) {
      ASSERT_EQ(q[i], (int32_t)nearbyint(ldexp((double)x[i], 31 - e)));
    }
    fd_bfp_to_f32(q, y, N, e);
    for (uint32_t i = 0U; i < N; i++) {
      float want = (float)ldexp((double)q[i], e - 31);
      ASSERT_TRUE(memcmp(&y[i], &want, sizeof want) == 0);
      int k = 0;
      (void)frexp((double)x[i], &k);
      if (e - k < 8) { /* within 2^7 of 2^e*/
        ASSERT_TRUE(y[i] == x[i]);
      }
    }

    /* in place, both ways */
    memcpy(y, x, sizeof y);
    fd_bfp_to_q31(y, (int32_t *)(void *)y, N, e);
    ASSERT_TRUE(memcmp(y, q, sizeof q) == 0);
    fd_bfp_to_f32((int32_t *)(void *)y, y, N, e);
    float back[N];
    fd_bfp_to_f32(q, back, N, e);
    ASSERT_TRUE(memcmp(y, back, sizeof back) == 0);
  }
}

void test_fd_bfp_edges(void) {
  float x[4] = {1.0f, -2.0f, 0.5f, 0.0f};
  ASSERT_EQ(fd_bfp_exp(x, 0U), -125);
  ASSERT_EQ(fd_bfp_exp(x, 4U), 2);
  x[2] = INFINITY;
  ASSERT_EQ(fd_bfp_exp(x, 4U), FD_BFP_BAD);
  x[2] = -NAN;
  ASSERT_EQ(fd_bfp_exp(x, 4U), FD_BFP_BAD);
  x[2] = f_of(1U); /* the smallest subnormal */
  ASSERT_EQ(fd_bfp_exp(x + 2, 2U), -125);

  /* the largest magnitude at its own exponent: m 2^7, exact */
  float big = f_of(0x7F7FFFFFU);
  int32_t q;
  fd_bfp_to_q31(&big, &q, 1U, fd_bfp_exp(&big, 1U));
  ASSERT_EQ(q, 0x7FFFFF80);
  float neg = -big;
  fd_bfp_to_q31(&neg, &q, 1U, 128);
  ASSERT_EQ(q, -0x7FFFFF80);

  /* ties to even at the half bit, in both directions */
  float t = 1.5f; /* at e = 31: 1.5 exactly between 1 and 2 */
  fd_bfp_to_q31(&t, &q, 1U, 31);
  ASSERT_EQ(q, 2);
  t = 2.5f;
  fd_bfp_to_q31(&t, &q, 1U, 31);
  ASSERT_EQ(q, 2);
  int32_t v = 0x01000001; /* 2^24 + 1: a tie, to even */
  float y;
  fd_bfp_to_f32(&v, &y, 1U, 31);
  ASSERT_TRUE(y == 16777216.0f);
  v = 0x01000003;
  fd_bfp_to_f32(&v, &y, 1U, 31);
  ASSERT_TRUE(y == 16777220.0f);

  /* overflow to infinity, underflow through the subnormals to signed zero */
  v = INT32_MIN;
  fd_bfp_to_f32(&v, &y, 1U, 129);
  ASSERT_TRUE(isinf(y) && y < 0.0f);
  v = 3;
  fd_bfp_to_f32(&v, &y, 1U, -150);
  ASSERT_TRUE(memcmp(&y, &(float){f_of(0U)}, sizeof y) == 0);
  v = -3;
  fd_bfp_to_f32(&v, &y, 1U, -119); /* 1.5 smallest subnormals: a tie, to 2 */
  ASSERT_TRUE(y == -f_of(2U));
}

/* 2^24 random bit patterns both ways: the exhaustive check's sample */
void test_fd_bfp_sampled(void) {
  uint64_t r = 0x5A3U;
  unsigned long bad = 0U;
  for (unsigned i = 0U; i < (1U << 24); i++) {
    uint32_t u = test_rng_u32(&r);
    float x = f_of(u);
    int32_t e0 = fd_bfp_exp(&x, 1U);
    if (isfinite(x)) {
      int32_t e = e0 + (int32_t)(u % 41U);
      int32_t q;
      fd_bfp_to_q31(&x, &q, 1U, e);
      bad += (double)q != nearbyint(ldexp((double)x, 31 - e));
    } else {
      bad += e0 != FD_BFP_BAD;
    }
    int32_t e = (int32_t)(test_rng_u32(&r) % 340U) - 180;
    int32_t v = (int32_t)u;
    float y;
    fd_bfp_to_f32(&v, &y, 1U, e);
    float want = (float)ldexp((double)v, e - 31);
    bad += memcmp(&y, &want, sizeof y) != 0;
  }
  ASSERT_EQ(bad, 0);
}
