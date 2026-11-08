/**
 * @file fu_fmt.h
 * @brief Numbers to text without printf: integers, Q16.16, float, and a small
 *        snprintf
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Each writes text + NUL and returns the length; buf needs FU_FMT_*_SIZE bytes,
 * any of which may be written.
 */

#ifndef FU_FMT_H
#define FU_FMT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FU_FMT_U32_SIZE 11U   /* "4294967295" + NUL */
#define FU_FMT_I32_SIZE 12U   /* "-2147483648" + NUL */
#define FU_FMT_HEX32_SIZE 9U  /* "ffffffff" + NUL */
#define FU_FMT_FRAC_MAX 9U    /* most fraction digits for q16 and f32 */
#define FU_FMT_Q16_SIZE 17U   /* "-32768." + 9 digits + NUL */
#define FU_FMT_F32_SIZE 51U   /* "-" + 39 digits + "." + 9 digits + NUL */

/* number of decimal digits in v (1 for 0) */
uint32_t fu_fmt_dec_digits(uint32_t v);

size_t fu_fmt_u32(char *buf, uint32_t v);
size_t fu_fmt_i32(char *buf, int32_t v);

/* At least min_digits (0..8) digits, zero-padded; always at least one. */
size_t fu_fmt_hex32(char *buf, uint32_t v, uint32_t min_digits, bool upper);

/* Exactly printf's "%.*f" (correctly rounded, ties to even), frac_digits 0..9.
 * Every float, no soft-float; NaN prints "nan". */
size_t fu_fmt_q16(char *buf, int32_t v, uint32_t frac_digits);
size_t fu_fmt_f32(char *buf, float x, uint32_t frac_digits);

/* %d %i %u %x %X %c %s %% and %q (Q16.16, .4 default, .9 max); flags - 0,
 * width, precision, *, l. Truncates and returns the full length. */
size_t fu_snprintf(char *buf, size_t size, const char *fmt, ...);
size_t fu_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#endif /* FU_FMT_H */
