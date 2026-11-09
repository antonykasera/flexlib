/**
 * @file cases_float.c
 * @brief Bench cases for fm_float.h, with candidates and newlib's libm on the
 *        same inputs
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_float.h"
#include "fm_internal.h"
#include <math.h>

static inline uint32_t f2u(float x) {
  uint32_t b;
  memcpy(&b, &x, sizeof b);
  return b;
}

static inline float u2f(uint32_t b) {
  float x;
  memcpy(&x, &b, sizeof x);
  return x;
}

/* ---- candidates (specials go to fm_sqrtf; the inputs have none) ---- */

/* fm_sqrtf with the s^2 test on every input */
BENCH_CALL static float sqrtf_fix_all(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_sqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t h = sqrt_norm32(hi);
  uint32_t s = (h << 16) + ((hi - h * h) << 15) / h;
  if (fx_umull(s, s) > (uint64_t)hi << 32) {
    s--;
  }
  return u2f((((e + 125U) >> 1) << 23) + ((s + 128U) >> 8));
}

/* divide-free: 1/sqrt seed, two UMULL Newton steps, sqrt = hi / sqrt(hi), then
 * the nearest by exact squares against 4N */
BENCH_CALL static float sqrtf_rsqrt(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_sqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  for (int i = 0; i < 2; i++) {
    uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);
    uint32_t p = (uint32_t)(fx_umull(hi, y2) >> 32);
    y = (uint32_t)(fx_umull(y, (3U << 28) - p) >> 29);
  }
  uint32_t r = ((uint32_t)(fx_umull(hi, y) >> 30) + 128U) >> 8;
  uint64_t n4 = (uint64_t)hi << 18;
  uint64_t up = 2U * (uint64_t)r + 1U;
  if (up * up < n4) {
    r++;
  } else {
    uint64_t dn = 2U * (uint64_t)r - 1U;
    if (dn * dn > n4) {
      r--;
    }
  }
  return u2f((((e + 125U) >> 1) << 23) + r);
}

/* recipf: floor(2^48 / m) by divlu (two UDIV digits) */
BENCH_CALL static float recipf_divlu(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) {
    return fm_recipf(x);
  }
  uint32_t rem;
  uint32_t q = udiv_norm(1ULL << 40, (b << 8) | 0x80000000U, &rem);
  return u2f((b & 0x80000000U) + ((252U - (a >> 24)) << 23) + ((q + 1U) >> 1));
}

/* recipf without a divide: fm_log2_rcp seed (2^-7), two Newton steps in Q31,
 * then the floor by the remainder */
BENCH_CALL static float recipf_newton(float x) {
  uint32_t b = f2u(x);
  uint32_t a = b << 1;
  if (a - 0x01000000U >= 0xFC000000U) {
    return fm_recipf(x);
  }
  uint32_t m = (b & 0x7FFFFFU) | 0x800000U;
  uint32_t y = fm_log2_rcp[(m >> 17) & 63U] >> 1; /* 2^54 / m */
  for (int i = 0; i < 2; i++) {
    int32_t d = (int32_t)((int64_t)((1ULL << 54) - fx_umull(m, y)) >> 22);
    y += (uint32_t)(fx_smull((int32_t)y, d) >> 32);
  }
  uint32_t q = y >> 6;
  int32_t r = (int32_t)(0U - q * m);
  if (r < 0) {
    q--;
  } else if ((uint32_t)r >= m) {
    q++;
  }
  return u2f((b & 0x80000000U) + ((252U - (a >> 24)) << 23) + ((q + 1U) >> 1));
}

/* rsqrtf: a third-order first step (y within (-1, 2.5)), so only 3 of 128
 * guard values need the exact test, (2R - 1)^2 hi against 2^80 */
static inline int32_t rsqrt_resid(uint32_t hi, uint32_t y) {
  uint32_t y2 = (uint32_t)(fx_umull(y, y) >> 32);
  return (int32_t)((1U << 30) - (uint32_t)(fx_umull(hi, y2) >> 30));
}

