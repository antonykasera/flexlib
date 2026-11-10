/**
 * @file exhaustive_parse.c
 * @brief fu_parse on fu_fmt's text for every 32-bit value
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include "fu_parse.h"
#include <stdio.h>

int main(void) {
  unsigned long long b0 = 0U, b1 = 0U, b2 = 0U, b3 = 0U;

#pragma omp parallel for reduction(+ : b0, b1, b2, b3) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t v = (uint32_t)i;
    char s[24];
    uint32_t u = 0U;
    int32_t q = 0;

    size_t n = fu_fmt_u32(s, v);
    b0 += fu_parse_u32(s, n, &u, NULL) != FU_PARSE_OK || u != v;

    n = fu_fmt_i32(s, (int32_t)v);
    b1 += fu_parse_i32(s, n, &q, NULL) != FU_PARSE_OK || q != (int32_t)v;

    /* prefix, width and case vary with the value */
    size_t p = (v & 0x200U) != 0U ? 2U : 0U;
    s[0] = '0';
    s[1] = 'x';
    n = p + fu_fmt_hex32(s + p, v, v % 9U, (v & 0x100U) != 0U);
    b2 += fu_parse_hex32(s, n, &u, NULL) != FU_PARSE_OK || u != v;

    /* 5 fraction digits are enough to be unique */
    n = fu_fmt_q16(s, (int32_t)v, 5U);
    b3 += fu_parse_q16(s, n, &q, NULL) != FU_PARSE_OK || q != (int32_t)v;
  }
  printf("exhaustive: fu_parse u32 %llu bad, i32 %llu, hex32 %llu, q16 %llu, "
         "of 4294967296 each\n",
         b0, b1, b2, b3);
  return (b0 | b1 | b2 | b3) == 0U ? 0 : 1;
}
