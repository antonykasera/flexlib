/**
 * @file fu_fmt.h
 * @brief Numbers to text without printf, and a small snprintf
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_FMT_H
#define FU_FMT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* buf sizes; each fu_fmt_* writes text + NUL and returns the length */
#define FU_FMT_U32_SIZE 11U   /* "4294967295" + NUL */
#define FU_FMT_I32_SIZE 12U   /* "-2147483648" + NUL */
#define FU_FMT_HEX32_SIZE 9U  /* "ffffffff" + NUL */
#define FU_FMT_FRAC_MAX 9U    /* most fraction digits for q16 and f32 */
#define FU_FMT_Q16_SIZE 17U   /* "-32768." + 9 digits + NUL */
#define FU_FMT_F32_SIZE 51U   /* "-" + 39 digits + "." + 9 digits + NUL */

/* decimal digits of v (1 for 0) */
uint32_t fu_fmt_dec_digits(uint32_t v);

size_t fu_fmt_u32(char *buf, uint32_t v);
size_t fu_fmt_i32(char *buf, int32_t v);

/* zero-padded to min_digits (0..8), at least one digit */
size_t fu_fmt_hex32(char *buf, uint32_t v, uint32_t min_digits, bool upper);

/* same as printf "%.*f", frac_digits 0..9; NaN prints "nan" */
size_t fu_fmt_q16(char *buf, int32_t v, uint32_t frac_digits);
size_t fu_fmt_f32(char *buf, float x, uint32_t frac_digits);

/* %d %i %u %x %X %c %s %% %q (Q16.16); flags - 0, width, .prec, *, l;
 * truncates, returns the full length */
size_t fu_snprintf(char *buf, size_t size, const char *fmt, ...);
size_t fu_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#endif /* FU_FMT_H */
