/**
 * @file cases_cmsis.c
 * @brief Baselines from CMSIS-DSP
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "arm_math.h"
#include "bench.h"

/* arm_sin_q15 maps 0 .. 0x7FFF onto [0, 2pi) */
static inline q15_t in_q15_turn(uint32_t u) { return (q15_t)(u >> 17); }
/* arm_sin_q31 maps 0 .. 0x7FFFFFFF onto [0, 2pi) */
static inline q31_t in_q31_turn(uint32_t u) { return (q31_t)(u >> 1); }
/* +-pi */
static inline float in_angle(uint32_t u) {
  return (float)(int32_t)u * 1.46291807926716e-9f;
}
/* +-1 */
static inline float in_unit(uint32_t u) {
  return (float)(int32_t)u * 4.65661287e-10f;
}

/* the inputs of the fm_sqrt cases */
static inline q15_t in_q15_pos(uint32_t u) { return (q15_t)(u >> 17); }
static inline q31_t in_q31_mag(uint32_t u) {
  return (q31_t)(u >> 1) >> (u & 31U);
}

static inline q15_t sqrt_q15(q15_t x) {
  q15_t r;
  (void)arm_sqrt_q15(x, &r);
  return r;
}
static inline q31_t sqrt_q31(q31_t x) {
  q31_t r;
  (void)arm_sqrt_q31(x, &r);
  return r;
}

/* the inputs of the fm_div cases */
static inline q15_t in_q15_num(uint32_t u) {
  return (q15_t)((q15_t)(u >> 16) >> (1U + (u & 7U)));
}
static inline q15_t in_q15_den(uint32_t u) { return (q15_t)((u >> 16) | 1U); }
static inline q31_t in_q31_num(uint32_t u) {
  return (q31_t)u >> (1U + (u & 15U));
}
static inline q31_t in_q31_den(uint32_t u) {
  return (q31_t)(u | 1U) >> (u & 3U);
}

static inline q15_t divide_q15(q15_t a, q15_t b) {
  q15_t q;
  int16_t s;
  (void)arm_divide_q15(a, b, &q, &s);
  return q;
}
static inline q31_t divide_q31(q31_t a, q31_t b) {
  q31_t q;
  int16_t s;
  (void)arm_divide_q31(a, b, &q, &s);
  return q;
}

static inline q15_t atan2_q15(q15_t y, q15_t x) {
  q15_t r;
  (void)arm_atan2_q15(y, x, &r);
  return r;
}
static inline q31_t atan2_q31(q31_t y, q31_t x) {
  q31_t r;
  (void)arm_atan2_q31(y, x, &r);
  return r;
}
static inline float atan2_f32(float y, float x) {
  float r;
  (void)arm_atan2_f32(y, x, &r);
  return r;
}
static inline q31_t vlog_q31(q31_t x) {
  q31_t r;
  arm_vlog_q31(&x, &r, 1U);
  return r;
}
static inline q15_t vlog_q15(q15_t x) {
  q15_t r;
  arm_vlog_q15(&x, &r, 1U);
  return r;
}
/* positive, every magnitude (the fm_exp log inputs) */
static inline q31_t in_q31_pos(uint32_t u) {
  return ((q31_t)(u >> 1) >> (u & 15U)) | 1;
}
static inline q15_t in_q15_pos1(uint32_t u) { return (q15_t)((u >> 17) | 1U); }

/* the inputs of the fm_atan cases */
static inline q15_t in_q15_any(uint32_t u) { return (q15_t)(u >> 16); }
static inline q31_t in_q31_any(uint32_t u) { return (q31_t)u >> (u & 15U); }

BENCH_UNARY_COLD(b_arm_sin_q15, q15_t, in_q15_turn, q15_t, arm_sin_q15(x))
BENCH_UNARY(b_arm_sin_f32, float, in_angle, float, arm_sin_f32(x))
BENCH_UNARY(b_arm_cos_f32, float, in_angle, float, arm_cos_f32(x))
BENCH_BINARY(b_arm_atan2_f32, float, in_unit, float, in_unit, float,
             atan2_f32(a, b))
BENCH_UNARY_COLD(b_arm_sin_q31, q31_t, in_q31_turn, q31_t, arm_sin_q31(x))
BENCH_UNARY_COLD(b_arm_sqrt_q15, q15_t, in_q15_pos, q15_t, sqrt_q15(x))
BENCH_UNARY(b_arm_sqrt_q31, q31_t, in_q31_mag, q31_t, sqrt_q31(x))
BENCH_BINARY_COLD(b_arm_atan2_q15, q15_t, in_q15_any, q15_t, in_q15_any, q15_t,
                  atan2_q15(a, b))
BENCH_BINARY_COLD(b_arm_atan2_q31, q31_t, in_q31_any, q31_t, in_q31_any, q31_t,
                  atan2_q31(a, b))
BENCH_UNARY(b_arm_vlog_q31, q31_t, in_q31_pos, q31_t, vlog_q31(x))
BENCH_UNARY(b_arm_vlog_q15, q15_t, in_q15_pos1, q15_t, vlog_q15(x))
BENCH_BINARY(b_arm_divide_q15, q15_t, in_q15_num, q15_t, in_q15_den, q15_t,
             divide_q15(a, b))
BENCH_BINARY(b_arm_divide_q31, q31_t, in_q31_num, q31_t, in_q31_den, q31_t,
             divide_q31(a, b))

/* no golden hash: CMSIS isn't built on the host */
static const BenchCase cases[] = {
    {"arm_sin_q15", "CMSIS-DSP", b_arm_sin_q15, false},
    {"arm_sin_f32", "CMSIS-DSP", b_arm_sin_f32, false},
    {"arm_cos_f32", "CMSIS-DSP", b_arm_cos_f32, false},
    {"arm_atan2_f32", "CMSIS-DSP", b_arm_atan2_f32, false},
    {"arm_sin_q31", "CMSIS-DSP", b_arm_sin_q31, false},
    {"arm_sqrt_q15", "CMSIS-DSP", b_arm_sqrt_q15, false},
    {"arm_sqrt_q31", "CMSIS-DSP", b_arm_sqrt_q31, false},
    {"arm_divide_q15", "CMSIS-DSP", b_arm_divide_q15, false},
    {"arm_divide_q31", "CMSIS-DSP", b_arm_divide_q31, false},
    {"arm_atan2_q15", "CMSIS-DSP", b_arm_atan2_q15, false},
    {"arm_atan2_q31", "CMSIS-DSP", b_arm_atan2_q31, false},
    {"arm_vlog_q31, one value (ln, q5.26 out)", "CMSIS-DSP", b_arm_vlog_q31,
     false},
    {"arm_vlog_q15, one value (ln, q4.11 out)", "CMSIS-DSP", b_arm_vlog_q15,
     false},
};

const BenchGroup bench_group_cmsis = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"arm_sin_q15", "CMSIS-DSP", b_arm_sin_q15_cold, false},
    {"arm_sin_q31", "CMSIS-DSP", b_arm_sin_q31_cold, false},
    {"arm_sqrt_q15", "CMSIS-DSP", b_arm_sqrt_q15_cold, false},
    {"arm_atan2_q15", "CMSIS-DSP", b_arm_atan2_q15_cold, false},
    {"arm_atan2_q31", "CMSIS-DSP", b_arm_atan2_q31_cold, false},
};

const BenchGroup bench_group_cmsis_cold = {cold, BENCH_COUNT(cold)};
