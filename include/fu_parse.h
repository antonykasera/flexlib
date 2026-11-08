/**
 * @file fu_parse.h
 * @brief Text to numbers without libc: u32, i32, hex and Q16.16
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Input is s[0..len), no NUL needed. With used, the number is a prefix and used
 * gets its length; with used NULL, all of s must be the number.
 */

#ifndef FU_PARSE_H
#define FU_PARSE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  FU_PARSE_OK,
  FU_PARSE_SYNTAX, /* no digits, or (used NULL) more after them; out unchanged */
  FU_PARSE_RANGE,  /* too large: out saturates */
} FU_ParseStatus;

FU_ParseStatus fu_parse_u32(const char *s, size_t len, uint32_t *out,
                            size_t *used);
FU_ParseStatus fu_parse_i32(const char *s, size_t len, int32_t *out,
                            size_t *used); /* optional + or - */

/* optional 0x or 0X; a-f in either case */
FU_ParseStatus fu_parse_hex32(const char *s, size_t len, uint32_t *out,
                              size_t *used);

/* [+-]digits[.digits], either side may be empty; correctly rounded to the
 * nearest 2^-16 (ties to even), so fu_fmt_q16's text reads back exactly */
FU_ParseStatus fu_parse_q16(const char *s, size_t len, int32_t *out,
                            size_t *used);

#endif /* FU_PARSE_H */
