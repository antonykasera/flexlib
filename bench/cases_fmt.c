/**
 * @file cases_fmt.c
 * @brief Bench cases for fu_fmt.h, with the decimal candidates it was chosen
 *        from and newlib's snprintf as the baseline
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_bits.h"
#include "fu_fmt.h"
#include <stdio.h>

static char text[16];

/* ---- decimal candidates (fu_fmt_u32: two digits per step, a loop) ---- */

/* a copy of fu_fmt_dec_digits, inlined here as it is in fu_fmt_u32, so the
 * candidates differ from it only in the digit loop */
static const uint32_t pow10_tab[10] = {
    1U,      10U,      100U,      1000U,      10000U,
    100000U, 1000000U, 10000000U, 100000000U, 1000000000U};

static inline uint32_t dec_digits(uint32_t v) {
  v |= 1U;
  uint32_t t = ((32U - fu_clz(v)) * 1233U) >> 12;
  return t + 1U - (v < pow10_tab[t] ? 1U : 0U);
}

static const char digit_pairs[201] FX_ALIGN_LINE =
    "00010203040506070809101112131415161718192021222324252627282930313233343536"
    "37383940414243444546474849505152535455565758596061626364656667686970717273"
    "7475767778798081828384858687888990919293949596979899";

/* fixed point per digit count: one UMULL to v / 10^k, then a UMULL by 100 per
 * pair, unrolled with a switch (the version before the split) */
static const uint32_t frac_mul[4] = {2748779070U, 3518437209U, 2251799814U,
                                     2882303762U};
static const uint8_t frac_shift[4] = {6U, 13U, 19U, 26U};

static inline char *put_pair(char *p, uint32_t *f) {
  uint64_t t = (uint64_t)*f * 100U;
  memcpy(p, &digit_pairs[2U * (uint32_t)(t >> 32)], 2U);
  *f = (uint32_t)t;
  return p + 2;
}

BENCH_CALL static size_t u32_unrolled(char *buf, uint32_t v) {
  uint32_t n = dec_digits(v);
  if (n <= 2U) {
    if (n == 2U) {
      memcpy(buf, &digit_pairs[2U * v], 2U);
    } else {
      buf[0] = (char)('0' + v);
    }
    buf[n] = '\0';
    return n;
  }
  uint32_t i = (n - 1U) / 2U - 1U;
  uint64_t y = (((uint64_t)v * frac_mul[i]) >> frac_shift[i]) + 1U;
  uint32_t lead = (uint32_t)(y >> 32);
  uint32_t f = (uint32_t)y;
  char *p = buf;
  if ((n & 1U) != 0U) {
    *p++ = (char)('0' + lead);
  } else {
    memcpy(p, &digit_pairs[2U * lead], 2U);
    p += 2;
  }
  switch (i) {
  case 3U:
    p = put_pair(p, &f);
    __attribute__((fallthrough));
  case 2U:
    p = put_pair(p, &f);
    __attribute__((fallthrough));
  case 1U:
    p = put_pair(p, &f);
    __attribute__((fallthrough));
  default:
    p = put_pair(p, &f);
  }
  *p = '\0';
  return n;
}

/* the next two digits of the 0.32 fraction f */
static inline char *put_pair2(char *p, uint32_t *f) {
  uint64_t t = (uint64_t)*f * 100U;
  memcpy(p, &digit_pairs[2U * (uint32_t)(t >> 32)], 2U);
  *f = (uint32_t)t;
  return p + 2;
}

/* All ten digits without a branch, then the last n of them. v splits at 10^8
 * into hi (0..42, two digits) and lo (eight digits, leading zeros kept).
 * ((lo * M) >> 19) + 1 is lo / 10^6 in 32.32 fixed point: the +1 keeps it from
 * rounding below the true value, and M keeps it less than 2^32 / 10^6 above it
 * for every lo < 10^8. So the integer part is the first pair, and each multiply
 * of the fraction by 100 yields the next. Exhaustively correct (2^32 inputs) while it was the library version. */
BENCH_CALL static size_t u32_split(char *buf, uint32_t v) {
  uint32_t n = dec_digits(v);
  uint32_t hi = v / 100000000U;
  uint32_t lo = v - hi * 100000000U;

  /* digits in t[0..9], NUL at t[10]; zeros up to t[19] so the fixed 11-byte
   * copy below never reads uninitialised bytes */
  uint32_t tmp[5];
  char *t = (char *)tmp;
  tmp[3] = 0U;
  tmp[4] = 0U;
  memcpy(t, &digit_pairs[2U * hi], 2U);
  uint64_t y = (((uint64_t)lo * 2251799814U) >> 19) + 1U;
  memcpy(t + 2, &digit_pairs[2U * (uint32_t)(y >> 32)], 2U);
  uint32_t f = (uint32_t)y;
  char *p = put_pair2(t + 4, &f);
  p = put_pair2(p, &f);
  p = put_pair2(p, &f);
  p[0] = '\0';
  p[1] = '\0';

  const char *s = t + 10U - n;
  memcpy(buf, s, 8U);
  memcpy(buf + 8, s + 8, 3U);
  return n;
}

/* hex one nibble per step, with a digit table */
BENCH_CALL static size_t hex_loop(char *buf, uint32_t v, uint32_t min_digits,
                                  bool upper) {
  const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  uint32_t n = (35U - fu_clz(v | 1U)) / 4U;
  if (n < min_digits) {
    n = min_digits;
  }
  buf[n] = '\0';
  for (uint32_t i = n; i > 0U; i--) {
    buf[i - 1U] = dig[v & 0xFU];
    v >>= 4;
  }
  return n;
}

