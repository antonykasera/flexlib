/**
 * @file exhaustive_float.c
 * @brief fm_float.h on every one of the 2^32 floats, bit for bit against
 *        correctly rounded references (`make exhaustive`)
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

/* double division then float rounding is correctly rounded: 53 >= 2 * 24 + 1 */
static uint32_t ref_recipf_bits(uint32_t b) {
  float x = u2f(b);
  return isnan(x) ? b | 0x00400000U : f2u((float)(1.0 / (double)x));
}

__extension__ typedef unsigned __int128 u128;

/* sign of a - 2^s, for a >= 1 */
static int cmp_pow2(u128 a, int s) {
  if (s < 0) {
    return 1;
  }
  u128 p = (u128)1U << s;
  return a < p ? -1 : (a > p ? 1 : 0);
}

/* r is the nearest float to 1/sqrt(x), x > 0 finite: 1/sqrt(x) > mid iff
 * mid^2 x < 1, exact in 128 bits (the result is always normal) */
static int rsqrt_nearest(uint32_t xb, uint32_t rb) {
  int ex = (int)(xb >> 23);
  uint32_t mx = xb & 0x7FFFFFU;
  if (ex == 0) {
    ex = 1;
  } else {
    mx |= 0x800000U;
  }
  ex -= 150;
  int er = (int)(rb >> 23);
  if (er == 0 || er == 255) {
    return 0;
  }
  uint32_t m = (rb & 0x7FFFFFU) | 0x800000U;
  er -= 150;
  u128 kh = 2U * (u128)m + 1U;
  u128 kl = m == 0x800000U ? 4U * (u128)m - 1U : 2U * (u128)m - 1U;
  int sl = m == 0x800000U ? er - 2 : er - 1;
  return cmp_pow2(kh * kh * mx, -(2 * (er - 1) + ex)) > 0 &&
         cmp_pow2(kl * kl * mx, -(2 * sl + ex)) < 0;
}

static int rsqrtf_ok(uint32_t b, uint32_t r) {
  float x = u2f(b);
  if (isnan(x)) {
    return r == (b | 0x00400000U);
  }
  if ((b << 1) == 0U) {
    return r == (b | 0x7F800000U);
  }
  if ((int32_t)b < 0) {
    return r == 0x7FC00000U;
  }
  return b == 0x7F800000U ? r == 0U : rsqrt_nearest(b, r);
}

int main(void) {
  unsigned long long bs = 0U, br = 0U, bq = 0U;

#pragma omp parallel for reduction(+ : bs, br, bq) schedule(static)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t b = (uint32_t)i;
    float x = u2f(b);
    bs += f2u(fm_sqrtf(x)) != ref_sqrtf_bits(b);
    br += f2u(fm_recipf(x)) != ref_recipf_bits(b);
    bq += !rsqrtf_ok(b, f2u(fm_rsqrtf(x)));
  }
  printf("exhaustive: fm_sqrtf %llu bad, fm_recipf %llu, fm_rsqrtf %llu (all "
         "2^32, bit for bit)\n",
         bs, br, bq);
  return (bs | br | bq) == 0U ? 0 : 1;
}
