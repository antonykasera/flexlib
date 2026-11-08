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

/* checks text, length, NUL, and that nothing past the documented buffer size
 * (FU_FMT_*_SIZE) was written */
static void check_u32(uint32_t v) {
  char want[16];
  char got[16];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, "%lu", (unsigned long)v);
  size_t len = fu_fmt_u32(got, v);
  ASSERT_EQ(len, n);
  ASSERT_EQ(fu_fmt_dec_digits(v), n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[FU_FMT_U32_SIZE], CANARY);
}

static void check_i32(int32_t v) {
  char want[16];
  char got[16];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, "%ld", (long)v);
  size_t len = fu_fmt_i32(got, v);
  ASSERT_EQ(len, n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[FU_FMT_I32_SIZE], CANARY);
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
  ASSERT_EQ((uint8_t)got[FU_FMT_HEX32_SIZE], CANARY);
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

/* ---- Q16.16 and float, against printf's "%.*f" (exact, ties to even) ---- */

static void check_q16(int32_t v, uint32_t d) {
  char want[64];
  char got[64];
  memset(got, CANARY, sizeof got);
  int n = snprintf(want, sizeof want, "%.*f", (int)d, (double)v / 65536.0);
  size_t len = fu_fmt_q16(got, v, d);
  ASSERT_EQ(len, n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[FU_FMT_Q16_SIZE], CANARY);
}

static void check_f32(float x, uint32_t d) {
  char want[64];
  char got[64];
  memset(got, CANARY, sizeof got);
  int n = x != x ? snprintf(want, sizeof want, "nan")
                 : snprintf(want, sizeof want, "%.*f", (int)d, (double)x);
  size_t len = fu_fmt_f32(got, x, d);
  ASSERT_EQ(len, n);
  ASSERT_TRUE(strcmp(got, want) == 0);
  ASSERT_EQ((uint8_t)got[FU_FMT_F32_SIZE], CANARY);
}

static float f32_bits(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

void test_fu_fmt_q16(void) {
  static const int32_t whole[] = {0, 1, -1, 2, -3, 32767, -32768, -32767};
  for (uint32_t d = 0U; d <= FU_FMT_FRAC_MAX; d++) {
    for (size_t w = 0U; w < sizeof whole / sizeof whole[0]; w++) {
      for (uint32_t f = 0U; f < 65536U; f++) {
        check_q16((int32_t)((uint32_t)whole[w] << 16 | f), d);
      }
    }
  }
  check_q16(INT32_MAX, 9U);
  check_q16(INT32_MIN, 9U);
  uint64_t s = 8U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&s);
    check_q16((int32_t)u >> (u & 15U), u % 10U);
  }
}

void test_fu_fmt_f32(void) {
  static const uint32_t edges[] = {
      0x00000000U, 0x80000000U, /* +-0 */
      0x00000001U, 0x807FFFFFU, /* subnormals */
      0x00800000U, 0x7F7FFFFFU, /* smallest normal, FLT_MAX */
      0x4F7FFFFFU, 0x4F800000U, /* the largest below 2^32, and 2^32 */
      0x3F000000U, 0x3FC00000U, 0x40200000U, /* 0.5 1.5 2.5: ties */
      0x3E000000U, 0x3EC00000U,              /* 0.125 0.375 */
      0x3F7FFFFFU, 0x447A0000U,              /* 1 - 2^-24, 1000 */
      0x7F800000U, 0xFF800000U, 0x7FC00000U, 0xFFC00001U, /* inf, nan */
  };
  for (uint32_t d = 0U; d <= FU_FMT_FRAC_MAX; d++) {
    for (size_t i = 0U; i < sizeof edges / sizeof edges[0]; i++) {
      check_f32(f32_bits(edges[i]), d);
    }
    for (uint32_t e = 0U; e < 255U; e++) { /* every power of two */
      check_f32(f32_bits(e << 23), d);
      check_f32(-f32_bits(e << 23 | 0x7FFFFFU), d);
    }
  }
  uint64_t s = 9U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&s);
    check_f32(f32_bits(u), u % 10U); /* every exponent */
    /* and the common range, |x| < 2^20 */
    check_f32(f32_bits((u & 0x807FFFFFU) | (100U + u % 47U) << 23),
              (u >> 7) % 10U);
  }
}

/* ---- fu_snprintf ---- */

#define CHECK_PRINTF(fmt, ...)                                                 \
  do {                                                                         \
    char want_[96];                                                            \
    char got_[96];                                                             \
    int n_ = snprintf(want_, sizeof want_, fmt, __VA_ARGS__);                  \
    size_t len_ = fu_snprintf(got_, sizeof got_, fmt, __VA_ARGS__);            \
    ASSERT_EQ(len_, n_);                                                       \
    ASSERT_TRUE(strcmp(got_, want_) == 0);                                     \
  } while (0)

