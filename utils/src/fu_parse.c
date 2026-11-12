/**
 * @file fu_parse.c
 * @brief Text to numbers without libc: u32, i32, hex and Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fu_parse.h"
#include "fu_bits.h"
#include <stdbool.h>

static const uint32_t pow10_tab[10] = {
    1U,      10U,      100U,      1000U,      10000U,
    100000U, 1000000U, 10000000U, 100000000U, 1000000000U};

/* decimal digits from s[i] on, continuing x; sets *ovf past 2^32 - 1 */
FX_INLINE size_t dec1(const char *s, size_t i, size_t len, uint32_t *v,
                      uint32_t x, bool *ovf) {
  for (; i < len; i++) {
    uint32_t d = (uint32_t)(uint8_t)s[i] - '0';
    if (d > 9U) {
      break;
    }
    if (x >= 429496729U && (x > 429496729U || d > 5U)) {
      *ovf = true;
    }
    x = x * 10U + d;
  }
  *v = x;
  return i;
}

/* same, four digits per word */
FX_INLINE size_t dec(const char *s, size_t i, size_t len, uint32_t *v,
                     bool *ovf) {
  uint32_t x = 0U;
  while (len - i >= 4U) {
    uint32_t w = fu_load_le32(s + i) - 0x30303030U;
    if ((((w + 0x76767676U) | w) & 0x80808080U) != 0U) {
      break; /* a byte outside '0'..'9' */
    }
    w = w * 10U + (w >> 8); /* bytes 0 and 2: the two pairs */
    w = (w & 0xFFU) * 100U + ((w >> 16) & 0xFFU);
    uint64_t t = (uint64_t)x * 10000U + w;
    if ((t >> 32) != 0U) {
      *ovf = true;
    }
    x = (uint32_t)t;
    i += 4U;
  }
  return dec1(s, i, len, v, x, ovf);
}

/* i: where parsing stopped; first: where the digits began */
static FU_ParseStatus finish(size_t i, size_t first, size_t len, size_t *used,
                             bool ovf) {
  if (i == first || (used == NULL && i != len)) {
    return FU_PARSE_SYNTAX;
  }
  if (used != NULL) {
    *used = i;
  }
  return ovf ? FU_PARSE_RANGE : FU_PARSE_OK;
}

FU_ParseStatus fu_parse_u32(const char *s, size_t len, uint32_t *out,
                            size_t *used) {
  FX_ASSERT(out != NULL && (s != NULL || len == 0U));
  uint32_t v;
  bool ovf = false;
  size_t i = dec(s, 0U, len, &v, &ovf);
  FU_ParseStatus st = finish(i, 0U, len, used, ovf);
  if (st != FU_PARSE_SYNTAX) {
    *out = ovf ? UINT32_MAX : v;
  }
  return st;
}

FU_ParseStatus fu_parse_i32(const char *s, size_t len, int32_t *out,
                            size_t *used) {
  FX_ASSERT(out != NULL && (s != NULL || len == 0U));
  size_t i = 0U;
  bool neg = false;
  if (len != 0U && (s[0] == '-' || s[0] == '+')) {
    neg = s[0] == '-';
    i = 1U;
  }
  uint32_t m;
  bool ovf = false;
  size_t end = dec(s, i, len, &m, &ovf);
  ovf = ovf || m > 0x7FFFFFFFU + (neg ? 1U : 0U);
  FU_ParseStatus st = finish(end, i, len, used, ovf);
  if (st != FU_PARSE_SYNTAX) {
    if (ovf) {
      *out = neg ? INT32_MIN : INT32_MAX;
    } else {
      *out = neg ? (int32_t)(0U - m) : (int32_t)m;
    }
  }
  return st;
}

/* hex digit values; 255 for any other character */
static const uint8_t hexval[256] FX_ALIGN_LINE = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255, 255,
    255, 255, 255, 255, 255, 10, 11, 12, 13, 14, 15, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255,
};

