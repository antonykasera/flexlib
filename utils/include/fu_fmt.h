/**
 * @file fu_fmt.h
 * @brief Numbers to text without printf, and a small snprintf
 *
 * Each `fu_fmt_` function writes the text and a NUL to `buf`, which holds the
 * matching `_SIZE`, and returns the length without the NUL.
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

/** Buffer size for fu_fmt_u32(). */
#define FU_FMT_U32_SIZE 11U
/** Buffer size for fu_fmt_i32(). */
#define FU_FMT_I32_SIZE 12U
/** Buffer size for fu_fmt_hex32(). */
#define FU_FMT_HEX32_SIZE 9U
/** Most fraction digits fu_fmt_q16() and fu_fmt_f32() take. */
#define FU_FMT_FRAC_MAX 9U
/** Buffer size for fu_fmt_q16(). */
#define FU_FMT_Q16_SIZE 17U
/** Buffer size for fu_fmt_f32(), any float. */
#define FU_FMT_F32_SIZE 51U

/** Decimal digits in v; 1 for 0. */
uint32_t fu_fmt_dec_digits(uint32_t v);

/** v in decimal. */
size_t fu_fmt_u32(char *buf, uint32_t v);
/** v in decimal, `-` first when negative. */
size_t fu_fmt_i32(char *buf, int32_t v);

/**
 * v in hex, no prefix.
 *
 * @param buf        FU_FMT_HEX32_SIZE bytes
 * @param v          value
 * @param min_digits pad with zeros to this many digits, 0 to 8
 * @param upper      `A-F` rather than `a-f`
 */
size_t fu_fmt_hex32(char *buf, uint32_t v, uint32_t min_digits, bool upper);

/**
 * A Q16.16 value exactly as `printf("%.*f", frac_digits, v / 65536.0)`
 * prints it.
 *
 * @param buf         FU_FMT_Q16_SIZE bytes
 * @param v           Q16.16
 * @param frac_digits 0 to FU_FMT_FRAC_MAX
 */
size_t fu_fmt_q16(char *buf, int32_t v, uint32_t frac_digits);

/**
 * x exactly as `printf("%.*f", frac_digits, (double)x)` prints it, except
 * that every NaN prints `nan`.
 *
 * @param buf         FU_FMT_F32_SIZE bytes
 * @param x           any float
 * @param frac_digits 0 to FU_FMT_FRAC_MAX
 */
size_t fu_fmt_f32(char *buf, float x, uint32_t frac_digits);

/**
 * A small snprintf; an unknown conversion asserts.
 *
 * ```
 * conversions  %d %i %u %x %X %c %s %%, and %q for Q16.16
 * flags        - 0, width, *, l
 * precision    %s; %q fraction digits, 4 by default, 9 at most
 * ```
 *
 * @param buf  size bytes; may be NULL when size is 0
 * @param size the text is cut to size - 1 bytes and NUL-terminated
 * @param fmt  format
 * @return the full length, as if buf were large enough
 */
size_t fu_snprintf(char *buf, size_t size, const char *fmt, ...);
/** fu_snprintf() with a `va_list`. */
size_t fu_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#endif /* FU_FMT_H */
