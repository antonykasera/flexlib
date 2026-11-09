/**
 * @file test_fm_float.c
 * @brief Host tests for fm_float.h against correctly rounded references (every
 *        float is in test/exhaustive/exhaustive_float.c)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fm_float.h"
#include "test_stats.h"
#include <stdbool.h>
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

static void check_sqrtf(uint32_t b) {
  ASSERT_EQ(f2u(fm_sqrtf(u2f(b))), ref_sqrtf_bits(b));
}

void test_fm_sqrtf(void) {
  ASSERT_EQ(f2u(fm_sqrtf(0.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_sqrtf(-0.0f)), 0x80000000U);
  ASSERT_EQ(f2u(fm_sqrtf(INFINITY)), 0x7F800000U);
  ASSERT_EQ(f2u(fm_sqrtf(-INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(-1.0f)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0x80000001U))), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0x7F800001U))), 0x7FC00001U); /* quieted */
  ASSERT_EQ(f2u(fm_sqrtf(u2f(0xFFC01234U))), 0xFFC01234U);
  ASSERT_EQ(f2u(fm_sqrtf(1.0f)), f2u(1.0f));
  ASSERT_EQ(f2u(fm_sqrtf(4.0f)), f2u(2.0f));
  ASSERT_EQ(f2u(fm_sqrtf(0x1p-148f)), f2u(0x1p-74f));
  check_sqrtf(0x00000001U); /* 2^-149 */
  check_sqrtf(0x007FFFFFU);
  check_sqrtf(0x00800000U);
  check_sqrtf(0x7F7FFFFFU);
  for (uint32_t b = 0U; b < 0x00800000U; b += 7U) {
    check_sqrtf(b);
  }
  /* exact squares (12-bit roots, even exponent shifts) and their neighbours */
  uint64_t st = 151U;
  for (int i = 0; i < 100000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t s = (u >> 20) | 0x800U;
    uint32_t sb = f2u((float)(s * s)) - 0x20000000U + ((u & 63U) << 24);
    check_sqrtf(sb - 1U);
    check_sqrtf(sb);
    check_sqrtf(sb + 1U);
  }
  ErrStats es;
  stats_init(&es, "fm_sqrtf, random bits", "ulp");
  for (int i = 0; i < 1000000; i++) {
    uint32_t b = test_rng_u32(&st);
    check_sqrtf(b);
    float x = u2f(b & 0x7FFFFFFFU);
    if (!isnan(x)) {
      stats_add(&es, (double)x, ulp_err_f32(fm_sqrtf(x), sqrt((double)x)));
    }
  }
  ASSERT_STATS_MAX(&es, 0.5);
}

/* the double quotient rounded to float is correctly rounded (53 >= 2 * 24 +
 * 1), subnormal and overflowing results too */
static uint32_t ref_recipf_bits(uint32_t b) {
  float x = u2f(b);
  return isnan(x) ? b | 0x00400000U : f2u((float)(1.0 / (double)x));
}

static void check_recipf(uint32_t b) {
  ASSERT_EQ(f2u(fm_recipf(u2f(b))), ref_recipf_bits(b));
}

void test_fm_recipf(void) {
  ASSERT_EQ(f2u(fm_recipf(0.0f)), 0x7F800000U);
  ASSERT_EQ(f2u(fm_recipf(-0.0f)), 0xFF800000U);
  ASSERT_EQ(f2u(fm_recipf(INFINITY)), 0x00000000U);
  ASSERT_EQ(f2u(fm_recipf(-INFINITY)), 0x80000000U);
  ASSERT_EQ(f2u(fm_recipf(u2f(0xFF800001U))), 0xFFC00001U);
  ASSERT_EQ(f2u(fm_recipf(-4.0f)), f2u(-0.25f));
  ASSERT_EQ(f2u(fm_recipf(0x1p126f)), 0x00800000U);
  ASSERT_EQ(f2u(fm_recipf(0x1p127f)), 0x00400000U); /* subnormal */
  ASSERT_EQ(f2u(fm_recipf(0x1p-126f)), f2u(0x1p126f));
  ASSERT_EQ(f2u(fm_recipf(0x1p-128f)), 0x7F800000U); /* 2^128 overflows */
  check_recipf(0x7F7FFFFFU);
  check_recipf(0x00000001U);
  check_recipf(0x00200000U); /* 2^-129: overflows */
  check_recipf(0x00200001U);
  check_recipf(0x00400001U); /* just below 2^128 */
  for (uint32_t b = 0U; b < 0x00800000U; b += 7U) { /* subnormal inputs */
    check_recipf(b);
    check_recipf(b | 0x80000000U);
  }
  for (uint32_t b = 0x7E800000U; b < 0x7F800000U; b += 7U) { /* subnormal out */
    check_recipf(b);
  }
  uint64_t st = 161U;
  ErrStats es;
  stats_init(&es, "fm_recipf, random bits", "ulp");
  for (int i = 0; i < 1000000; i++) {
    uint32_t b = test_rng_u32(&st);
    check_recipf(b);
    float x = u2f(b);
    if (!isnan(x) && fabsf(x) < 0x1p126f && fabsf(x) >= 0x1p-126f) {
      stats_add(&es, (double)x, ulp_err_f32(fm_recipf(x), 1.0 / (double)x));
    }
  }
  ASSERT_STATS_MAX(&es, 0.5);
}

