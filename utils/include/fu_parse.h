/**
 * @file fu_parse.h
 * @brief Text to numbers without libc: u32, i32, hex and Q16.16
 *
 * Each parser reads `s[0, len)`, needs no NUL and skips no spaces. With
 * `used` NULL, all of `s` must be the number; otherwise the number may be a
 * prefix, and `*used` gets the characters it took.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FU_PARSE_H
#define FU_PARSE_H

#include <stddef.h>
#include <stdint.h>

/** Parser result. */
typedef enum {
  FU_PARSE_OK,     /**< parsed */
  FU_PARSE_SYNTAX, /**< no number, or text after it; out and used unchanged */
  FU_PARSE_RANGE,  /**< too large; out saturated */
} FU_ParseStatus;

/** Decimal digits. */
FU_ParseStatus fu_parse_u32(const char *s, size_t len, uint32_t *out,
                            size_t *used);
/** Decimal digits after an optional `+` or `-`. */
FU_ParseStatus fu_parse_i32(const char *s, size_t len, int32_t *out,
                            size_t *used);

/** Hex digits in either case, after an optional `0x` or `0X`. */
FU_ParseStatus fu_parse_hex32(const char *s, size_t len, uint32_t *out,
                              size_t *used);

/** `[+-]digits[.digits]` to Q16.16, rounded to nearest, ties to even. One
 * side of the point may be empty. */
FU_ParseStatus fu_parse_q16(const char *s, size_t len, int32_t *out,
                            size_t *used);

#endif /* FU_PARSE_H */