FU_ParseStatus fu_parse_hex32(const char *s, size_t len, uint32_t *out,
                              size_t *used) {
  FX_ASSERT(out != NULL && (s != NULL || len == 0U));
  size_t i = 0U;
  if (len >= 3U && s[0] == '0' && (s[1] | 0x20) == 'x' &&
      hexval[(uint8_t)s[2]] < 16U) {
    i = 2U;
  }
  size_t first = i;
  uint32_t v = 0U;
  bool ovf = false;
  for (; i < len; i++) {
    uint32_t d = hexval[(uint8_t)s[i]];
    if (d > 15U) {
      break;
    }
    ovf = ovf || (v >> 28) != 0U;
    v = (v << 4) | d;
  }
  FU_ParseStatus st = finish(i, first, len, used, ovf);
  if (st != FU_PARSE_SYNTAX) {
    *out = ovf ? UINT32_MAX : v;
  }
  return st;
}

/* up to n digits from s[*i], zero-padded on the right */
FX_INLINE uint32_t frac_chunk(const char *s, size_t *i, size_t len, uint32_t n,
                              size_t *count) {
  uint32_t x = 0U;
  uint32_t k = 0U;
  for (; *i < len && k < n; (*i)++, k++) {
    uint32_t d = (uint32_t)(uint8_t)s[*i] - '0';
    if (d > 9U) {
      break;
    }
    x = x * 10U + d;
  }
  *count += k;
  return x * pow10_tab[n - k];
}

FU_ParseStatus fu_parse_q16(const char *s, size_t len, int32_t *out,
                            size_t *used) {
  FX_ASSERT(out != NULL && (s != NULL || len == 0U));
  size_t i = 0U;
  bool neg = false;
  if (len != 0U && (s[0] == '-' || s[0] == '+')) {
    neg = s[0] == '-';
    i = 1U;
  }
  size_t first = i;
  uint32_t ip;
  bool ovf = false;
  i = dec1(s, i, len, &ip, 0U, &ovf);  size_t digits = i - first;

  /* the fraction as N / 10^17: hi = 9 digits, lo = 8, then a sticky bit */
  uint32_t hi = 0U;
  uint32_t lo = 0U;
  bool sticky = false;
  if (i < len && s[i] == '.') {
    i++;
    hi = frac_chunk(s, &i, len, 9U, &digits);
    lo = frac_chunk(s, &i, len, 8U, &digits);
    for (; i < len && (uint32_t)(uint8_t)s[i] - '0' <= 9U; i++) {
      sticky = sticky || s[i] != '0';
      digits++;
    }
  }
  if (digits == 0U || (used == NULL && i != len)) {
    return FU_PARSE_SYNTAX;
  }
  if (used != NULL) {
    *used = i;
  }

  /* t = floor(N / 5^17): estimate from hi, then at most one more */
  const uint64_t five17 = 762939453125ULL;
  uint32_t a = hi / 1953125U;
  uint32_t t = a * 256U + ((hi - a * 1953125U) * 256U) / 1953125U;
  uint64_t n = (uint64_t)hi * 100000000U + lo;
  if (n >= (uint64_t)(t + 1U) * five17) {
    t++;
  }
  bool above = n != (uint64_t)t * five17 || sticky;
  uint32_t q = t >> 1;
  q += (t & 1U) & ((above ? 1U : 0U) | (q & 1U)); /* nearest, ties to even */

  ovf = ovf || ip > 32768U;
  uint32_t m = ovf ? 0xFFFFFFFFU : (ip << 16) + q;
  if (m > 0x7FFFFFFFU + (neg ? 1U : 0U)) {
    *out = neg ? INT32_MIN : INT32_MAX;
    return FU_PARSE_RANGE;
  }
  *out = neg ? (int32_t)(0U - m) : (int32_t)m;
  return FU_PARSE_OK;
}
