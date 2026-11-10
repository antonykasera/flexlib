/**
 * @file cases_sqrt.c
 * @brief Bench cases for fm_sqrt.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_sqrt.h"
#include <math.h>

/* ---- candidates ---- */

/* digit by digit: no table, no divide */
BENCH_CALL static uint32_t isqrt32_bits(uint32_t x) {
  uint32_t r = 0U;
  uint32_t b = 1U << 30;
  while (b > x) {
    b >>= 2;
  }
  for (; b != 0U; b >>= 2) {
    if (x >= r + b) {
      x -= r + b;
      r = (r >> 1) + b;
    } else {
      r >>= 1;
    }
  }
  return r;
}

/* Newton with UDIV from a CLZ seed (2^ceil(bits/2) >= sqrt), until it stops falling */
BENCH_CALL static uint32_t isqrt32_newton(uint32_t x) {
  if (x == 0U) {
    return 0U;
  }
  uint32_t s = 1U << ((33U - fx_clz(x)) >> 1);
  for (;;) {
    uint32_t t = (s + x / s) >> 1;
    if (t >= s) {
      return s;
    }
    s = t;
  }
}

BENCH_CALL static uint32_t isqrt64_bits(uint64_t x) {
  uint64_t r = 0U;
  uint64_t b = 1ULL << 62;
  while (b > x) {
    b >>= 2;
  }
  for (; b != 0U; b >>= 2) {
    if (x >= r + b) {
      x -= r + b;
      r = (r >> 1) + b;
    } else {
      r >>= 1;
    }
  }
  return (uint32_t)r;
}

/* ---- inputs: every magnitude ---- */

static inline uint32_t in_u32(uint32_t u) { return u >> (u & 31U); }
static inline uint64_t in_u64(uint32_t u) {
  return ((uint64_t)u << 32 | (u * 2654435761U)) >> (u & 63U);
}
static inline int16_t in_q15(uint32_t u) { return (int16_t)(u >> 17); }
static inline int32_t in_q31(uint32_t u) {
  return (int32_t)(u >> 1) >> (u & 31U);
}

BENCH_UNARY_COLD(s_i32, uint32_t, in_u32, uint32_t, fm_isqrt32(x))
BENCH_UNARY_COLD(s_i32_bits, uint32_t, in_u32, uint32_t, isqrt32_bits(x))
BENCH_UNARY(s_i32_newton, uint32_t, in_u32, uint32_t, isqrt32_newton(x))
BENCH_UNARY(s_i32_sf, uint32_t, in_u32, uint32_t, (uint32_t)sqrtf((float)x))
BENCH_UNARY(s_i64, uint64_t, in_u64, uint32_t, fm_isqrt64(x))
BENCH_UNARY(s_i64_bits, uint64_t, in_u64, uint32_t, isqrt64_bits(x))
BENCH_UNARY_COLD(s_q15, int16_t, in_q15, int16_t, fm_sqrt_q15(x))
BENCH_UNARY(s_q31, int32_t, in_q31, int32_t, fm_sqrt_q31(x))
BENCH_UNARY(s_q16, int32_t, in_q31, int32_t, fm_sqrt_q16(x))
BENCH_UNARY(s_q16_sf, int32_t, in_q31, int32_t,
            (int32_t)(sqrtf((float)x * (1.0f / 65536.0f)) * 65536.0f))
BENCH_UNARY_COLD(s_rq16, int32_t, in_q31, int32_t, fm_rsqrt_q16(x))
BENCH_UNARY(s_rq16_sf, int32_t, in_q31, int32_t,
            (int32_t)(65536.0f / sqrtf((float)x * (1.0f / 65536.0f))))

/* soft-float rows: inexact, and sqrtf comes from newlib (not golden) */
static const BenchCase cases[] = {
    {"fm_isqrt32", "flexmath", s_i32, true},
    {"isqrt32 digit by digit", "candidate", s_i32_bits, true},
    {"isqrt32 Newton from a CLZ seed", "candidate", s_i32_newton, true},
    {"(uint32_t)sqrtf((float)x)", "soft-float", s_i32_sf, false},
    {"fm_isqrt64", "flexmath", s_i64, true},
    {"isqrt64 digit by digit", "candidate", s_i64_bits, true},
    {"fm_sqrt_q15", "flexmath", s_q15, true},
    {"fm_sqrt_q31", "flexmath", s_q31, true},
    {"fm_sqrt_q16", "flexmath", s_q16, true},
    {"sqrtf on Q16.16 via float", "soft-float", s_q16_sf, false},
    {"fm_rsqrt_q16", "flexmath", s_rq16, true},
    {"1/sqrtf on Q16.16 via float", "soft-float", s_rq16_sf, false},
};

const BenchGroup bench_group_sqrt = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_isqrt32", "flexmath", s_i32_cold, true},
    {"isqrt32 digit by digit", "candidate", s_i32_bits_cold, true},
    {"fm_sqrt_q15", "flexmath", s_q15_cold, true},
    {"fm_rsqrt_q16", "flexmath", s_rq16_cold, true},
};

const BenchGroup bench_group_sqrt_cold = {cold, BENCH_COUNT(cold)};
