/**
 * @file test_fnn_softmax.c
 * @brief Host tests for fnn_softmax.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_softmax.h"
#include "test_assert.h"
#include "test_stats.h"
#include <math.h>

#define NMAX 1100U

static int8_t x[NMAX], y[NMAX + 1U];
static uint32_t tab[256];

/* The exact result, by 64-bit division. */
static int8_t model(const int8_t *v, uint32_t n, uint32_t i) {
  int32_t mx = v[0];
  for (uint32_t j = 1U; j < n; j++) {
    mx = v[j] > mx ? v[j] : mx;
  }
  uint64_t s = 0U;
  for (uint32_t j = 0U; j < n; j++) {
    s += tab[mx - v[j]];
  }
  uint64_t q = (512U * (uint64_t)tab[mx - v[i]] + s) / (2U * s);
  return q > 255U ? 127 : (int8_t)((int32_t)q - 128);
}

static unsigned run(uint32_t n, double k) {
  y[n] = 55;
  fnn_softmax_s8(x, y, n, tab);
  unsigned bad = y[n] != 55;
  int32_t mx = x[0];
  for (uint32_t j = 1U; j < n; j++) {
    mx = x[j] > mx ? x[j] : mx;
  }
  long double sum = 0.0L;
  for (uint32_t j = 0U; j < n; j++) {
    sum += expl(-(long double)k * (long double)(mx - x[j]));
  }
  long double tol = 1.0L / 512.0L + (long double)(n + 1U) * ldexpl(1.0L, -32);
  for (uint32_t i = 0U; i < n; i++) {
    bad += y[i] != model(x, n, i);
    long double p = expl(-(long double)k * (long double)(mx - x[i])) / sum;
    long double got = ((long double)y[i] + 128.0L) / 256.0L;
    if (p <= 255.5L / 256.0L) {
      bad += fabsl(got - p) > tol;
    } else {
      bad += y[i] != 127;
    }
  }
  return bad;
}

void test_fnn_softmax_build(void) {
  static const double ks[] = {1.0 / 16.0, 0.003, 0.5, 2.0, 40.0, 1e-9};
  unsigned bad = 0U;
  for (unsigned c = 0U; c < sizeof ks / sizeof ks[0]; c++) {
    ASSERT_EQ(fnn_softmax_s8_build(1.0, ks[c], tab), FNN_OK);
    bad += tab[0] != 1U << 31;
    for (uint32_t d = 0U; d < 256U; d++) {
      long double v = ldexpl(expl(-(long double)ks[c] * (long double)d), 31);
      bad += fabsl((long double)tab[d] - v) > 0.5L + 1e-6L;
      bad += d > 0U && tab[d] > tab[d - 1U];
    }
  }
  ASSERT_EQ(bad, 0U);

  /* beta and the scale only matter as a product */
  static uint32_t t2[256];
  ASSERT_EQ(fnn_softmax_s8_build(2.0, 0.125, t2), FNN_OK);
  ASSERT_EQ(fnn_softmax_s8_build(1.0, 0.25, tab), FNN_OK);
  bad = 0U;
  for (uint32_t d = 0U; d < 256U; d++) {
    bad += tab[d] != t2[d];
  }
  ASSERT_EQ(bad, 0U);

  static const double badk[] = {0.0, -1.0, (double)NAN, (double)INFINITY};
  for (unsigned i = 0U; i < sizeof badk / sizeof badk[0]; i++) {
    tab[0] = 55U;
    ASSERT_EQ(fnn_softmax_s8_build(1.0, badk[i], tab), FNN_ERR_QUANT);
    ASSERT_EQ(fnn_softmax_s8_build(badk[i], 1.0, tab), FNN_ERR_QUANT);
    ASSERT_EQ(tab[0], 55U);
  }
  ASSERT_EQ(fnn_softmax_s8_build(1e200, 1e200, tab), FNN_ERR_QUANT);
}

