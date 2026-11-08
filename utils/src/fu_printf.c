/**
 * @file fu_printf.c
 * @brief fu_snprintf: a small snprintf on top of fu_fmt
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "fx_arch.h"
#include <string.h>

typedef struct {
  char *buf;
  size_t lim; /* characters that fit: size - 1 */
  size_t n;   /* characters the text needs so far */
} Out;

static void put(Out *o, const char *s, size_t len) {
  if (o->n < o->lim) {
    size_t room = o->lim - o->n;
    memcpy(o->buf + o->n, s, len < room ? len : room);
  }
  o->n += len;
}

static void pad(Out *o, char c, size_t len) {
  if (o->n < o->lim) {
    size_t room = o->lim - o->n;
    memset(o->buf + o->n, c, len < room ? len : room);
  }
  o->n += len;
}

size_t fu_snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t n = fu_vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return n;
}

size_t fu_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
  Out o = {buf, size != 0U ? size - 1U : 0U, 0U};
  va_list args;
  va_copy(args, ap);
  for (;;) {
    const char *lit = fmt;
    while (*fmt != '\0' && *fmt != '%') {
      fmt++;
    }
    put(&o, lit, (size_t)(fmt - lit));
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
      FX_ASSERT(false); /* unknown conversion */
      if (*fmt == '\0') {
        fmt--; /* a lone % at the end */
      }
      num = false;
      break;
    }
    fmt++;

    size_t fill = width > len ? width - len : 0U;
    if (left) {
      put(&o, s, len);
      pad(&o, ' ', fill);
    } else if (zero && num) {
      size_t sign = s[0] == '-' ? 1U : 0U; /* zeros go after the sign */
      put(&o, s, sign);
      pad(&o, '0', fill);
      put(&o, s + sign, len - sign);
    } else {
      pad(&o, ' ', fill);
      put(&o, s, len);
    }
  }
  va_end(args);
  if (size != 0U) {
    buf[o.n < o.lim ? o.n : o.lim] = '\0';
  }
  return o.n;
}
