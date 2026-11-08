/**
 * @file test_fu_parse.c
 * @brief Host tests for fu_parse.h: edge cases, round trips through fu_fmt,
 *        and exact references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "fu_parse.h"
#include "test_stats.h"

__extension__ typedef unsigned __int128 u128;

#define LEN(lit) (sizeof(lit) - 1U)

/* whole-string parse of a literal: status and value */
#define CHECK_U32(lit, st, val)                                                \
  do {                                                                         \
    uint32_t v_ = 0xDEADBEEFU;                                                 \
    ASSERT_EQ(fu_parse_u32(lit, LEN(lit), &v_, NULL), st);                     \
    ASSERT_EQ(v_, val);                                                        \
  } while (0)
#define CHECK_I32(lit, st, val)                                                \
  do {                                                                         \
    int32_t v_ = 0x5EADBEEF;                                                   \
    ASSERT_EQ(fu_parse_i32(lit, LEN(lit), &v_, NULL), st);                     \
    ASSERT_EQ(v_, val);                                                        \
  } while (0)
#define CHECK_HEX(lit, st, val)                                                \
  do {                                                                         \
    uint32_t v_ = 0xDEADBEEFU;                                                 \
    ASSERT_EQ(fu_parse_hex32(lit, LEN(lit), &v_, NULL), st);                   \
    ASSERT_EQ(v_, val);                                                        \
  } while (0)
#define CHECK_Q16(lit, st, val)                                                \
  do {                                                                         \
    int32_t v_ = 0x5EADBEEF;                                                   \
    ASSERT_EQ(fu_parse_q16(lit, LEN(lit), &v_, NULL), st);                     \
    ASSERT_EQ(v_, val);                                                        \
  } while (0)

void test_fu_parse_edges(void) {
  CHECK_U32("0", FU_PARSE_OK, 0U);
  CHECK_U32("4294967295", FU_PARSE_OK, UINT32_MAX);
  CHECK_U32("0000000000004294967295", FU_PARSE_OK, UINT32_MAX);
  CHECK_U32("4294967296", FU_PARSE_RANGE, UINT32_MAX);
  CHECK_U32("99999999999999999999", FU_PARSE_RANGE, UINT32_MAX);
  CHECK_U32("", FU_PARSE_SYNTAX, 0xDEADBEEFU);
  CHECK_U32("+5", FU_PARSE_SYNTAX, 0xDEADBEEFU);
  CHECK_U32("12a", FU_PARSE_SYNTAX, 0xDEADBEEFU);
  CHECK_U32(" 1", FU_PARSE_SYNTAX, 0xDEADBEEFU);

  CHECK_I32("-2147483648", FU_PARSE_OK, INT32_MIN);
  CHECK_I32("+2147483647", FU_PARSE_OK, INT32_MAX);
  CHECK_I32("2147483648", FU_PARSE_RANGE, INT32_MAX);
  CHECK_I32("-2147483649", FU_PARSE_RANGE, INT32_MIN);
  CHECK_I32("-99999999999", FU_PARSE_RANGE, INT32_MIN);
  CHECK_I32("-0", FU_PARSE_OK, 0);
  CHECK_I32("-", FU_PARSE_SYNTAX, 0x5EADBEEF);
  CHECK_I32("--1", FU_PARSE_SYNTAX, 0x5EADBEEF);

  CHECK_HEX("ffffFFFF", FU_PARSE_OK, UINT32_MAX);
  CHECK_HEX("0xDeadBeef", FU_PARSE_OK, 0xDEADBEEFU);
  CHECK_HEX("0X00000000000a", FU_PARSE_OK, 10U);
  CHECK_HEX("100000000", FU_PARSE_RANGE, UINT32_MAX);
  CHECK_HEX("0x", FU_PARSE_SYNTAX, 0xDEADBEEFU); /* "0", then "x" */
  CHECK_HEX("g", FU_PARSE_SYNTAX, 0xDEADBEEFU);

  CHECK_Q16("1.5", FU_PARSE_OK, 98304);
  CHECK_Q16("-1.5", FU_PARSE_OK, -98304);
  CHECK_Q16(".5", FU_PARSE_OK, 32768);
  CHECK_Q16("2.", FU_PARSE_OK, 131072);
  CHECK_Q16("-32768", FU_PARSE_OK, INT32_MIN);
  CHECK_Q16("32767.99999", FU_PARSE_OK, INT32_MAX);
  CHECK_Q16("32767.999995", FU_PARSE_RANGE, INT32_MAX); /* rounds to 32768 */
  CHECK_Q16("-32768.00001", FU_PARSE_RANGE, INT32_MIN);
  CHECK_Q16("99999999999.5", FU_PARSE_RANGE, INT32_MAX);
  /* exact ties at (2k + 1) / 2^17 go to even; anything past one goes up */
  CHECK_Q16("0.00000762939453125", FU_PARSE_OK, 0);
  CHECK_Q16("0.00002288818359375", FU_PARSE_OK, 2);
  CHECK_Q16("0.000007629394531250000000001", FU_PARSE_OK, 1);
  CHECK_Q16("-0.00002288818359375", FU_PARSE_OK, -2);
  CHECK_Q16(".", FU_PARSE_SYNTAX, 0x5EADBEEF);
  CHECK_Q16("-.", FU_PARSE_SYNTAX, 0x5EADBEEF);
  CHECK_Q16("1.2.3", FU_PARSE_SYNTAX, 0x5EADBEEF);

  /* prefixes: used reports where the number stopped */
  uint32_t u = 0U;
  size_t used = 99U;
  ASSERT_EQ(fu_parse_u32("123,45", 6U, &u, &used), FU_PARSE_OK);
  ASSERT_EQ(u, 123U);
  ASSERT_EQ(used, 3U);
  ASSERT_EQ(fu_parse_u32("123", 2U, &u, &used), FU_PARSE_OK); /* bounded */
  ASSERT_EQ(u, 12U);
  ASSERT_EQ(fu_parse_hex32("0xz", 3U, &u, &used), FU_PARSE_OK);
  ASSERT_EQ(u, 0U);
  ASSERT_EQ(used, 1U);
  used = 99U;
  ASSERT_EQ(fu_parse_u32("x1", 2U, &u, &used), FU_PARSE_SYNTAX);
  ASSERT_EQ(used, 99U);
  int32_t q = 0;
  ASSERT_EQ(fu_parse_q16("-0.25V", 6U, &q, &used), FU_PARSE_OK);
  ASSERT_EQ(q, -16384);
  ASSERT_EQ(used, 5U);
  ASSERT_EQ(fu_parse_u32(NULL, 0U, &u, &used), FU_PARSE_SYNTAX);
}