__extension__ typedef unsigned __int128 u128;

/* sign of a - 2^s */
static int cmp_pow2(u128 a, int s) {
  if (s < 0) {
    return 1; /* a >= 1 here */
  }
  u128 p = (u128)1U << s;
  return a < p ? -1 : (a > p ? 1 : 0);
}

/* r (a normal float) is the nearest float to 1/sqrt(x), x > 0 finite: v =
 * 1/sqrt(x) > mid iff mid^2 x < 1, exact in 128 bits */
static bool rsqrt_nearest(uint32_t xb, uint32_t rb) {
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
    return false;
  }
  uint32_t m = (rb & 0x7FFFFFU) | 0x800000U;
  er -= 150;
  u128 kh = 2U * (u128)m + 1U; /* (2m + 1) 2^(er - 1) */
  u128 kl = m == 0x800000U ? 4U * (u128)m - 1U : 2U * (u128)m - 1U;
  int sl = m == 0x800000U ? er - 2 : er - 1;
  return cmp_pow2(kh * kh * mx, -(2 * (er - 1) + ex)) > 0 &&
         cmp_pow2(kl * kl * mx, -(2 * sl + ex)) < 0;
}

static void check_rsqrtf(uint32_t b) {
  uint32_t r = f2u(fm_rsqrtf(u2f(b)));
  ASSERT_TRUE(rsqrt_nearest(b, r));
  ASSERT_TRUE(!rsqrt_nearest(b, r + 1U) && !rsqrt_nearest(b, r - 1U));
}

