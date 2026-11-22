/**
 * @file test_fnn_dense.c
 * @brief Host tests for fnn_dense.h against TFLite's reference arithmetic
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dense.h"
#include "fnn_quant.h"
#include "ref_nn.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

#define MODE ((RefRound)FNN_ROUNDING)
#define NMAX 260U
#define OMAX 70U

static int8_t x[NMAX], w[NMAX * OMAX], y[OMAX + 1U], want[OMAX];
static int8_t wp[NMAX * OMAX] FX_ALIGN(4);
static int32_t b[OMAX], bf[OMAX], mult[OMAX], shift[OMAX];
static int8_t rsh[OMAX];

static int8_t rand_s8(uint64_t *r, int lo) {
  return (int8_t)(lo + (int32_t)(test_rng_u32(r) % (uint32_t)(128 - lo)));
}

static unsigned run(uint32_t n_in, uint32_t n_out, int per_channel, int32_t zx,
                    int32_t zy, int32_t lo, int32_t hi, const int32_t *bias) {
  uint32_t nq = per_channel ? n_out : 1U;
  for (uint32_t c = 0U; c < nq; c++) {
    rsh[c] = (int8_t)(-1 - shift[c]);
  }
  fnn_fold_bias_s8(w, n_in, n_out, bias, zx, bf);
  ref_fully_connected(x, n_in, w, n_out, bias, zx, mult, shift, per_channel, zy,
                      lo, hi, MODE, want);
  fnn_dense_pack_s8(w, n_in, n_out, wp);
  unsigned bad = 0U;
  for (uint32_t packed = 0U; packed < 2U; packed++) {
    const FNN_Dense l = {packed ? wp : w, bf,          n_in, n_out, packed,
                         {mult, rsh, nq}, {zy, lo, hi}};
    y[n_out] = 55;
    fnn_dense_s8(&l, x, y);
    bad += y[n_out] != 55;
    for (uint32_t o = 0U; o < n_out; o++) {
      bad += y[o] != want[o];
    }
  }
  return bad;
}

void test_fnn_dense_random(void) {
  static const uint32_t ins[] = {0,  1,  2,  3,  4,  5,  7,  8,
                                 15, 16, 17, 63, 64, 65, 256};
  static const uint32_t outs[] = {1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13, 64, 70};
  uint64_t r = 0xDE45EU;
  unsigned bad = 0U;
  for (unsigned rep = 0U; rep < 20U; rep++) {
    for (unsigned a = 0U; a < sizeof ins / sizeof ins[0]; a++) {
      for (unsigned c = 0U; c < sizeof outs / sizeof outs[0]; c++) {
        uint32_t n = ins[a], m = outs[c];
        for (uint32_t i = 0U; i < n; i++) {
          x[i] = rand_s8(&r, -128);
        }
        for (uint32_t i = 0U; i < n * m; i++) {
          w[i] = rand_s8(&r, -127);
        }
        int per_channel = (int)(test_rng_u32(&r) & 1U);
        for (uint32_t o = 0U; o < m; o++) {
          b[o] = (int32_t)test_rng_u32(&r) >> 12;
          /* a scale that brings a sum of n products near int8 range */
          double s = ldexp(1.0 + (test_rng_u32(&r) % 1000U) / 1000.0,
                           -14 - (int)(test_rng_u32(&r) % 4U)) /
                     sqrt(n + 1.0);
          ref_quantize_multiplier(s, &mult[o], &shift[o]);
        }
        int32_t zx = (int32_t)(test_rng_u32(&r) % 256U) - 128;
        int32_t zy = (int32_t)(test_rng_u32(&r) % 256U) - 128;
        int32_t lo = rep % 3U == 0U ? zy : -128; /* a fused ReLU */
        int32_t hi = rep % 5U == 0U ? (zy + 127) / 2 + 1 : 127;
        hi = hi < lo ? lo : hi;
        bad += run(n, m, per_channel, zx, zy, lo, hi, rep % 7U ? b : NULL);
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_dense_extremes(void) {
  const uint32_t n = 256U, m = 9U;
  for (int sx = 0; sx < 2; sx++) {
    for (int sw = 0; sw < 2; sw++) {
      for (int sz = 0; sz < 2; sz++) {
        memset(x, sx ? 127 : -128, n);
        memset(w, sw ? 127 : -127, n * m);
        for (uint32_t o = 0U; o < m; o++) {
          b[o] = (int32_t)o * 1000 - 4000;
          ref_quantize_multiplier(0x1p-23, &mult[o], &shift[o]);
        }
        ASSERT_EQ(run(n, m, 0, sz ? 127 : -128, 0, -128, 127, b), 0U);
        ASSERT_EQ(run(n, m, 1, sz ? 127 : -128, -128, -128, 127, b), 0U);
      }
    }
  }
}

/* the folded bias wraps past int32, the true sum does not */
void test_fnn_dense_wrap(void) {
  const uint32_t n = 256U, m = 5U;
  memset(x, -128, n);
  memset(w, 127, n * m);
  for (uint32_t o = 0U; o < m; o++) {
    b[o] = INT32_MAX - (int32_t)o;
    ref_quantize_multiplier(0x1p-25, &mult[o], &shift[o]);
  }
  fnn_fold_bias_s8(w, n, m, b, -128, bf);
  ASSERT_TRUE(bf[0] < 0); /* the fold wrapped */
  ASSERT_EQ(run(n, m, 0, -128, 0, -128, 127, b), 0U);
}

void test_fnn_dense_edges(void) {
  ref_quantize_multiplier(0.01, &mult[0], &shift[0]);
  rsh[0] = (int8_t)(-1 - shift[0]);
  int32_t one = 300;
  const FNN_Dense none = {w, &one, 4U, 0U, 0U, {mult, rsh, 1U}, {0, -128, 127}};
  y[0] = 9;
  fnn_dense_s8(&none, x, y);
  ASSERT_EQ(y[0], 9);
  /* no inputs: the output is the bias, requantised */
  const FNN_Dense empty = {
      w, &one, 0U, 1U, 0U, {mult, rsh, 1U}, {0, -128, 127}};
  fnn_dense_s8(&empty, x, y);
  ASSERT_EQ(y[0], 3);
  int32_t folded[2];
  int8_t w2[6] = {1, 2, 3, -4, -5, -6};
  fnn_fold_bias_s8(w2, 3U, 2U, NULL, 10, folded);
  ASSERT_EQ(folded[0], -60);
  ASSERT_EQ(folded[1], 150);
  int32_t in_place[2] = {7, 8};
  fnn_fold_bias_s8(w2, 3U, 2U, in_place, -1, in_place);
  ASSERT_EQ(in_place[0], 13);
  ASSERT_EQ(in_place[1], -7);
}

/* the packed order: groups of eight rows, one of four, then whole rows */
void test_fnn_dense_pack(void) {
  int8_t rows[13 * 3], p[13 * 3];
  for (int i = 0; i < 13 * 3; i++) {
    rows[i] = (int8_t)i;
  }
  fnn_dense_pack_s8(rows, 3U, 13U, p);
  ASSERT_EQ(p[0], 0);   /* row 0, input 0 */
  ASSERT_EQ(p[1], 3);   /* row 1, input 0 */
  ASSERT_EQ(p[8], 1);   /* row 0, input 1 */
  ASSERT_EQ(p[24], 24); /* row 8, input 0 */
  ASSERT_EQ(p[25], 27); /* row 9, input 0 */
  ASSERT_EQ(p[36], 36); /* row 12 as it was */
  ASSERT_EQ(p[38], 38);
}
