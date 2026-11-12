/**
 * @file cases_fmt.c
 * @brief Bench cases for fu_fmt.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fu_bits.h"
#include "fu_fmt.h"
#include <stdarg.h>
#include <stdio.h>

static char text[16];
static char line[64]; /* f32 and printf cases */

/* ---- decimal candidates (fu_fmt_u32: two digits per step, a loop) ---- */

/* fu_fmt_dec_digits, inlined as in fu_fmt_u32 */
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

/* all ten digits branch-free, then the last n */
BENCH_CALL static size_t u32_split(char *buf, uint32_t v) {
  uint32_t n = dec_digits(v);
  uint32_t hi = v / 100000000U;
  uint32_t lo = v - hi * 100000000U;

  /* digits in t[0..9], NUL at t[10] */
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

/* one digit per step */
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

/* ---- fu_snprintf candidates ---- */

/* the first version: each conversion into tmp, then newlib memcpy/memset */
static size_t v1_vsnprintf(char *buf, size_t size, const char *fmt,
                           va_list ap);

typedef struct {
  char *buf;
  size_t lim;
  size_t n;
} V1Out;

static void v1_put(V1Out *o, const char *s, size_t len) {
  if (o->n < o->lim) {
    size_t room = o->lim - o->n;
    memcpy(o->buf + o->n, s, len < room ? len : room);
  }
  o->n += len;
}

static void v1_pad(V1Out *o, char c, size_t len) {
  if (o->n < o->lim) {
    size_t room = o->lim - o->n;
    memset(o->buf + o->n, c, len < room ? len : room);
  }
  o->n += len;
}

BENCH_CALL static size_t v1_snprintf(char *buf, size_t size, const char *fmt,
                                     ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t n = v1_vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return n;
}

static size_t v1_vsnprintf(char *buf, size_t size, const char *fmt,
                           va_list ap) {
  V1Out o = {buf, size != 0U ? size - 1U : 0U, 0U};
  va_list args;
  va_copy(args, ap);
  for (;;) {
    const char *lit = fmt;
    while (*fmt != '\0' && *fmt != '%') {
      fmt++;
    }
    v1_put(&o, lit, (size_t)(fmt - lit));
    if (*fmt == '\0') {
      break;
    }
    fmt++;

    bool left = false;
    bool zero = false;
    for (;; fmt++) {
      if (*fmt == '-') {
        left = true;
      } else if (*fmt == '0') {
        zero = true;
      } else {
        break;
      }
    }
    size_t width = 0U;
    if (*fmt == '*') {
      int w = va_arg(args, int);
      left = left || w < 0;
      width = w < 0 ? 0U - (size_t)w : (size_t)w;
      fmt++;
    }
    for (; *fmt >= '0' && *fmt <= '9'; fmt++) {
      width = width * 10U + (size_t)(*fmt - '0');
    }
    int32_t prec = -1;
    if (*fmt == '.') {
      fmt++;
      prec = 0;
      if (*fmt == '*') {
        int p = va_arg(args, int);
        prec = p < 0 ? -1 : (int32_t)p;
        fmt++;
      }
      for (; *fmt >= '0' && *fmt <= '9'; fmt++) {
        prec = prec * 10 + (*fmt - '0');
      }
    }
    bool lng = *fmt == 'l';
    fmt += lng ? 1 : 0;

    char tmp[FU_FMT_Q16_SIZE];
    const char *s = tmp;
    size_t len = 0U;
    bool num = true;
    switch (*fmt) {
    case 'd':
    case 'i':
      len = fu_fmt_i32(tmp, lng ? (int32_t)va_arg(args, long)
                                : (int32_t)va_arg(args, int));
      break;
    case 'u':
      len = fu_fmt_u32(tmp, lng ? (uint32_t)va_arg(args, unsigned long)
                                : va_arg(args, unsigned));
      break;
    case 'x':
    case 'X':
      len = fu_fmt_hex32(tmp,
                         lng ? (uint32_t)va_arg(args, unsigned long)
                             : va_arg(args, unsigned),
                         0U, *fmt == 'X');
      break;
    case 'q': {
      FX_ASSERT(prec <= (int32_t)FU_FMT_FRAC_MAX);
      uint32_t d = prec < 0 ? 4U : (uint32_t)prec;
      len = fu_fmt_q16(tmp,
                       lng ? (int32_t)va_arg(args, long)
                           : (int32_t)va_arg(args, int),
                       d < FU_FMT_FRAC_MAX ? d : FU_FMT_FRAC_MAX);
      break;
    }
    case 'c':
      tmp[0] = (char)va_arg(args, int);
      len = 1U;
      num = false;
      break;
    case 's':
      s = va_arg(args, const char *);
      FX_ASSERT(s != NULL);
      while ((prec < 0 || len < (size_t)prec) && s[len] != '\0') {
        len++;
      }
      num = false;
      break;
    case '%':
      tmp[0] = '%';
      len = 1U;
      num = false;
      break;
    default:
      FX_ASSERT(false);
      if (*fmt == '\0') {
        fmt--;
      }
      num = false;
      break;
    }
    fmt++;

    size_t fill = width > len ? width - len : 0U;
    if (left) {
      v1_put(&o, s, len);
      v1_pad(&o, ' ', fill);
    } else if (zero && num) {
      size_t sign = s[0] == '-' ? 1U : 0U;
      v1_put(&o, s, sign);
      v1_pad(&o, '0', fill);
      v1_put(&o, s + sign, len - sign);
    } else {
      v1_pad(&o, ' ', fill);
      v1_put(&o, s, len);
    }
  }
  va_end(args);
  if (size != 0U) {
    buf[o.n < o.lim ? o.n : o.lim] = '\0';
  }
  return o.n;
}

/* ---- inputs ---- */

/* 1e9 .. 2^32 - 1: always 10 digits */
static inline uint32_t in_10dig(uint32_t u) {
  return 1000000000U + u % 3294967296U;
}
/* every digit count, about equally often per bit length */
static inline uint32_t in_logu(uint32_t u) { return u >> (u & 31U); }
static inline int32_t in_logs(uint32_t u) { return (int32_t)u >> (u & 31U); }

static inline float f32_bits(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}
/* +-2^-7 .. 2^13: readings, setpoints */
static inline float in_f32_common(uint32_t u) {
  return f32_bits((u & 0x807FFFFFU) | (120U + u % 20U) << 23);
}
/* 2^32 .. FLT_MAX: the long path */
static inline float in_f32_big(uint32_t u) {
  return f32_bits((u & 0x807FFFFFU) | (159U + u % 96U) << 23);
}

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

BENCH_TEXT(b_q16_4, int32_t, in_logs, fu_fmt_q16(text, x, 4U), text)
BENCH_TEXT(b_q16_snprintf, int32_t, in_logs,
           snprintf(line, sizeof line, "%.4f", (double)x / 65536.0), line)
BENCH_TEXT(b_f32_2, float, in_f32_common, fu_fmt_f32(line, x, 2U), line)
BENCH_TEXT(b_f32_snprintf, float, in_f32_common,
           snprintf(line, sizeof line, "%.2f", (double)x), line)
BENCH_TEXT(b_f32_big, float, in_f32_big, fu_fmt_f32(line, x, 0U), line)
BENCH_TEXT(b_f32_big_snprintf, float, in_f32_big,
           snprintf(line, sizeof line, "%.0f", (double)x), line)

/* casts: uint32_t is unsigned long on Arm */
#define LINE_FMT "n=%u t=%d id=%08X\r\n"
BENCH_TEXT(b_printf_fu, uint32_t, in_logu,
           fu_snprintf(line, sizeof line, LINE_FMT, (unsigned)x,
                       (int)x >> 4, (unsigned)x),
           line)
BENCH_TEXT(b_printf_v1, uint32_t, in_logu,
           v1_snprintf(line, sizeof line, LINE_FMT, (unsigned)x, (int)x >> 4,
                       (unsigned)x),
           line)
BENCH_TEXT(b_printf_newlib, uint32_t, in_logu,
           snprintf(line, sizeof line, LINE_FMT, (unsigned)x, (int)x >> 4,
                    (unsigned)x),
           line)

/* "log" inputs have every digit count */
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
    {"fu_fmt_q16, %.4f, log", "flexlib", b_q16_4, true},
    {"snprintf %.4f of q16, log", "newlib", b_q16_snprintf, true},
    {"fu_fmt_f32, %.2f, 2^-7..2^13", "flexlib", b_f32_2, true},
    {"snprintf %.2f, 2^-7..2^13", "newlib", b_f32_snprintf, true},
    {"fu_fmt_f32, %.0f, >= 2^32", "flexlib", b_f32_big, true},
    {"snprintf %.0f, >= 2^32", "newlib", b_f32_big_snprintf, true},
    {"fu_snprintf u32 + i32 + hex line", "flexlib", b_printf_fu, true},
    {"fu_snprintf v1 (tmp + memcpy), same line", "candidate", b_printf_v1,
     true},
    {"snprintf u32 + i32 + hex line", "newlib", b_printf_newlib, true},
};

const BenchGroup bench_group_fmt = {cases, BENCH_COUNT(cases)};
