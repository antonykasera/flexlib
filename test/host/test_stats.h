/**
 * @file test_stats.h
 * @brief Error statistics for accuracy sweeps, ULP error, seeded test inputs
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 *   ErrStats s;
 *   stats_init(&s, "fm_sin_q15", "lsb");
 *   for (int32_t x = -32768; x < 32768; x++)
 *     stats_add(&s, x, fm_sin_q15(x) - ref(x) * 32768.0);
 *   ASSERT_STATS_MAX(&s, 1.0);   // prints the stats line, fails above 1 lsb
 */

#ifndef TEST_STATS_H
#define TEST_STATS_H

#include "test_assert.h"
#include <stdint.h>

typedef struct {
  const char *name;
  const char *unit; /* "lsb", "ulp", ... */
  uint64_t n;
  double max_abs;  /* largest |err|; NaN errors count as infinite */
  double worst_in; /* input that gave max_abs */
  double sum_abs;
  double sum; /* signed, for the bias */
} ErrStats;

void stats_init(ErrStats *s, const char *name, const char *unit);
void stats_add(ErrStats *s, double in, double err);
double stats_mean_abs(const ErrStats *s);
double stats_bias(const ErrStats *s);
void stats_print(const ErrStats *s);

#define ASSERT_STATS_MAX(s, bound)                                             \
  do {                                                                         \
    stats_print(s);                                                            \
    ASSERT_TRUE((s)->n > 0U && (s)->max_abs <= (double)(bound));               \
  } while (0)

/* Signed error of `got` in ULPs, where an ULP is the float spacing at ref's
 * exponent (2^-149 below the normal range). Matching NaNs or infinities
 * give 0; any other NaN or infinity gives HUGE_VAL (+inf). */
double ulp_err_f32(float got, double ref);

/* splitmix64: reproducible inputs, independent of the fu_rand under test */
uint64_t test_rng_next(uint64_t *state);
uint32_t test_rng_u32(uint64_t *state);

#endif /* TEST_STATS_H */
