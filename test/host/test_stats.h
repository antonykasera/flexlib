/**
 * @file test_stats.h
 * @brief Error statistics for accuracy sweeps
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_STATS_H
#define TEST_STATS_H

#include "test_assert.h"
#include <stdint.h>

typedef struct {
  const char *name;
  const char *unit; /* "lsb", "ulp", ... */
  uint64_t n;
  double max_abs;  /* largest |err| */
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

/* signed ULPs at ref's exponent */
double ulp_err_f32(float got, double ref);

/* splitmix64: reproducible inputs, independent of the fu_rand under test */
uint64_t test_rng_next(uint64_t *state);
uint32_t test_rng_u32(uint64_t *state);

#endif /* TEST_STATS_H */
