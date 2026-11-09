/**
 * @file cases_fixed.c
 * @brief Bench cases for fm_fixed.h, with other saturation forms and the
 *        usual soft-float conversions
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_fixed.h"

/* ---- candidates ---- */

FX_INLINE int32_t add_q31_clamp(int32_t a, int32_t b) {
  return fm_sat_q31((int64_t)a + b);
}

/* overflow iff both inputs differ in sign from the sum */
FX_INLINE int32_t add_q31_xor(int32_t a, int32_t b) {
  uint32_t s = (uint32_t)a + (uint32_t)b;
  uint32_t sat = ((uint32_t)a >> 31) + 0x7FFFFFFFU;
  return (int32_t)((((uint32_t)a ^ s) & ((uint32_t)b ^ s)) >> 31 ? sat : s);
}

FX_INLINE fm_q16 mul_q16_clamp(fm_q16 a, fm_q16 b) {
  return fm_sat_q31(((int64_t)a * b + 0x8000) >> 16);
}

/* ---- inputs ---- */

/* every magnitude, so a share of sums and products saturate */
static inline int32_t in_q31(uint32_t u) { return (int32_t)u >> (u & 7U); }
static inline int16_t in_q15(uint32_t u) { return (int16_t)(u >> 16); }
static inline float in_f32(uint32_t u) {
  return (float)(int32_t)u * (1.1f / 2147483648.0f); /* about +-1.1 */
}

BENCH_BINARY(x_add31, int32_t, in_q31, int32_t, in_q31, int32_t,
             fm_add_q31(a, b))
BENCH_BINARY(x_add31_clamp, int32_t, in_q31, int32_t, in_q31, int32_t,
             add_q31_clamp(a, b))
BENCH_BINARY(x_add31_xor, int32_t, in_q31, int32_t, in_q31, int32_t,
             add_q31_xor(a, b))
BENCH_BINARY(x_mul15, int16_t, in_q15, int16_t, in_q15, int16_t,
             fm_mul_q15(a, b))
BENCH_BINARY(x_mul31, int32_t, in_q31, int32_t, in_q31, int32_t,
             fm_mul_q31(a, b))
BENCH_BINARY(x_mul16, int32_t, in_q31, int32_t, in_q31, int32_t,
             fm_mul_q16(a, b))
BENCH_BINARY(x_mul16_clamp, int32_t, in_q31, int32_t, in_q31, int32_t,
             mul_q16_clamp(a, b))
BENCH_UNARY(x_f2q15, float, in_f32, int16_t, fm_f32_to_q15(x))
BENCH_UNARY(x_f2q15_sf, float, in_f32, int16_t,
            fm_sat_q15((int32_t)(x * 32768.0f)))
BENCH_UNARY(x_q152f, int16_t, in_q15, float, fm_q15_to_f32(x))
BENCH_UNARY(x_q152f_sf, int16_t, in_q15, float, (float)x * (1.0f / 32768.0f))
BENCH_UNARY(x_f2q31, float, in_f32, int32_t, fm_f32_to_q31(x))
BENCH_UNARY(x_f2q31_sf, float, in_f32, int32_t,
            fm_sat_q31((int64_t)(x * 2147483648.0f)))
BENCH_UNARY(x_q312f, int32_t, in_q31, float, fm_q31_to_f32(x))
BENCH_UNARY(x_q312f_sf, int32_t, in_q31, float,
            (float)x * (1.0f / 2147483648.0f))

/* soft-float rows truncate where flexmath rounds, so their hashes differ */
static const BenchCase cases[] = {
    {"fm_add_q31 (inline)", "flexmath", x_add31, true},
    {"add q31, 64-bit clamp", "candidate", x_add31_clamp, true},
    {"add q31, sign xor + IT", "candidate", x_add31_xor, true},
    {"fm_mul_q15 (inline)", "flexmath", x_mul15, true},
    {"fm_mul_q31 (inline)", "flexmath", x_mul31, true},
    {"fm_mul_q16 (inline)", "flexmath", x_mul16, true},
    {"mul q16, 64-bit clamp", "candidate", x_mul16_clamp, true},
    {"fm_f32_to_q15", "flexmath", x_f2q15, true},
    {"(q15)(x * 32768.0f), saturated", "soft-float", x_f2q15_sf, true},
    {"fm_q15_to_f32", "flexmath", x_q152f, true},
    {"(float)q * 2^-15", "soft-float", x_q152f_sf, true},
    {"fm_f32_to_q31", "flexmath", x_f2q31, true},
    {"(q31)(x * 2^31), saturated", "soft-float", x_f2q31_sf, true},
    {"fm_q31_to_f32", "flexmath", x_q312f, true},
    {"(float)q * 2^-31", "soft-float", x_q312f_sf, true},
};

const BenchGroup bench_group_fixed = {cases, BENCH_COUNT(cases)};
