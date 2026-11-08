/**
 * @file exhaustive_fmt_slow.c
 * @brief Checks fu_fmt_q16 and fu_fmt_f32 on every 32-bit input against the C
 *        library's "%.*f" (`make exhaustive-slow`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  unsigned long long bad_q16 = 0U;
  unsigned long long bad_f32 = 0U;

#pragma omp parallel for reduction(+ : bad_q16, bad_f32) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t u = (uint32_t)i;
    char want[64];
    char got[64];
    /* the digit count changes with the input, so every count sees every
     * fraction pattern */
    uint32_t d = (u ^ (u >> 16)) % 10U;

    int32_t v = (int32_t)u;
    int n = snprintf(want, sizeof want, "%.*f", (int)d, (double)v / 65536.0);
    if (fu_fmt_q16(got, v, d) != (size_t)n || strcmp(got, want) != 0) {
      if (bad_q16++ < 4U) {
        printf("  fu_fmt_q16(%ld, %lu): got \"%s\", want \"%s\"\n", (long)v,
               (unsigned long)d, got, want);
      }
    }

    float x;
    memcpy(&x, &u, sizeof x);
    n = x != x ? snprintf(want, sizeof want, "nan")
               : snprintf(want, sizeof want, "%.*f", (int)d, (double)x);
    if (fu_fmt_f32(got, x, d) != (size_t)n || strcmp(got, want) != 0) {
      if (bad_f32++ < 4U) {
        printf("  fu_fmt_f32(%08lx, %lu): got \"%s\", want \"%s\"\n",
               (unsigned long)u, (unsigned long)d, got, want);
      }
    }
  }

  printf("exhaustive: fu_fmt_q16 %llu bad, fu_fmt_f32 %llu bad, of "
         "4294967296 inputs each\n",
         bad_q16, bad_f32);
  return (bad_q16 == 0U && bad_f32 == 0U) ? 0 : 1;
}