BENCH_CALL static float rsqrtf_third(float x) {
  uint32_t b = f2u(x);
  if (b - 0x00800000U >= 0x7F000000U) {
    return fm_rsqrtf(x);
  }
  uint32_t e = b >> 23;
  uint32_t hi = ((b << 8) | 0x80000000U) >> (e & 1U);
  uint32_t y = (uint32_t)fm_rsqrt_seed[(hi >> 25) - 32U] << 16;
  int32_t d = rsqrt_resid(hi, y);
  int32_t c = (d >> 1) + (int32_t)((3 * (fx_smull(d, d) >> 30)) >> 3);
  y += (uint32_t)(fx_smull((int32_t)(y >> 1), c) >> 29);
  d = rsqrt_resid(hi, y);
  y += (uint32_t)(fx_smull((int32_t)(y >> 1), d) >> 30);
  uint32_t r = (y + 64U) >> 7;
  if (((y - 64U) & 127U) < 3U) {
    uint64_t a = fx_umull(2U * r - 1U, 2U * r - 1U);
    uint64_t p = fx_umull((uint32_t)a, hi) +
                 ((uint64_t)((uint32_t)(a >> 32) * hi) << 32);
    r -= (int64_t)p > 0 ? 1U : 0U;
  }
  return u2f((((378U - e) >> 1) << 23) + r);
}

/* ---- inputs ---- */

/* positive normals 2^-31 .. 2^33, uniform in the exponent */
static inline float in_posf(uint32_t u) { return u2f(0x30000000U + (u >> 3)); }
static inline float in_subf(uint32_t u) { return u2f((u >> 9) | 1U); }
static inline float in_sgnf(uint32_t u) { return u2f((u << 31) | f2u(in_posf(u))); }

BENCH_UNARY_COLD(f_sqrt, float, in_posf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_sub, float, in_subf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_fix_all, float, in_posf, float, sqrtf_fix_all(x))
BENCH_UNARY(f_sqrt_rsqrt, float, in_posf, float, sqrtf_rsqrt(x))
BENCH_UNARY_COLD(f_sqrt_libm, float, in_posf, float, sqrtf(x))
BENCH_UNARY_COLD(f_recip, float, in_sgnf, float, fm_recipf(x))
BENCH_UNARY(f_recip_sub, float, in_subf, float, fm_recipf(x))
BENCH_UNARY(f_recip_divlu, float, in_sgnf, float, recipf_divlu(x))
BENCH_UNARY(f_recip_newton, float, in_sgnf, float, recipf_newton(x))
BENCH_UNARY_COLD(f_recip_libgcc, float, in_sgnf, float, 1.0f / x)
BENCH_UNARY_COLD(f_rsqrt, float, in_posf, float, fm_rsqrtf(x))
BENCH_UNARY(f_rsqrt_sub, float, in_subf, float, fm_rsqrtf(x))
BENCH_UNARY(f_rsqrt_third, float, in_posf, float, rsqrtf_third(x))
BENCH_UNARY(f_rsqrt_composed, float, in_posf, float, fm_recipf(fm_sqrtf(x)))
BENCH_UNARY_COLD(f_rsqrt_libm, float, in_posf, float, 1.0f / sqrtf(x))

static const BenchCase cases[] = {
    {"fm_sqrtf", "flexmath", f_sqrt, true},
    {"fm_sqrtf, subnormal inputs", "flexmath", f_sqrt_sub, true},
    {"sqrtf, s^2 test on every input", "candidate", f_sqrt_fix_all, true},
    {"sqrtf by divide-free 1/sqrt Newton", "candidate", f_sqrt_rsqrt, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm, false},
    {"fm_recipf", "flexmath", f_recip, true},
    {"fm_recipf, subnormal inputs", "flexmath", f_recip_sub, true},
    {"recipf by divlu (two UDIV digits)", "candidate", f_recip_divlu, true},
    {"recipf by Newton from a table seed", "candidate", f_recip_newton, true},
    {"1.0f / x", "libgcc", f_recip_libgcc, false},
    {"fm_rsqrtf", "flexmath", f_rsqrt, true},
    {"fm_rsqrtf, subnormal inputs", "flexmath", f_rsqrt_sub, true},
    {"rsqrtf, third-order step, 3/128 window", "candidate", f_rsqrt_third,
     true},
    {"fm_recipf(fm_sqrtf(x)) (two roundings)", "candidate", f_rsqrt_composed,
     true},
    {"1.0f / sqrtf(x)", "libm", f_rsqrt_libm, false},
};

const BenchGroup bench_group_float = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_sqrtf", "flexmath", f_sqrt_cold, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm_cold, false},
    {"fm_recipf", "flexmath", f_recip_cold, true},
    {"1.0f / x", "libgcc", f_recip_libgcc_cold, false},
    {"fm_rsqrtf", "flexmath", f_rsqrt_cold, true},
    {"1.0f / sqrtf(x)", "libm", f_rsqrt_libm_cold, false},
};

const BenchGroup bench_group_float_cold = {cold, BENCH_COUNT(cold)};
