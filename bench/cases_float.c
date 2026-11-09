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

/* ---- inputs ---- */

/* positive normals 2^-31 .. 2^33, uniform in the exponent */
static inline float in_posf(uint32_t u) { return u2f(0x30000000U + (u >> 3)); }
static inline float in_subf(uint32_t u) { return u2f((u >> 9) | 1U); }

BENCH_UNARY_COLD(f_sqrt, float, in_posf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_sub, float, in_subf, float, fm_sqrtf(x))
BENCH_UNARY(f_sqrt_fix_all, float, in_posf, float, sqrtf_fix_all(x))
BENCH_UNARY(f_sqrt_rsqrt, float, in_posf, float, sqrtf_rsqrt(x))
BENCH_UNARY_COLD(f_sqrt_libm, float, in_posf, float, sqrtf(x))

static const BenchCase cases[] = {
    {"fm_sqrtf", "flexmath", f_sqrt, true},
    {"fm_sqrtf, subnormal inputs", "flexmath", f_sqrt_sub, true},
    {"sqrtf, s^2 test on every input", "candidate", f_sqrt_fix_all, true},
    {"sqrtf by divide-free 1/sqrt Newton", "candidate", f_sqrt_rsqrt, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm, false},
};

const BenchGroup bench_group_float = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_sqrtf", "flexmath", f_sqrt_cold, true},
    {"sqrtf, same inputs", "libm", f_sqrt_libm_cold, false},
};

const BenchGroup bench_group_float_cold = {cold, BENCH_COUNT(cold)};