/* one digit per step; v / 10 is one UMULL and a shift */
BENCH_CALL static size_t u32_by10(char *buf, uint32_t v) {
  uint32_t n = dec_digits(v);
  char *p = buf + n;
  *p = '\0';
  do {
    uint32_t q = v / 10U;
    *--p = (char)('0' + (v - q * 10U));
    v = q;
  } while (v != 0U);
  return n;
}

/* the same with the hardware divider: a divisor the compiler can't see */
static volatile uint32_t ten = 10U;

BENCH_CALL static size_t u32_udiv(char *buf, uint32_t v) {
  uint32_t d = ten;
  uint32_t n = dec_digits(v);
  char *p = buf + n;
  *p = '\0';
  do {
    uint32_t q = v / d;
    *--p = (char)('0' + (v - q * d));
    v = q;
  } while (v != 0U);
  return n;
}

/* ---- inputs ---- */

/* 1e9 .. 2^32 - 1: always 10 digits */
static inline uint32_t in_10dig(uint32_t u) {
  return 1000000000U + u % 3294967296U;
}
/* every digit count, about equally often per bit length */
static inline uint32_t in_logu(uint32_t u) { return u >> (u & 31U); }
static inline int32_t in_logs(uint32_t u) { return (int32_t)u >> (u & 31U); }

BENCH_TEXT(b_u32_10, uint32_t, in_10dig, fu_fmt_u32(text, x), text)
BENCH_TEXT(b_u32_log, uint32_t, in_logu, fu_fmt_u32(text, x), text)
BENCH_TEXT(b_split_10, uint32_t, in_10dig, u32_split(text, x), text)
BENCH_TEXT(b_split_log, uint32_t, in_logu, u32_split(text, x), text)
BENCH_TEXT(b_unrolled_10, uint32_t, in_10dig, u32_unrolled(text, x), text)
BENCH_TEXT(b_unrolled_log, uint32_t, in_logu, u32_unrolled(text, x), text)
BENCH_TEXT(b_by10_10, uint32_t, in_10dig, u32_by10(text, x), text)
BENCH_TEXT(b_by10_log, uint32_t, in_logu, u32_by10(text, x), text)
BENCH_TEXT(b_udiv_10, uint32_t, in_10dig, u32_udiv(text, x), text)
BENCH_TEXT(b_udiv_log, uint32_t, in_logu, u32_udiv(text, x), text)
BENCH_TEXT(b_snprintf_10, uint32_t, in_10dig,
           snprintf(text, sizeof text, "%lu", (unsigned long)x), text)
BENCH_TEXT(b_snprintf_log, uint32_t, in_logu,
           snprintf(text, sizeof text, "%lu", (unsigned long)x), text)
BENCH_TEXT(b_i32_log, int32_t, in_logs, fu_fmt_i32(text, x), text)
BENCH_TEXT(b_hex_8, uint32_t, in_logu, fu_fmt_hex32(text, x, 8U, false), text)
BENCH_TEXT(b_hex_min, uint32_t, in_logu, fu_fmt_hex32(text, x, 0U, false),
           text)
BENCH_TEXT(b_hexloop_8, uint32_t, in_logu, hex_loop(text, x, 8U, false), text)
BENCH_TEXT(b_hexloop_min, uint32_t, in_logu, hex_loop(text, x, 0U, false),
           text)
BENCH_UNARY(b_digits, uint32_t, in_logu, uint32_t, fu_fmt_dec_digits(x))

/* "log" inputs have every digit count; snprintf is golden too, since newlib
 * and the host's libc must print the same text */
static const BenchCase cases[] = {
    {"fu_fmt_u32, 10 digits", "flexlib", b_u32_10, true},
    {"fu_fmt_u32, log", "flexlib", b_u32_log, true},
    {"u32 split at 1e8, no branch, 10 digits", "candidate", b_split_10, true},
    {"u32 split at 1e8, no branch, log", "candidate", b_split_log, true},
    {"u32 fixed point unrolled, 10 digits", "candidate", b_unrolled_10, true},
    {"u32 fixed point unrolled, log", "candidate", b_unrolled_log, true},
    {"u32 by 10s, 10 digits", "candidate", b_by10_10, true},
    {"u32 by 10s, log", "candidate", b_by10_log, true},
    {"u32 by 10s UDIV, 10 digits", "candidate", b_udiv_10, true},
    {"u32 by 10s UDIV, log", "candidate", b_udiv_log, true},
    {"snprintf %lu, 10 digits", "newlib", b_snprintf_10, true},
    {"snprintf %lu, log", "newlib", b_snprintf_log, true},
    {"fu_fmt_i32, log", "flexlib", b_i32_log, true},
    {"fu_fmt_hex32, 8 digits", "flexlib", b_hex_8, true},
    {"fu_fmt_hex32, minimal", "flexlib", b_hex_min, true},
    {"hex nibble loop, 8 digits", "candidate", b_hexloop_8, true},
    {"hex nibble loop, minimal", "candidate", b_hexloop_min, true},
    {"fu_fmt_dec_digits", "flexlib", b_digits, true},
};

const BenchGroup bench_group_fmt = {cases, BENCH_COUNT(cases)};
