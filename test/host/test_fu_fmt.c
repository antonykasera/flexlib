/**
 * @file test_fu_fmt.c
 * @brief Host tests for fu_fmt.h, against the C library's snprintf
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "test_stats.h"
#include <string.h>

#define CANARY 0x5A

/* checks text, length, NUL, and that nothing past the NUL was written */
static void check_u32(uint32_t v) {
  char want[16];
  char got[16];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, "%lu", (unsigned long)v);
  size_t len = fu_fmt_u32(got, v);
  ASSERT_EQ(len, n);
  ASSERT_EQ(fu_fmt_dec_digits(v), n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[len + 1U], CANARY);
}

static void check_i32(int32_t v) {
  char want[16];
  char got[16];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, "%ld", (long)v);
  size_t len = fu_fmt_i32(got, v);
  ASSERT_EQ(len, n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[len + 1U], CANARY);
}

static void check_hex(uint32_t v, uint32_t digits, bool upper) {
  char want[16];
  char got[16];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, upper ? "%0*lX" : "%0*lx", (int)digits,
                   (unsigned long)v);
  size_t len = fu_fmt_hex32(got, v, digits, upper);
  ASSERT_EQ(len, n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[FU_FMT_HEX32_SIZE], CANARY); /* may use all 9 */
}

void test_fu_fmt_u32(void) {
  for (uint32_t v = 0U; v < 200000U; v++) {
    check_u32(v);
  }
  uint32_t p = 1U;
  for (int k = 0; k < 10; k++, p *= 10U) {
    check_u32(p - 1U);
    check_u32(p);
    check_u32(p + 1U);
  }
  check_u32(UINT32_MAX);
  check_u32(UINT32_MAX - 1U);

  uint64_t s = 5U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t v = test_rng_u32(&s);
    check_u32(v >> (v & 31U)); /* every digit count */
  }
}

void test_fu_fmt_i32(void) {
  for (int32_t v = -100000; v <= 100000; v++) {
    check_i32(v);
  }
  check_i32(INT32_MIN);
  check_i32(INT32_MIN + 1);
  check_i32(INT32_MAX);
  uint64_t s = 6U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&s);
    check_i32((int32_t)u >> (u & 31U));
  }
}

void test_fu_fmt_hex32(void) {
  for (uint32_t d = 0U; d <= 8U; d++) {
    check_hex(0U, d, false);
    check_hex(UINT32_MAX, d, true);
    for (uint32_t k = 0U; k < 32U; k++) {
      check_hex(1U << k, d, false);
      check_hex((1U << k) - 1U, d, true);
    }
  }
  uint64_t s = 7U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t v = test_rng_u32(&s);
    check_hex(v >> (v & 31U), v % 9U, (v & 64U) != 0U);
  }
}