/* exact reference: the decimal times 2^16, rounded to nearest, ties to even */
static FU_ParseStatus ref_q16(const char *s, size_t len, int32_t *out) {
  size_t i = 0U;
  bool neg = false;
  if (i < len && (s[i] == '-' || s[i] == '+')) {
    neg = s[i++] == '-';
  }
  u128 ip = 0U;
  u128 f = 0U;
  u128 p10 = 1U;
  size_t nd = 0U;
  for (; i < len && s[i] >= '0' && s[i] <= '9'; i++, nd++) {
    ip = ip * 10U + (u128)((uint32_t)(uint8_t)s[i] - 48U);
  }
  if (i < len && s[i] == '.') {
    for (i++; i < len && s[i] >= '0' && s[i] <= '9'; i++, nd++) {
      f = f * 10U + (u128)((uint32_t)(uint8_t)s[i] - 48U);
      p10 *= 10U;
    }
  }
  if (nd == 0U || i != len) {
    return FU_PARSE_SYNTAX;
  }
  u128 num = (ip * p10 + f) << 16;
  u128 q = num / p10;
  u128 r = num % p10;
  if (2U * r > p10 || (2U * r == p10 && (q & 1U) != 0U)) {
    q++;
  }
  if (q > (neg ? 0x80000000U : 0x7FFFFFFFU)) {
    *out = neg ? INT32_MIN : INT32_MAX;
    return FU_PARSE_RANGE;
  }
  *out = neg ? (int32_t)(0U - (uint32_t)q) : (int32_t)q;
  return FU_PARSE_OK;
}

