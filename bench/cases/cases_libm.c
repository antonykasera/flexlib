/**
 * @file cases_libm.c
 * @brief Baselines: newlib libm and the libgcc soft-float helpers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include <math.h>

/* +-pi */
static inline float in_angle(uint32_t u) {
  return (float)(int32_t)u * 1.46291807926716e-9f;
}
/* +-1 */
static inline float in_unit(uint32_t u) {
  return (float)(int32_t)u * 4.65661287e-10f;
}
/* 0 .. 1024 */
static inline float in_pos(uint32_t u) {
  return (float)(u >> 8) * (1.0f / 16384.0f);
}
/* 1 .. 2, so a divide by it never overflows */
static inline float in_one_two(uint32_t u) {
  return 1.0f + (float)(u >> 8) * (1.0f / 16777216.0f);
}

BENCH_UNARY_COLD(b_sinf, float, in_angle, float, sinf(x))
BENCH_UNARY(b_cosf, float, in_angle, float, cosf(x))
BENCH_UNARY(b_sqrtf, float, in_pos, float, sqrtf(x))
BENCH_BINARY(b_atan2f, float, in_unit, float, in_unit, float, atan2f(a, b))
BENCH_UNARY(b_asinf, float, in_unit, float, asinf(x))
BENCH_UNARY(b_acosf, float, in_unit, float, acosf(x))
BENCH_BINARY_COLD(b_fmul, float, in_unit, float, in_unit, float, a * b)
BENCH_BINARY(b_fdiv, float, in_unit, float, in_one_two, float, a / b)

static const BenchCase cases[] = {
    {"sinf", "libm", b_sinf, false},
    {"cosf", "libm", b_cosf, false},
    {"sqrtf", "libm", b_sqrtf, false},
    {"atan2f", "libm", b_atan2f, false},
    {"asinf", "libm", b_asinf, false},
    {"acosf", "libm", b_acosf, false},
    {"float multiply", "libgcc", b_fmul, false},
    {"float divide", "libgcc", b_fdiv, false},
};

const BenchGroup bench_group_libm = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"sinf", "libm", b_sinf_cold, false},
    {"float multiply", "libgcc", b_fmul_cold, false},
};

const BenchGroup bench_group_libm_cold = {cold, BENCH_COUNT(cold)};