void test_fnn_softmax_random(void) {
  static const double ks[] = {1.0 / 16.0, 0.01, 0.2, 1.0, 1e-4};
  static const uint32_t ns[] = {1,  2,  3,  5,   10,  12,   16,
                                17, 64, 65, 255, 256, 1000, 1100};
  uint64_t r = 0x50F7AU;
  unsigned bad = 0U;
  for (unsigned c = 0U; c < sizeof ks / sizeof ks[0]; c++) {
    ASSERT_EQ(fnn_softmax_s8_build(1.0, ks[c], tab), FNN_OK);
    for (unsigned a = 0U; a < sizeof ns / sizeof ns[0]; a++) {
      for (unsigned rep = 0U; rep < 20U; rep++) {
        uint32_t span = rep % 2U ? 256U : 1U + test_rng_u32(&r) % 16U;
        int32_t base = (int32_t)(test_rng_u32(&r) % (257U - span)) - 128;
        for (uint32_t i = 0U; i < ns[a]; i++) {
          x[i] = (int8_t)(base + (int32_t)(test_rng_u32(&r) % span));
        }
        bad += run(ns[a], ks[c]);
      }
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_softmax_edges(void) {
  ASSERT_EQ(fnn_softmax_s8_build(1.0, 1.0 / 16.0, tab), FNN_OK);

  /* one score is certain, and certainty clamps to 255 / 256 */
  x[0] = -128;
  fnn_softmax_s8(x, y, 1U, tab);
  ASSERT_EQ(y[0], 127);

  /* n equal scores give 1 / n each: 512 of them is a tie that rounds up */
  static const struct {
    uint32_t n;
    int8_t want;
  } eq[] = {{2, 0}, {4, -64}, {256, -127}, {512, -127}, {513, -128}, {3, -43}};
  for (unsigned c = 0U; c < sizeof eq / sizeof eq[0]; c++) {
    for (uint32_t i = 0U; i < eq[c].n; i++) {
      x[i] = 7;
    }
    ASSERT_EQ(run(eq[c].n, 1.0 / 16.0), 0U);
    ASSERT_EQ(y[0], eq[c].want);
    ASSERT_EQ(y[eq[c].n - 1U], eq[c].want);
  }

  ASSERT_EQ(fnn_softmax_s8_build(1.0, 0.05, tab), FNN_OK);
  for (uint32_t i = 0U; i < 20U; i++) {
    x[i] = (int8_t)(i % 2U ? 127 : -128);
  }
  ASSERT_EQ(run(20U, 0.05), 0U);
  int8_t z[20];
  for (uint32_t i = 0U; i < 20U; i++) {
    z[i] = x[i];
  }
  fnn_softmax_s8(z, z, 20U, tab);
  unsigned bad = 0U;
  for (uint32_t i = 0U; i < 20U; i++) {
    bad += z[i] != y[i];
  }
  ASSERT_EQ(bad, 0U);

  /* Entries just below a tie, where a high estimate gives 90 and 6. */
  static const struct {
    uint32_t t1, t2, n_max;
    int8_t want;
  } near[] = {{1323689169U, 357803769U, 1U, 88 - 128},
              {927377820U, 362410684U, 9U, 4 - 128}};
  for (unsigned c = 0U; c < sizeof near / sizeof near[0]; c++) {
    tab[1] = near[c].t1;
    tab[2] = near[c].t2;
    uint32_t n = near[c].n_max + 2U;
    for (uint32_t i = 0U; i < near[c].n_max; i++) {
      x[i] = 0;
    }
    x[n - 2U] = -1;
    x[n - 1U] = -2;
    fnn_softmax_s8(x, y, n, tab);
    ASSERT_EQ(y[c == 0U ? n - 2U : n - 1U], near[c].want);
    ASSERT_EQ(y[n - 2U], model(x, n, n - 2U));
    ASSERT_EQ(y[n - 1U], model(x, n, n - 1U));
  }

  for (uint32_t d = 0U; d < 256U; d++) {
    tab[d] = 1U << 31;
  }
  for (uint32_t i = 0U; i < NMAX; i++) {
    x[i] = (int8_t)(i * 37U);
  }
  bad = 0U;
  for (uint32_t n = 1U; n <= NMAX; n += 13U) {
    y[n] = 55;
    fnn_softmax_s8(x, y, n, tab);
    bad += y[n] != 55;
    for (uint32_t i = 0U; i < n; i++) {
      bad += y[i] != model(x, n, i);
    }
  }
  ASSERT_EQ(bad, 0U);
}

void test_fnn_argmax(void) {
  uint64_t r = 0xA4C3AU;
  unsigned bad = 0U;
  for (uint32_t n = 1U; n <= 300U; n++) {
    for (uint32_t i = 0U; i < n; i++) {
      x[i] = (int8_t)(test_rng_u32(&r) % (n < 50U ? 5U : 256U));
    }
    uint32_t k = 0U;
    for (uint32_t i = 1U; i < n; i++) {
      k = x[i] > x[k] ? i : k;
    }
    bad += fnn_argmax_s8(x, n) != k;
  }
  ASSERT_EQ(bad, 0U);

  for (uint32_t i = 0U; i < 40U; i++) {
    x[i] = -128;
  }
  ASSERT_EQ(fnn_argmax_s8(x, 40U), 0U);
  x[39] = -127;
  ASSERT_EQ(fnn_argmax_s8(x, 40U), 39U);
  x[17] = 127;
  x[30] = 127;
  ASSERT_EQ(fnn_argmax_s8(x, 40U), 17U);
  ASSERT_EQ(fnn_argmax_s8(x, 1U), 0U);
}

/* Tables whose entry for d = 1 sits next to a tie. */
void test_fnn_softmax_ties(void) {
  uint64_t r = 0x7135U;
  unsigned bad = 0U, cases = 0U;
  while (cases < 100000U) {
    uint64_t m = 1U + test_rng_u32(&r) % 32U;
    uint64_t f = test_rng_u32(&r) >> 1;
    uint64_t k = test_rng_u32(&r) % 256U;
    uint64_t e = (2U * k + 1U) * ((m << 31) + f) / (511U - 2U * k);
    e += test_rng_u32(&r) % 7U;
    if (e < 3U || e - 3U > 1U << 31) {
      continue;
    }
    tab[1] = (uint32_t)(e - 3U);
    tab[2] = (uint32_t)f;
    uint32_t n = (uint32_t)m + 2U;
    for (uint32_t i = 0U; i < m; i++) {
      x[i] = 5;
    }
    x[n - 2U] = 4;
    x[n - 1U] = 3;
    fnn_softmax_s8(x, y, n, tab);
    for (uint32_t i = n - 3U; i < n; i++) {
      bad += y[i] != model(x, n, i);
    }
    cases++;
  }
  ASSERT_EQ(bad, 0U);
}
