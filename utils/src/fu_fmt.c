/**
 * @file fu_fmt.c
 * @brief Integers to text without printf: decimal and hex
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "fu_bits.h"
#include <string.h>

/* "00" .. "99"; 201 so the literal's NUL fits */
static const char digit_pairs[201] FX_ALIGN_LINE =
    "00010203040506070809101112131415161718192021222324252627282930313233343536"
    "37383940414243444546474849505152535455565758596061626364656667686970717273"
    "7475767778798081828384858687888990919293949596979899";

static const uint32_t pow10_tab[10] = {
    1U,      10U,      100U,      1000U,      10000U,
    100000U, 1000000U, 10000000U, 100000000U, 1000000000U};

uint32_t fu_fmt_dec_digits(uint32_t v) {
  v |= 1U;
  /* floor(log10) from the bit length (1233 / 4096 ~ log10 2), off by at most
   * one, then corrected */
  uint32_t t = ((32U - fu_clz(v)) * 1233U) >> 12;
  return t + 1U - (v < pow10_tab[t] ? 1U : 0U);
}

/* the next two digits of the 0.32 fraction f */
FX_INLINE char *put_pair(char *p, uint32_t *f) {
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
 * of the fraction by 100 yields the next. `make exhaustive` checks all 2^32. */
size_t fu_fmt_u32(char *buf, uint32_t v) {
  uint32_t n = fu_fmt_dec_digits(v);
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
  char *p = put_pair(t + 4, &f);
  p = put_pair(p, &f);
  p = put_pair(p, &f);
  p[0] = '\0';
  p[1] = '\0';

  const char *s = t + 10U - n;
  memcpy(buf, s, 8U);
  memcpy(buf + 8, s + 8, 3U);
  return n;
}

size_t fu_fmt_i32(char *buf, int32_t v) {
  if (v < 0) {
    buf[0] = '-';
    return 1U + fu_fmt_u32(buf + 1, 0U - (uint32_t)v);
  }
  return fu_fmt_u32(buf, (uint32_t)v);
}

/* Four nibbles to four ASCII digits at once (SWAR). The top nibble ends up in
 * the low byte, which a little-endian store puts first. alpha = 'A' - '9' - 1
 * or 'a' - '9' - 1. */
FX_INLINE uint32_t hex4(uint32_t x, uint32_t alpha) {
  x = ((x & 0xFF00U) << 8) | (x & 0x00FFU);
  x = ((x & 0x00F000F0U) << 4) | (x & 0x000F000FU); /* one nibble per byte */
  uint32_t ge10 = ((x + 0x06060606U) >> 4) & 0x01010101U;
  return fx_rev(x + 0x30303030U + ge10 * alpha);
}

size_t fu_fmt_hex32(char *buf, uint32_t v, uint32_t min_digits, bool upper) {
  FX_ASSERT(min_digits <= 8U);
  uint32_t n = (35U - fu_clz(v | 1U)) / 4U;
  if (n < min_digits) {
    n = min_digits;
  }
  uint32_t alpha = upper ? 7U : 39U;
  if (n <= 4U) { /* v < 2^16 */
    fu_store_le32(buf, hex4(v, alpha) >> (8U * (4U - n)));
    buf[n] = '\0';
    return n;
  }
  uint64_t s = (uint64_t)hex4(v >> 16, alpha) |
               (uint64_t)hex4(v & 0xFFFFU, alpha) << 32;
  s >>= 8U * (8U - n); /* drop the leading zeros; the freed bytes are NULs */
  fu_store_le32(buf, (uint32_t)s);
  fu_store_le32(buf + 4, (uint32_t)(s >> 32));
  buf[n] = '\0';
  return n;
}
