/**
 * @file cases_exp.c
 * @brief Bench cases for fm_exp.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fm_exp.h"
#include <math.h>

/* ---- candidates ---- */

/* 17 squarings of the mantissa, one bit each, then rounded: no table */
BENCH_CALL static int32_t log2_bits(int32_t x) {
  if (x <= 0) {
    return INT32_MIN;
  }
  uint32_t lz = fx_clz((uint32_t)x);
  uint32_t m = (uint32_t)x << lz; /* Q31, in [1, 2) */
  uint32_t r = 0U;
  for (unsigned i = 0U; i < 17U; i++) {
    uint64_t sq = fx_umull(m, m);
    r <<= 1;
    if ((sq >> 63) != 0U) {
      r |= 1U;
      m = (uint32_t)(sq >> 32);
    } else {
      m = (uint32_t)(sq >> 31);
    }
  }
  return (15 - (int32_t)lz) * 65536 + (int32_t)((r + 1U) >> 1);
}

/* log2(1 + t) on [0, 1), near-minimax degree 8 in Q30: no table */
BENCH_CALL static int32_t log2_poly(int32_t x) {
  if (x <= 0) {
    return INT32_MIN;
  }
  uint32_t lz = fx_clz((uint32_t)x);
  int32_t t = (int32_t)(((uint32_t)x << lz) >> 1) - (1 << 30);
  int32_t p = -9759136;
  p = 54905164 + (fx_smull_hi(p, t) << 2);
  p = -145950395 + (fx_smull_hi(p, t) << 2);
  p = 258138505 + (fx_smull_hi(p, t) << 2);
  p = -372103675 + (fx_smull_hi(p, t) << 2);
  p = 513747045 + (fx_smull_hi(p, t) << 2);
  p = -774309808 + (fx_smull_hi(p, t) << 2);
  p = 1549074032 + (fx_smull_hi(p, t) << 2);
  p = 45 + (fx_smull_hi(p, t) << 2);
  return (15 - (int32_t)lz) * 65536 + ((p + (1 << 13)) >> 14);
}

/* 2^f on [0, 1), near-minimax degree 7 in Q32 (positive terms): no table */
BENCH_CALL static int32_t exp2_poly(int32_t x) {
  int32_t i = x >> 16;
  if (i >= 15) {
    return INT32_MAX;
  }
  if (i < -17) {
    return 0;
  }
  uint32_t f = (uint32_t)x << 16;
  uint32_t p = 93032U;
  p = 614019U + fx_umull_hi(p, f);
  p = 5768246U + fx_umull_hi(p, f);
  p = 41289694U + fx_umull_hi(p, f);
  p = 238393458U + fx_umull_hi(p, f);
  p = 1031764343U + fx_umull_hi(p, f);
  p = 2977044503U + fx_umull_hi(p, f);
  uint64_t v = (1ULL << 32) + fx_umull_hi(p, f);
  return (int32_t)((v + (1ULL << (15 - i))) >> (16 - i));
}

/* ---- inputs ---- */

/* positive, every magnitude */
static inline int32_t in_pos(uint32_t u) {
  return ((int32_t)(u >> 1) >> (u & 15U)) | 1;
}
static inline int32_t in_exp(uint32_t u) { return (int32_t)u >> 12; } /* +-8 */
static inline int32_t in_pow_y(uint32_t u) {
  return (int32_t)u >> (12U + (u & 3U)); /* +-1 .. +-8 */
}

#define VIA_FLOAT(fn, x) fm_f32_to_q16(fn(fm_q16_to_f32(x)))

BENCH_UNARY_COLD(e_log2, int32_t, in_pos, int32_t, fm_log2_q16(x))
BENCH_UNARY(e_log2_bits, int32_t, in_pos, int32_t, log2_bits(x))
BENCH_UNARY_COLD(e_log2_poly, int32_t, in_pos, int32_t, log2_poly(x))
BENCH_UNARY(e_log2_sf, int32_t, in_pos, int32_t, VIA_FLOAT(log2f, x))
BENCH_UNARY(e_ln, int32_t, in_pos, int32_t, fm_ln_q16(x))
BENCH_UNARY(e_ln_sf, int32_t, in_pos, int32_t, VIA_FLOAT(logf, x))
BENCH_UNARY(e_log10, int32_t, in_pos, int32_t, fm_log10_q16(x))
BENCH_UNARY(e_db, int32_t, in_pos, int32_t, fm_db_q16(x))
BENCH_UNARY_COLD(e_exp2, int32_t, in_exp, int32_t, fm_exp2_q16(x))
BENCH_UNARY_COLD(e_exp2_poly, int32_t, in_exp, int32_t, exp2_poly(x))
BENCH_UNARY(e_exp2_sf, int32_t, in_exp, int32_t, VIA_FLOAT(exp2f, x))
BENCH_UNARY(e_exp, int32_t, in_exp, int32_t, fm_exp_q16(x))
BENCH_UNARY(e_exp_sf, int32_t, in_exp, int32_t, VIA_FLOAT(expf, x))
BENCH_BINARY(e_pow, int32_t, in_pos, int32_t, in_pow_y, int32_t,
             fm_pow_q16(a, b))
BENCH_BINARY(e_pow_sf, int32_t, in_pos, int32_t, in_pow_y, int32_t,
             fm_f32_to_q16(powf(fm_q16_to_f32(a), fm_q16_to_f32(b))))

/* libm rows aren't golden (glibc on the host) */
static const BenchCase cases[] = {
    {"fm_log2_q16", "flexmath", e_log2, true},
    {"log2 q16, bit by bit", "candidate", e_log2_bits, true},
    {"log2 q16, degree-8 polynomial", "candidate", e_log2_poly, true},
    {"log2f via float", "libm", e_log2_sf, false},
    {"fm_ln_q16", "flexmath", e_ln, true},
    {"logf via float", "libm", e_ln_sf, false},
    {"fm_log10_q16", "flexmath", e_log10, true},
    {"fm_db_q16", "flexmath", e_db, true},
    {"fm_exp2_q16", "flexmath", e_exp2, true},
    {"exp2 q16, degree-7 polynomial", "candidate", e_exp2_poly, true},
    {"exp2f via float", "libm", e_exp2_sf, false},
    {"fm_exp_q16", "flexmath", e_exp, true},
    {"expf via float", "libm", e_exp_sf, false},
    {"fm_pow_q16", "flexmath", e_pow, true},
    {"powf via float", "libm", e_pow_sf, false},
};

const BenchGroup bench_group_exp = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_log2_q16", "flexmath", e_log2_cold, true},
    {"log2 q16, degree-8 polynomial", "candidate", e_log2_poly_cold, true},
    {"fm_exp2_q16", "flexmath", e_exp2_cold, true},
    {"exp2 q16, degree-7 polynomial", "candidate", e_exp2_poly_cold, true},
};

const BenchGroup bench_group_exp_cold = {cold, BENCH_COUNT(cold)};
