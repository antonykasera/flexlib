/**
 * @file exhaustive_fmt.c
 * @brief Checks fu_fmt_u32 and fu_fmt_hex32 on every 32-bit input against
 *        plain one-digit-at-a-time references (`make exhaustive`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fu_fmt.h"
#include <stdio.h>
#include <string.h>

static size_t ref_dec(char *out, uint32_t v) {
  char tmp[10];
  size_t n = 0U;
  do {
    tmp[n++] = (char)('0' + v % 10U);
    v /= 10U;
  } while (v != 0U);
  for (size_t i = 0U; i < n; i++) {
    out[i] = tmp[n - 1U - i];
  }
  out[n] = '\0';
  return n;
}

static size_t ref_hex(char *out, uint32_t v, size_t min_digits, bool upper) {
  const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  size_t n = 1U;
  while (n < 8U && (v >> (4U * n)) != 0U) {
    n++;
  }
  if (n < min_digits) {
    n = min_digits;
  }
  for (size_t i = 0U; i < n; i++) {
    out[n - 1U - i] = dig[(v >> (4U * i)) & 0xFU];
  }
  out[n] = '\0';
  return n;
}

int main(void) {
  unsigned long long bad_dec = 0U;
  unsigned long long bad_hex = 0U;

#pragma omp parallel for reduction(+ : bad_dec, bad_hex) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t v = (uint32_t)i;
    char want[16];
    char got[16];

    size_t n = ref_dec(want, v);
    if (fu_fmt_u32(got, v) != n || memcmp(got, want, n + 1U) != 0) {
      if (bad_dec++ < 4U) {
        printf("  fu_fmt_u32(%lu): got \"%s\", want \"%s\"\n",
               (unsigned long)v, got, want);
      }
    }

    /* alternate width and case so all of them see every input value */
    size_t digits = (size_t)(v % 9U);
    bool upper = (v & 0x100U) != 0U;
    n = ref_hex(want, v, digits, upper);
    if (fu_fmt_hex32(got, v, (uint32_t)digits, upper) != n ||
        memcmp(got, want, n + 1U) != 0) {
      if (bad_hex++ < 4U) {
        printf("  fu_fmt_hex32(%08lx, %zu): got \"%s\", want \"%s\"\n",
               (unsigned long)v, digits, got, want);
      }
    }
  }

  printf("exhaustive: fu_fmt_u32 %llu bad, fu_fmt_hex32 %llu bad, of "
         "4294967296 inputs each\n",
         bad_dec, bad_hex);
  return (bad_dec == 0U && bad_hex == 0U) ? 0 : 1;
}