void test_fu_parse_q16_vs_exact(void) {
  uint64_t st = 51U;
  char s[48];
  for (int it = 0; it < 1000000; it++) {
    uint32_t u = test_rng_u32(&st);
    size_t n = 0U;
    if ((u & 3U) == 0U) {
      s[n++] = (u & 4U) != 0U ? '-' : '+';
    }
    size_t ni = (u >> 3) % 7U; /* 0..6 integer digits */
    size_t nf = (u >> 6) % 24U; /* 0..23 fraction digits */
    for (size_t k = 0U; k < ni; k++) {
      s[n++] = (char)('0' + test_rng_u32(&st) % 10U);
    }
    if ((u & 0x8000U) != 0U || nf != 0U) {
      s[n++] = '.';
    }
    for (size_t k = 0U; k < nf; k++) {
      uint32_t r = test_rng_u32(&st);
      /* runs of 0s and 9s reach the rounding edge cases */
      s[n++] = (char)('0' + ((r & 3U) == 0U ? 0U : (r & 3U) == 1U ? 9U
                                                                  : r % 10U));
    }
    int32_t want = 0x5EADBEEF;
    int32_t got = 0x5EADBEEF;
    FU_ParseStatus a = ref_q16(s, n, &want);
    FU_ParseStatus b = fu_parse_q16(s, n, &got, NULL);
    ASSERT_EQ(b, a);
    ASSERT_EQ(got, want);
  }
}

/* through fu_fmt and back, with trailing text reported by used */
void test_fu_parse_round_trips(void) {
  uint64_t st = 61U;
  char s[32];
  for (int it = 0; it < 1000000; it++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t x = u >> (u & 31U);
    size_t used = 0U;

    size_t n = fu_fmt_u32(s, x);
    s[n] = ';';
    uint32_t v = 0U;
    ASSERT_EQ(fu_parse_u32(s, n + 1U, &v, &used), FU_PARSE_OK);
    ASSERT_EQ(v, x);
    ASSERT_EQ(used, n);

    n = fu_fmt_i32(s, (int32_t)x);
    int32_t iv = 0;
    ASSERT_EQ(fu_parse_i32(s, n, &iv, NULL), FU_PARSE_OK);
    ASSERT_EQ(iv, (int32_t)x);

    s[0] = '0';
    s[1] = (u & 64U) != 0U ? 'x' : 'X';
    n = fu_fmt_hex32(s + 2, x, u % 9U, (u & 128U) != 0U) + 2U;
    ASSERT_EQ(fu_parse_hex32(s, n, &v, NULL), FU_PARSE_OK);
    ASSERT_EQ(v, x);

    /* 5 or more digits are unique for Q16.16, so the value comes back */
    n = fu_fmt_q16(s, (int32_t)u, 5U + u % 5U);
    ASSERT_EQ(fu_parse_q16(s, n, &iv, NULL), FU_PARSE_OK);
    ASSERT_EQ(iv, (int32_t)u);
  }
}

/* Exact midpoints (2k + 1) / 2^17 have 17 fraction digits: (2k + 1) * 5^17.
 * Each one, nudged just below, just above, and with a far nonzero digit. */
void test_fu_parse_q16_ties(void) {
  uint64_t st = 71U;
  char s[64];
  for (int it = 0; it < 200000; it++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t ip = u % 32768U;
    uint64_t k = (u >> 15) & 0xFFFFU; /* the q below the tie */
    uint64_t mid = (2U * k + 1U) * 762939453125ULL; /* 17 digits */
    size_t n = fu_fmt_u32(s, ip);
    s[n++] = '.';
    char digits[18];
    for (int d = 16; d >= 0; d--) {
      digits[d] = (char)('0' + mid % 10U);
      mid /= 10U;
    }
    memcpy(s + n, digits, 17U);
    n += 17U;
    uint32_t base = (ip << 16) + (uint32_t)k;
    int32_t got = 0;
    /* the tie itself: to even */
    ASSERT_EQ(fu_parse_q16(s, n, &got, NULL), FU_PARSE_OK);
    ASSERT_EQ((uint32_t)got, base + (uint32_t)(k & 1U));
    /* a far nonzero digit: up */
    memcpy(s + n, "000000001", 9U);
    ASSERT_EQ(fu_parse_q16(s, n + 9U, &got, NULL), FU_PARSE_OK);
    ASSERT_EQ((uint32_t)got, base + 1U);
    /* the last digit one lower (mid ends in 5): down */
    s[n - 1U] = '4';
    ASSERT_EQ(fu_parse_q16(s, n, &got, NULL), FU_PARSE_OK);
    ASSERT_EQ((uint32_t)got, base);
    /* one higher: up */
    s[n - 1U] = '6';
    ASSERT_EQ(fu_parse_q16(s, n, &got, NULL), FU_PARSE_OK);
    ASSERT_EQ((uint32_t)got, base + 1U);
  }
}
