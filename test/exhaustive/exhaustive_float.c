/**
 * @file exhaustive_float.c
 * @brief fm_float.h on every one of the 2^32 floats (`make exhaustive`)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

static float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* the double root rounded to float is correctly rounded: 53 >= 2 * 24 + 2 */
static uint32_t ref_sqrtf_bits(uint32_t b) {
  float x = u2f(b);
  if (isnan(x)) {
    return b | 0x00400000U;
  }
  if (x < 0.0f) {
    return 0x7FC00000U;
  }
  return f2u((float)sqrt((double)x));
}

int main(void) {
  unsigned long long bs = 0U;

#pragma omp parallel for reduction(+ : bs) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t b = (uint32_t)i;
    bs += f2u(fm_sqrtf(u2f(b))) != ref_sqrtf_bits(b);
  }
  printf("exhaustive: fm_sqrtf %llu bad (all 2^32, bit for bit)\n", bs);
  return bs == 0U ? 0 : 1;
}
