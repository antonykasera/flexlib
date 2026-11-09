/**
 * @file exhaustive_float.c
 * @brief fm_float.h on every one of the 2^32 floats: sqrt, 1/x, 1/sqrt bit for
 *        bit against correctly rounded references; sin, cos within their ULP
 *        bound against double, and sincos bit for bit (`make exhaustive`)
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

/* |got - ref| in ULPs at ref's binade; NaN must match NaN */
static double ulp_abs(float got, double ref) {
  if (isnan(got) || isnan(ref)) {
    return isnan(got) && isnan(ref) ? 0.0 : HUGE_VAL;
  }
  int e;
  (void)frexp(ref, &e);
  return fabs(((double)got - ref) / ldexp(1.0, (e - 1 < -126 ? -126 : e - 1) - 23));
}

int main(void) {
  unsigned long long bs = 0U, br = 0U, bq = 0U, bsc = 0U;
  double es = 0.0, ec = 0.0;

#pragma omp parallel for reduction(+ : bs, br, bq, bsc)                       \
    reduction(max : es, ec) schedule(dynamic, 65536)
  for (long long i = 0; i <= 0xFFFFFFFFLL; i++) {
    uint32_t b = (uint32_t)i;
    float x = u2f(b);
    bs += f2u(fm_sqrtf(x)) != ref_sqrtf_bits(b);
    br += f2u(fm_recipf(x)) != ref_recipf_bits(b);
    bq += !rsqrtf_ok(b, f2u(fm_rsqrtf(x)));
    float s = fm_sinf(x);
    float c = fm_cosf(x);
    float s2, c2;
    fm_sincosf(x, &s2, &c2);
    bsc += f2u(s) != f2u(s2) || f2u(c) != f2u(c2);
    double e1 = ulp_abs(s, sin((double)x));
    double e2 = ulp_abs(c, cos((double)x));
    es = e1 > es ? e1 : es;
    ec = e2 > ec ? e2 : ec;
  }
  printf("exhaustive: fm_sqrtf %llu bad, fm_recipf %llu, fm_rsqrtf %llu (all "
         "2^32, bit for bit)\n",
         bs, br, bq);
  printf("exhaustive: fm_sinf max %.4f ulp, fm_cosf %.4f (bound 0.515), "
         "fm_sincosf %llu differ (all 2^32)\n",
         es, ec, bsc);
  return (bs | br | bq | bsc) == 0U && es <= 0.515 && ec <= 0.515 ? 0 : 1;
}
