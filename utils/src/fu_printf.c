/**
 * @file fu_printf.c
 * @brief fu_snprintf: a small snprintf on top of fu_fmt
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "fx_arch.h"

#define CONV_MAX (FU_FMT_Q16_SIZE - 1U) /* fu_fmt may write this + 1 bytes */

/* bounded copies; buf[0..lim) writable, n counts all */
static size_t put(char *buf, size_t lim, size_t n, const char *s, size_t len) {
  for (size_t i = 0U; i < len && n + i < lim; i++) {
    buf[n + i] = s[i];
  }
  return n + len;
}

static size_t pad(char *buf, size_t lim, size_t n, char c, size_t len) {
  for (size_t i = 0U; i < len && n + i < lim; i++) {
    buf[n + i] = c;
  }
  return n + len;
}

size_t fu_snprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t n = fu_vsnprintf(buf, size, fmt, ap);
  va_end(ap);
  return n;
}

size_t fu_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
  size_t lim = size != 0U ? size - 1U : 0U;
  size_t n = 0U;
  va_list args;
  va_copy(args, ap);
  for (;;) {
    char c = *fmt;
    for (; n < lim && c != '%' && c != '\0'; c = *++fmt) {
      buf[n++] = c;
    }
    for (; c != '%' && c != '\0'; c = *++fmt) {
      n++;
    }
    if (c == '\0') {
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

    /* fast path: format straight into buf if the field fits */
    char tmp[FU_FMT_Q16_SIZE];
    bool fit = n <= lim && lim - n >= (width > CONV_MAX ? width : CONV_MAX);
    char *d = fit ? buf + n : tmp;
    const char *s = d;
    size_t len = 0U;
    bool num = true;
    switch (*fmt) {
    case 'd':
    case 'i':
      len = fu_fmt_i32(d, lng ? (int32_t)va_arg(args, long)
                              : (int32_t)va_arg(args, int));
      break;
    case 'u':
      len = fu_fmt_u32(d, lng ? (uint32_t)va_arg(args, unsigned long)
                              : va_arg(args, unsigned));
      break;
    case 'x':
    case 'X':
      len = fu_fmt_hex32(d,
                         lng ? (uint32_t)va_arg(args, unsigned long)
                             : va_arg(args, unsigned),
                         zero && !left && width <= 8U ? (uint32_t)width : 0U,
                         *fmt == 'X');
      break;
    case 'q': {
      FX_ASSERT(prec <= (int32_t)FU_FMT_FRAC_MAX);
      uint32_t fd = prec < 0 ? 4U : (uint32_t)prec;
      len = fu_fmt_q16(d,
                       lng ? (int32_t)va_arg(args, long)
                           : (int32_t)va_arg(args, int),
                       fd < FU_FMT_FRAC_MAX ? fd : FU_FMT_FRAC_MAX);
      break;
    }
    case 'c':
      tmp[0] = (char)va_arg(args, int);
      s = tmp;
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
      s = tmp;
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
    if (num && fit) {
      if (left) {
        for (size_t i = len; i < width; i++) {
          d[i] = ' ';
        }
      } else if (fill != 0U) {
        size_t keep = zero && d[0] == '-' ? 1U : 0U; /* zeros after the sign */
        for (size_t i = len; i-- > keep;) {
          d[i + fill] = d[i];
        }
        for (size_t i = keep; i < keep + fill; i++) {
          d[i] = zero ? '0' : ' ';
        }
      }
      n += len + fill;
    } else if (left) {
      n = put(buf, lim, n, s, len);
      n = pad(buf, lim, n, ' ', fill);
    } else if (zero && num) {
      size_t sign = s[0] == '-' ? 1U : 0U;
      n = put(buf, lim, n, s, sign);
      n = pad(buf, lim, n, '0', fill);
      n = put(buf, lim, n, s + sign, len - sign);
    } else {
      n = pad(buf, lim, n, ' ', fill);
      n = put(buf, lim, n, s, len);
    }
  }
  va_end(args);
  if (size != 0U) {
    buf[n < lim ? n : lim] = '\0';
  }
  return n;
}
