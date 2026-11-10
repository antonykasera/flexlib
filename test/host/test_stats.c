/**
 * @file test_stats.c
 * @brief Error statistics for accuracy sweeps
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "test_stats.h"
#include <math.h>

void stats_init(ErrStats *s, const char *name, const char *unit) {
  s->name = name;
  s->unit = unit;
  s->n = 0U;
  s->max_abs = 0.0;
  s->worst_in = 0.0;
  s->sum_abs = 0.0;
  s->sum = 0.0;
}

void stats_add(ErrStats *s, double in, double err) {
  double a = isnan(err) ? HUGE_VAL : fabs(err);
  if (s->n == 0U || a > s->max_abs) {
    s->max_abs = a;
    s->worst_in = in;
  }
  s->n++;
  s->sum_abs += a;
  s->sum += isnan(err) ? 0.0 : err;
}

double stats_mean_abs(const ErrStats *s) {
  return s->n != 0U ? s->sum_abs / (double)s->n : 0.0;
}

double stats_bias(const ErrStats *s) {
  return s->n != 0U ? s->sum / (double)s->n : 0.0;
}

void stats_print(const ErrStats *s) {
  printf("  %-24s n=%-8llu max=%.4g %s (at %.9g)  mean|e|=%.4g  bias=%+.3g\n",
         s->name, (unsigned long long)s->n, s->max_abs, s->unit, s->worst_in,
         stats_mean_abs(s), stats_bias(s));
}

void stats_add_block(ErrStats *s, const double *got, const double *ref,
                     unsigned n, double lsb) {
  for (unsigned i = 0U; i < n; i++) {
    stats_add(s, (double)i, (got[i] - ref[i]) / lsb);
  }
}

void snr_init(SnrStats *s, const char *name) {
  s->name = name;
  s->n = 0U;
  s->sum_ref2 = 0.0;
  s->sum_err2 = 0.0;
  s->max_abs = 0.0;
}

void snr_add(SnrStats *s, double got, double ref) {
  double e = isnan(got) ? HUGE_VAL : got - ref;
  s->n++;
  s->sum_ref2 += ref * ref;
  s->sum_err2 += e * e;
  if (fabs(e) > s->max_abs) {
    s->max_abs = fabs(e);
  }
}

double snr_db(const SnrStats *s) {
  if (s->sum_err2 == 0.0) {
    return HUGE_VAL;
  }
  return 10.0 * log10(s->sum_ref2 / s->sum_err2);
}

void snr_print(const SnrStats *s) {
  printf("  %-24s n=%-8llu snr=%.2f dB  max|e|=%.4g\n", s->name,
         (unsigned long long)s->n, snr_db(s), s->max_abs);
}

double ulp_err_f32(float got, double ref) {
  double g = (double)got;
  if (isnan(g) || isnan(ref)) {
    return (isnan(g) && isnan(ref)) ? 0.0 : HUGE_VAL;
  }
  if (g == ref) {
    return 0.0;
  }
  if (isinf(g) || isinf(ref)) {
    return HUGE_VAL;
  }
  int e = 0;
  (void)frexp(ref, &e); /* |ref| = m * 2^e, 0.5 <= m < 1 */
  int exp2 = e - 1;
  if (ref == 0.0 || exp2 < -126) {
    exp2 = -126;
  }
  return (g - ref) / ldexp(1.0, exp2 - 23);
}

uint64_t test_rng_next(uint64_t *state) {
  uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

uint32_t test_rng_u32(uint64_t *state) {
  return (uint32_t)(test_rng_next(state) >> 32);
}
