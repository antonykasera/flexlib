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

/* Y = ((v * M) >> S) + 1 is v / 10^k in 32.32 fixed point, for k = 2, 4, 6, 8.
 * The +1 keeps Y from rounding below the true value, and M, S keep it less
 * than 2^32 / 10^k above it for every v with up to k + 2 digits. So each
 * multiply of the fraction by 100 yields the next two digits exactly.
 * `make exhaustive` checks all 2^32 inputs. */
static const uint32_t frac_mul[4] = {2748779070U, 3518437209U, 2251799814U,
                                     2882303762U};
static const uint8_t frac_shift[4] = {6U, 13U, 19U, 26U};

/* the next two digits of the 0.32 fraction f */
FX_INLINE char *put_pair(char *p, uint32_t *f) {
  uint64_t t = (uint64_t)*f * 100U;
  memcpy(p, &digit_pairs[2U * (uint32_t)(t >> 32)], 2U);
  *f = (uint32_t)t;
  return p + 2;
}

size_t fu_fmt_u32(char *buf, uint32_t v) {
  uint32_t n = fu_fmt_dec_digits(v);
  if (n <= 2U) {
    if (n == 2U) {
      memcpy(buf, &digit_pairs[2U * v], 2U);
    } else {
      buf[0] = (char)('0' + v);
    }
    buf[n] = '\0';
    return n;
  }

  uint32_t i = (n - 1U) / 2U - 1U; /* k = 2 (i + 1) */
  uint64_t y = (((uint64_t)v * frac_mul[i]) >> frac_shift[i]) + 1U;
  uint32_t lead = (uint32_t)(y >> 32); /* the first 1 (n odd) or 2 digits */
  uint32_t f = (uint32_t)y;
  char *p = buf;
  if ((n & 1U) != 0U) {
    *p++ = (char)('0' + lead);
  } else {
    memcpy(p, &digit_pairs[2U * lead], 2U);
    p += 2;
  }
  switch (i) { /* i + 1 more pairs, unrolled */
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
  uint64_t s = (uint64_t)hex4(v >> 16, alpha) |
               (uint64_t)hex4(v & 0xFFFFU, alpha) << 32;
  s >>= 8U * (8U - n); /* drop the leading zeros; the freed bytes are NULs */
  fu_store_le32(buf, (uint32_t)s);
  fu_store_le32(buf + 4, (uint32_t)(s >> 32));
  buf[n] = '\0';
  return n;
}
