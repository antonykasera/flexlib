/**
 * @file fu_fmt.c
 * @brief Numbers to text without printf: integers, Q16.16 and float
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
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
  /* bits * log10(2) (1233/4096), at most one low */
  uint32_t t = ((32U - fu_clz(v)) * 1233U) >> 12;
  return t + 1U - (v < pow10_tab[t] ? 1U : 0U);
}

/* two digits per step; v / 100 is a UMULL */
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

/* four nibbles to ASCII (SWAR); alpha = 'A' or 'a' minus '9' + 1 */
FX_INLINE uint32_t hex4(uint32_t x, uint32_t alpha) {
  x = ((x & 0xFF00U) << 8) | (x & 0x00FFU);
  x = ((x & 0x00F000F0U) << 4) | (x & 0x000F000FU);  uint32_t ge10 = ((x + 0x06060606U) >> 4) & 0x01010101U;
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
  s >>= 8U * (8U - n); /* drop leading zeros */
  fu_store_le32(buf, (uint32_t)s);
  fu_store_le32(buf + 4, (uint32_t)(s >> 32));
  buf[n] = '\0';
  return n;
}

/* d digits of q, zero-padded */
static void put_fixed(char *p, uint32_t q, uint32_t d) {
  for (; d >= 2U; d -= 2U) {
    uint32_t t = q / 100U;
    memcpy(p + d - 2U, &digit_pairs[2U * (q - t * 100U)], 2U);
    q = t;
  }
  if (d != 0U) {
    p[0] = (char)('0' + q);
  }
}

/* [-]ip + f / 2^s (s 1..63, f < 2^24) to d digits, ties to even */
FX_INLINE size_t fmt_point(char *buf, bool neg, uint32_t ip, uint32_t f,
                           uint32_t s, uint32_t d) {
  uint64_t p = (uint64_t)f * pow10_tab[d];
  uint32_t odd = (d == 0U ? ip : (uint32_t)(p >> s)) & 1U;
  uint32_t q = (uint32_t)((p + (1ULL << (s - 1U)) - 1U + odd) >> s);
  if (q == pow10_tab[d]) {
    ip++;
    q = 0U;
  }
  char *o = buf;
  *o = '-';
  o += neg ? 1 : 0;
  o += fu_fmt_u32(o, ip);
  if (d != 0U) {
    *o++ = '.';
    put_fixed(o, q, d);
    o += d;
  }
  *o = '\0';
  return (size_t)(o - buf);
}

size_t fu_fmt_q16(char *buf, int32_t v, uint32_t frac_digits) {
  FX_ASSERT(frac_digits <= FU_FMT_FRAC_MAX);
  uint32_t m = v < 0 ? 0U - (uint32_t)v : (uint32_t)v;
  return fmt_point(buf, v < 0, m >> 16, m & 0xFFFFU, 16U, frac_digits);
}

FX_COLD static size_t fmt_special(char *buf, bool neg, uint32_t mant) {
  const char *s = mant != 0U ? "nan" : (neg ? "-inf" : "inf");
  size_t n = strlen(s);
  memcpy(buf, s, n + 1U);
  return n;
}

/* x >= 2^32: m * 2^e in base-10^4 limbs */
FX_COLD static size_t fmt_big(char *buf, bool neg, uint32_t m, uint32_t e,
                              uint32_t d) {
  uint32_t limb[10]; /* least significant first */
  uint32_t n = 0U;
  do {
    limb[n++] = m % 10000U;
    m /= 10000U;
  } while (m != 0U);
  while (e != 0U) {
    uint32_t k = e < 18U ? e : 18U;
    e -= k;
    uint32_t carry = 0U;
    for (uint32_t i = 0U; i < n; i++) {
      uint32_t t = (limb[i] << k) + carry;
      carry = t / 10000U;
      limb[i] = t - carry * 10000U;
    }
    for (; carry != 0U; carry /= 10000U) {
      limb[n++] = carry % 10000U;
    }
  }
  char *o = buf;
  *o = '-';
  o += neg ? 1 : 0;
  o += fu_fmt_u32(o, limb[--n]);
  while (n != 0U) {
    put_fixed(o, limb[--n], 4U);
    o += 4;
  }
  if (d != 0U) {
    *o++ = '.';
    memset(o, '0', d);
    o += d;
  }
  *o = '\0';
  return (size_t)(o - buf);
}

size_t fu_fmt_f32(char *buf, float x, uint32_t frac_digits) {
  FX_ASSERT(frac_digits <= FU_FMT_FRAC_MAX);
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  bool neg = (b >> 31) != 0U;
  uint32_t ex = (b >> 23) & 0xFFU;
  uint32_t m = b & 0x7FFFFFU;
  if (ex == 0xFFU) {
    return fmt_special(buf, neg, m);
  }
  if (ex != 0U) {
    m |= 0x800000U;
  } else {
    ex = 1U; /* subnormal */
  }
  /* x = m * 2^(ex - 150) = ip + f / 2^s */
  uint32_t ip = 0U;
  uint32_t f = m;
  uint32_t s;
  if (ex >= 150U) {
    if (ex > 158U) {
      return fmt_big(buf, neg, m, ex - 150U, frac_digits);
    }
    ip = m << (ex - 150U);
    f = 0U;
    s = 1U;
  } else {
    s = 150U - ex;
    if (s < 24U) {
      ip = m >> s;
      f = m & ((1U << s) - 1U);
    } else if (s > 63U) {
      s = 63U; /* rounds to 0 either way */
    }
  }
  return fmt_point(buf, neg, ip, f, s, frac_digits);
}
