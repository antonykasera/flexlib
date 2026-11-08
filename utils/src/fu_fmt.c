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

/* Two digits per step: v / 100 is one UMULL and a shift. The fastest of five
 * designs measured on the M3 (116 cycles for 10 digits); the others are kept
 * as bench candidates. */
size_t fu_fmt_u32(char *buf, uint32_t v) {
  uint32_t n = fu_fmt_dec_digits(v);
  char *p = buf + n;
  *p = '\0';
  while (v >= 100U) {
    uint32_t q = v / 100U;
    uint32_t r = v - q * 100U;
    v = q;
    p -= 2;
    memcpy(p, &digit_pairs[2U * r], 2U);
  }
  if (v >= 10U) {
    memcpy(p - 2, &digit_pairs[2U * v], 2U);
  } else {
    p[-1] = (char)('0' + v);
  }
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
