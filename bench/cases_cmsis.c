/**
 * @file cases_cmsis.c
 * @brief Baselines from CMSIS-DSP (bench/cmsis-dsp, generic C paths on the M3)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "arm_math.h"
#include "bench.h"

/* arm_sin_q15 maps 0 .. 0x7FFF onto [0, 2pi) */
static inline q15_t in_q15_turn(uint32_t u) { return (q15_t)(u >> 17); }
/* +-pi */
static inline float in_angle(uint32_t u) {
  return (float)(int32_t)u * 1.46291807926716e-9f;
}

BENCH_UNARY(b_arm_sin_q15, q15_t, in_q15_turn, q15_t, arm_sin_q15(x))
BENCH_UNARY(b_arm_sin_f32, float, in_angle, float, arm_sin_f32(x))

/* no golden hash: CMSIS isn't built on the host */
static const BenchCase cases[] = {
    {"arm_sin_q15", "CMSIS-DSP", b_arm_sin_q15, false},
    {"arm_sin_f32", "CMSIS-DSP", b_arm_sin_f32, false},
};

const BenchGroup bench_group_cmsis = {cases, BENCH_COUNT(cases)};
