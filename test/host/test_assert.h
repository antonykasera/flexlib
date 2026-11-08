/**
 * @file test_assert.h
 * @brief Host-side unit-test harness (macros only)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * This header provides only the assertion + runner MACROS. The
 * shared counters and main() live in test_main.c, which also lists every test.
 * Each module's tests go in test_<mod>.c as plain `void test_xxx(void)`
 * functions, declared in test_main.c and invoked via TEST_RUN.
 * Error statistics for accuracy sweeps are in test_stats.h.
 *
 *   // test_foo.c
 *   #include "test_assert.h"
 *   void test_foo_thing(void) { ASSERT_EQ(2 + 2, 4); }
 *
 *   // test_main.c
 *   void test_foo_thing(void);
 *   int main(void) { TEST_RUN(test_foo_thing); return TEST_SUMMARY(); }
 */

#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>

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

/* |actual - expected| <= tol; a NaN on either side fails */
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