/* every buffer size from 0 to len + 2: prefix, NUL, no write past size */
#define CHECK_SIZES(want, ...)                                                 \
  do {                                                                         \
    size_t wl_ = strlen(want);                                                 \
    for (size_t sz_ = 0U; sz_ <= wl_ + 2U; sz_++) {                            \
      char g_[128];                                                            \
      memset(g_, CANARY, sizeof g_);                                           \
      ASSERT_EQ(fu_snprintf(sz_ != 0U ? g_ : NULL, sz_, __VA_ARGS__), wl_);    \
      if (sz_ != 0U) {                                                         \
        size_t k_ = sz_ - 1U < wl_ ? sz_ - 1U : wl_;                           \
        ASSERT_TRUE(memcmp(g_, want, k_) == 0 && g_[k_] == '\0');              \
      }                                                                        \
      ASSERT_EQ((uint8_t)g_[sz_], CANARY);                                     \
    }                                                                          \
  } while (0)

static void test_snprintf_sizes(void) {
  char w[128];
  snprintf(w, sizeof w, "ab%dcd%-7uef%08Xg%5xh%%%s|%c", -12345, 678U,
           0xABCU, 0x1FU, "str", 'z');
  CHECK_SIZES(w, "ab%dcd%-7uef%08Xg%5xh%%%s|%c", -12345, 678U, 0xABCU, 0x1FU,
              "str", 'z');
  snprintf(w, sizeof w, "[%24d][%-20u][%020d][%019X][%5d]", INT32_MIN,
           UINT32_MAX, -77, 0xBEEFU, 3);
  CHECK_SIZES(w, "[%24d][%-20u][%020d][%019X][%5d]", INT32_MIN, UINT32_MAX,
              -77, 0xBEEFU, 3);
  snprintf(w, sizeof w, "%.9f|%22.3f|%-12.1f|%012.2f", -32768.0,
           -98304 / 65536.0, 1.5, -0.25);
  CHECK_SIZES(w, "%.9q|%22.3q|%-12.1q|%012.2q", INT32_MIN, -98304, 98304,
              -16384);
}

void test_fu_snprintf(void) {
  CHECK_PRINTF("plain %s", "text");
  CHECK_PRINTF("%d|%i|%u|%x|%X|%c|%%", -42, 7, 42U, 0xBEEFU, 0xBEEFU, 'z');
  CHECK_PRINTF("[%5d][%-5d][%05d]", -42, -42, -42);
  CHECK_PRINTF("[%08x][%8X][%-8x][%01x]", 0xABCU, 0xABCU, 0xABCU, 0U);
  CHECK_PRINTF("[%d][%d][%u]", INT32_MIN, INT32_MAX, UINT32_MAX);
  CHECK_PRINTF("[%ld][%lu][%lx]", -5L, 5UL, 0xFFUL);
  CHECK_PRINTF("[%*d][%-*d][%*d]", 6, 12, 6, 12, -6, 12);
  CHECK_PRINTF("[%10s][%-10s][%.3s][%.*s][%5c]", "ab", "ab", "abcdef", 2,
               "abcdef", 'q');
  CHECK_PRINTF("%s", "");
  CHECK_PRINTF("%c%c%c", 'a', 0x7F, ' ');
  CHECK_PRINTF("%%%d%%", 100);

  /* %q against %f of the same value */
  char got[64];
  char want[64];
  size_t n = fu_snprintf(got, sizeof got, "[%q][%.2q][%09.3q][%-9.1q][%.0q]",
                         -98304, 98304, -98304, 65536 / 3, 32768);
  int m = snprintf(want, sizeof want, "[%.4f][%.2f][%09.3f][%-9.1f][%.0f]",
                   -1.5, 1.5, -1.5, (65536 / 3) / 65536.0, 0.5);
  ASSERT_EQ(n, m);
  ASSERT_TRUE(strcmp(got, want) == 0);

  /* truncation: at most size - 1 characters, always a NUL, full length back */
  memset(got, CANARY, sizeof got);
  ASSERT_EQ(fu_snprintf(got, 6U, "value=%d", 12345), 11U);
  ASSERT_TRUE(strcmp(got, "value") == 0);
  ASSERT_EQ((uint8_t)got[6], CANARY);
  ASSERT_EQ(fu_snprintf(got, 1U, "%08x", 1U), 8U);
  ASSERT_EQ(got[0], '\0');
  ASSERT_EQ(fu_snprintf(NULL, 0U, "%s%10d", "abc", 1), 13U);
  memset(got, CANARY, sizeof got);
  ASSERT_EQ(fu_snprintf(got, 4U, "%-6c|", 'x'), 7U);
  ASSERT_TRUE(strcmp(got, "x  ") == 0);

  test_snprintf_sizes();
}
