/**
 * @file exhaustive_fixed.c
 * @brief fm_fixed.h on every input
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fm_fixed.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static long long ref_to_fixed(float x, int frac, long long lo, long long hi) {
  if (x != x) {
    return 0;
  }
  double d = nearbyint(ldexp((double)x, frac));
  return d < (double)lo ? lo : (d > (double)hi ? hi : (long long)d);
}

static long long clamp(long long x, long long lo, long long hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

int main(void) {
  unsigned long long arith = 0U, to_q = 0U, to_f = 0U;

#pragma omp parallel for reduction(+ : arith, to_q, to_f) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t u = (uint32_t)i;
    fm_q15 a = (fm_q15)(u >> 16);
    fm_q15 b = (fm_q15)u;
    arith += fm_add_q15(a, b) != clamp(a + b, -32768, 32767);
    arith += fm_sub_q15(a, b) != clamp(a - b, -32768, 32767);
    arith += fm_mul_q15(a, b) != clamp((a * b + 0x4000) >> 15, -32768, 32767);

    float x;
    memcpy(&x, &u, sizeof x);
    to_q += fm_f32_to_q15(x) != ref_to_fixed(x, 15, -32768, 32767);
    to_q += fm_f32_to_q31(x) != ref_to_fixed(x, 31, INT32_MIN, INT32_MAX);
    to_q += fm_f32_to_q16(x) != ref_to_fixed(x, 16, INT32_MIN, INT32_MAX);

    int32_t q = (int32_t)u;
    float f31 = fm_q31_to_f32(q);
    float w31 = (float)ldexp((double)q, -31);
    float f16 = fm_q16_to_f32(q);
    float w16 = (float)ldexp((double)q, -16);
    to_f += memcmp(&f31, &w31, sizeof f31) != 0;
    to_f += memcmp(&f16, &w16, sizeof f16) != 0;
  }
  printf("exhaustive: fm_fixed q15 add/sub/mul %llu bad (all pairs), f32 to Q "
         "%llu bad (all floats), Q to f32 %llu bad (all values)\n",
         arith, to_q, to_f);
  return (arith | to_q | to_f) == 0U ? 0 : 1;
}
