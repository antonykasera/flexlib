/**
 * @file fu_fmt.h
 * @brief Integers to text without printf: decimal and hex
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 *
 * Each function writes the digits and a terminating NUL to buf, and returns
 * the length without the NUL. buf must hold the matching FU_FMT_*_SIZE, and
 * any of those bytes may be written.
 */

#ifndef FU_FMT_H
#define FU_FMT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FU_FMT_U32_SIZE 11U   /* "4294967295" + NUL */
#define FU_FMT_I32_SIZE 12U   /* "-2147483648" + NUL */
#define FU_FMT_HEX32_SIZE 9U  /* "ffffffff" + NUL */

/* number of decimal digits in v (1 for 0) */
uint32_t fu_fmt_dec_digits(uint32_t v);

size_t fu_fmt_u32(char *buf, uint32_t v);
size_t fu_fmt_i32(char *buf, int32_t v);

/* At least min_digits (0..8) digits, zero-padded; always at least one. */
size_t fu_fmt_hex32(char *buf, uint32_t v, uint32_t min_digits, bool upper);

#endif /* FU_FMT_H */