void test_fm_rsqrtf(void) {
  ASSERT_EQ(f2u(fm_rsqrtf(0.0f)), 0x7F800000U);
  ASSERT_EQ(f2u(fm_rsqrtf(-0.0f)), 0xFF800000U);
  ASSERT_EQ(f2u(fm_rsqrtf(INFINITY)), 0x00000000U);
  ASSERT_EQ(f2u(fm_rsqrtf(-INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_rsqrtf(-2.0f)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_rsqrtf(u2f(0x7F800001U))), 0x7FC00001U);
  ASSERT_EQ(f2u(fm_rsqrtf(1.0f)), f2u(1.0f));
  ASSERT_EQ(f2u(fm_rsqrtf(4.0f)), f2u(0.5f));
  ASSERT_EQ(f2u(fm_rsqrtf(0x1p-148f)), f2u(0x1p74f));
  check_rsqrtf(0x00000001U);
  check_rsqrtf(0x007FFFFFU);
  check_rsqrtf(0x7F7FFFFFU);
  for (uint32_t b = 0U; b < 0x00800000U; b += 7U) {
    if (b != 0U) {
      check_rsqrtf(b);
    }
  }
  uint64_t st = 171U;
  ErrStats es;
  stats_init(&es, "fm_rsqrtf, random bits", "ulp");
  for (int i = 0; i < 1000000; i++) {
    uint32_t b = test_rng_u32(&st) >> 1;
    if (b == 0U || b >= 0x7F800000U) {
      continue;
    }
    check_rsqrtf(b);
    float x = u2f(b);
    stats_add(&es, (double)x, ulp_err_f32(fm_rsqrtf(x), 1.0 / sqrt((double)x)));
  }
  ASSERT_STATS_MAX(&es, 0.5001); /* the double reference rounds twice */
}

static void check_sincos(float x) {
  float s, c;
  fm_sincosf(x, &s, &c);
  ASSERT_EQ(f2u(s), f2u(fm_sinf(x)));
  ASSERT_EQ(f2u(c), f2u(fm_cosf(x)));
}

void test_fm_sinf_cosf(void) {
  ASSERT_EQ(f2u(fm_sinf(0.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_sinf(-0.0f)), 0x80000000U);
  ASSERT_EQ(f2u(fm_cosf(-0.0f)), f2u(1.0f));
  ASSERT_EQ(f2u(fm_sinf(INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_cosf(-INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_sinf(u2f(0xFF800123U))), 0xFFC00123U);
  ASSERT_EQ(f2u(fm_cosf(u2f(0x7F800001U))), 0x7FC00001U);
  check_sincos(INFINITY);
  check_sincos(u2f(0x7F800001U));
  /* |x| < 2^-12: sin x = x, cos x = 1, subnormals included */
  for (uint32_t b = 0U; b < 0x39800000U; b += 4099U) {
    ASSERT_EQ(f2u(fm_sinf(u2f(b))), b);
    ASSERT_EQ(f2u(fm_sinf(u2f(b | 0x80000000U))), b | 0x80000000U);
    ASSERT_EQ(f2u(fm_cosf(u2f(b))), f2u(1.0f));
    check_sincos(u2f(b));
  }
  /* the float closest to a quarter-turn multiple (2^-31 turns away), the
   * ends of the direct range */
  ErrStats es, ec;
  stats_init(&es, "fm_sinf, random bits + hard", "ulp");
  stats_init(&ec, "fm_cosf, random bits + hard", "ulp");
  /* and results that round up to a power of two (a carry once lost them) */
  float hard[] = {u2f(0x50A3E87FU), u2f(0x4466F861U), u2f(0x4502EEB3U),
                  u2f(0x7C9FE500U), 0x1.921fb6p0f, 0x1.921fb4p0f,
                  0x1.921fb6p1f, 0x1.921fb4p1f, u2f(0x7F7FFFFFU),
                  0x1.921fb4p-1f, 0x1.921fb6p-1f};
  for (unsigned i = 0U; i < sizeof hard / sizeof hard[0]; i++) {
    stats_add(&es, (double)hard[i],
              ulp_err_f32(fm_sinf(hard[i]), sin((double)hard[i])));
    stats_add(&ec, (double)hard[i],
              ulp_err_f32(fm_cosf(hard[i]), cos((double)hard[i])));
    check_sincos(hard[i]);
  }
  uint64_t st = 181U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    float x = (i & 1) ? u2f(u) : (float)(int32_t)u * 1.46291807926716e-9f;
    if (isnan(x) || isinf(x)) {
      continue;
    }
    stats_add(&es, (double)x, ulp_err_f32(fm_sinf(x), sin((double)x)));
    stats_add(&ec, (double)x, ulp_err_f32(fm_cosf(x), cos((double)x)));
    check_sincos(x);
  }
  ASSERT_STATS_MAX(&es, 0.515);
  ASSERT_STATS_MAX(&ec, 0.515);
}

static void check_atan2(float y, float x, ErrStats *es) {
  float r = fm_atan2f(y, x);
  double ref = atan2((double)y, (double)x);
  stats_add(es, (double)y, ulp_err_f32(r, ref));
}

void test_fm_atan2f(void) {
  const float pi = 3.14159265f;
  const float pi2 = 1.57079633f;
  const float pi4 = 0.785398163f;
  ASSERT_EQ(f2u(fm_atan2f(0.0f, 1.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_atan2f(-0.0f, 1.0f)), 0x80000000U);
  ASSERT_EQ(f2u(fm_atan2f(0.0f, -1.0f)), f2u(pi));
  ASSERT_EQ(f2u(fm_atan2f(-0.0f, -0.0f)), f2u(-pi));
  ASSERT_EQ(f2u(fm_atan2f(0.0f, 0.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_atan2f(-2.0f, 0.0f)), f2u(-pi2));
  ASSERT_EQ(f2u(fm_atan2f(INFINITY, INFINITY)), f2u(pi4));
  ASSERT_EQ(f2u(fm_atan2f(-INFINITY, -INFINITY)), f2u(-3.0f * pi4));
  ASSERT_EQ(f2u(fm_atan2f(INFINITY, -5.0f)), f2u(pi2));
  ASSERT_EQ(f2u(fm_atan2f(3.0f, -INFINITY)), f2u(pi));
  ASSERT_EQ(f2u(fm_atan2f(-3.0f, INFINITY)), 0x80000000U);
  ASSERT_EQ(f2u(fm_atan2f(u2f(0x7F800123U), 1.0f)), 0x7FC00123U);
  ASSERT_EQ(f2u(fm_atan2f(1.0f, u2f(0xFF800321U))), 0xFFC00321U);
  ASSERT_EQ(f2u(fm_atan2f(u2f(0x7FC00001U), u2f(0x7FC00002U))), 0x7FC00001U);
  ASSERT_EQ(f2u(fm_atan2f(1.0f, 1.0f)), f2u(pi4));
  ASSERT_EQ(f2u(fm_atan2f(1.0f, -1.0f)), f2u(3.0f * pi4));
  ASSERT_EQ(f2u(fm_atan2f(0x1p-141f, 0x1p10f)), 0x00000000U); /* underflows */
  ErrStats es;
  stats_init(&es, "fm_atan2f, random pairs", "ulp");
  check_atan2(0x1p-130f, 0x1.8p2f, &es); /* subnormal result */
  for (int k = -40; k <= 40; k++) { /* equal mantissas, every gap */
    check_atan2(ldexpf(1.25f, k), 1.25f, &es);
    check_atan2(1.75f, ldexpf(-1.75f, k), &es);
  }
  check_atan2(u2f(0x00000001U), u2f(0x00000003U), &es);
  check_atan2(u2f(0x7F7FFFFFU), u2f(0x00000001U), &es);
  uint64_t st = 191U;
  for (int i = 0; i < 1000000; i++) {
    uint32_t u = test_rng_u32(&st);
    uint32_t v = test_rng_u32(&st);
    if ((i & 1) != 0) { /* close magnitudes */
      v = (v & 0x80FFFFFFU) | ((u & 0x7F000000U) + 0x01000000U * (v >> 30));
    }
    float y = u2f(u);
    float x = u2f(v);
    if (isnan(y) || isnan(x) || isinf(y) || isinf(x)) {
      continue;
    }
    check_atan2(y, x, &es);
  }
  ASSERT_STATS_MAX(&es, 0.545);
}

void test_fm_asinf_acosf(void) {
  ASSERT_EQ(f2u(fm_asinf(-0.0f)), 0x80000000U);
  ASSERT_EQ(f2u(fm_asinf(u2f(0x00000005U))), 0x00000005U);
  ASSERT_EQ(f2u(fm_asinf(1.0f)), 0x3FC90FDBU);
  ASSERT_EQ(f2u(fm_asinf(-1.0f)), 0xBFC90FDBU);
  ASSERT_EQ(f2u(fm_acosf(1.0f)), 0x00000000U);
  ASSERT_EQ(f2u(fm_acosf(-1.0f)), 0x40490FDBU);
  ASSERT_EQ(f2u(fm_acosf(0.0f)), 0x3FC90FDBU);
  ASSERT_EQ(f2u(fm_asinf(1.0000001f)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_acosf(-INFINITY)), 0x7FC00000U);
  ASSERT_EQ(f2u(fm_asinf(u2f(0xFF800001U))), 0xFFC00001U);
  for (uint32_t b = 0x00800000U; b < 0x39800000U; b += 4099U) {
    ASSERT_EQ(f2u(fm_asinf(u2f(b))), b); /* |x| < 2^-12 */
  }
  ErrStats es;
  ErrStats ec;
  stats_init(&es, "fm_asinf, every 64th float", "ulp");
  stats_init(&ec, "fm_acosf, every 64th float", "ulp");
  for (uint32_t b = 0U; b < 0x3F800000U; b += 64U) {
    for (uint32_t sg = 0U; sg < 2U; sg++) {
      float x = u2f(b | sg << 31);
      stats_add(&es, (double)x, ulp_err_f32(fm_asinf(x), asin((double)x)));
      stats_add(&ec, (double)x, ulp_err_f32(fm_acosf(x), acos((double)x)));
    }
  }
  ASSERT_STATS_MAX(&es, 0.545);
  ASSERT_STATS_MAX(&ec, 0.545);
}
