/**
 * @file test_assert.h
 * @brief Host-side unit-test harness (macros only)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern const char *test_filter; /* argv[1]: run only tests whose name has it */
extern unsigned tests_run;
extern unsigned tests_failed;
extern unsigned checks_failed;

#define ASSERT_EQ(actual, expected)                                            \
  do {                                                                         \
    int64_t _a = (int64_t)(actual);                                            \
    int64_t _e = (int64_t)(expected);                                          \
    if (_a != _e) {                                                            \
      checks_failed++;                                                         \
      printf("  FAIL %s:%d: %s == %s  (got 0x%llX, want 0x%llX)\n", __FILE__,  \
             __LINE__, #actual, #expected, (unsigned long long)_a,             \
             (unsigned long long)_e);                                          \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      checks_failed++;                                                         \
      printf("  FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #cond);  \
    }                                                                          \
  } while (0)

/* |actual - expected| <= tol */
#define ASSERT_NEAR(actual, expected, tol)                                     \
  do {                                                                         \
    double _a = (double)(actual);                                              \
    double _e = (double)(expected);                                            \
    double _t = (double)(tol);                                                 \
    if (!(fabs(_a - _e) <= _t)) {                                              \
      checks_failed++;                                                         \
      printf("  FAIL %s:%d: %s ~= %s  (got %.17g, want %.17g, tol %g)\n",      \
             __FILE__, __LINE__, #actual, #expected, _a, _e, _t);              \
    }                                                                          \
  } while (0)

#define TEST_RUN(fn)                                                           \
  do {                                                                         \
    if (test_filter != NULL && strstr(#fn, test_filter) == NULL) {             \
      break;                                                                   \
    }                                                                          \
    unsigned _before = checks_failed;                                          \
    tests_run++;                                                               \
    printf("RUN  %s\n", #fn);                                                  \
    fn();                                                                      \
    if (checks_failed != _before) {                                            \
      tests_failed++;                                                          \
      printf("FAIL %s\n", #fn);                                                \
    } else {                                                                   \
      printf("PASS %s\n", #fn);                                                \
    }                                                                          \
  } while (0)

#define TEST_SUMMARY() test_summary()

int test_summary(void);

#endif /* TEST_ASSERT_H */
