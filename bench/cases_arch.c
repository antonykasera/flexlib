/**
 * @file cases_arch.c
 * @brief Bench cases for fx_arch.h. Each inlines to one instruction, so these
 *        also sanity-check the overhead subtraction.
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fx_arch.h"

static inline uint32_t in_u32(uint32_t u) { return u; }
static inline int32_t in_s32(uint32_t u) { return (int32_t)u; }
/* shifted right by 0..31, so the leading-zero count varies */
static inline uint32_t in_shifted(uint32_t u) { return u >> (u & 31U); }
/* +-2^17: SSAT #16 clips about 3/4 of these */
static inline int32_t in_s18(uint32_t u) { return (int32_t)u >> 14; }
/* +-2^9: USAT #8 clips the negatives and about half the rest */
static inline int32_t in_s10(uint32_t u) { return (int32_t)u >> 22; }
static inline int16_t in_s16(uint32_t u) { return (int16_t)(u >> 16); }

BENCH_UNARY(b_empty_s16, int16_t, in_s16, int32_t, x)
BENCH_UNARY(b_clz, uint32_t, in_shifted, uint32_t, fx_clz(x))
BENCH_UNARY(b_ctz, uint32_t, in_u32, uint32_t, fx_ctz(x))
BENCH_UNARY(b_rbit, uint32_t, in_u32, uint32_t, fx_rbit(x))
BENCH_UNARY(b_rev, uint32_t, in_u32, uint32_t, fx_rev(x))
BENCH_UNARY(b_ssat16, int32_t, in_s18, int32_t, FX_SSAT(x, 16))
BENCH_UNARY(b_usat8, int32_t, in_s10, uint32_t, FX_USAT(x, 8))
BENCH_BINARY(b_smull_hi, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_smull_hi(a, b))
BENCH_BINARY(b_umull_hi, uint32_t, in_u32, uint32_t, in_u32, uint32_t,
             fx_umull_hi(a, b))

static const BenchCase cases[] = {
    {"empty, int16 arg", "-", b_empty_s16, true},
    {"fx_clz", "flexlib", b_clz, true},
    {"fx_ctz", "flexlib", b_ctz, true},
    {"fx_rbit", "flexlib", b_rbit, true},
    {"fx_rev", "flexlib", b_rev, true},
    {"FX_SSAT(x, 16)", "flexlib", b_ssat16, true},
    {"FX_USAT(x, 8)", "flexlib", b_usat8, true},
    {"fx_smull_hi", "flexlib", b_smull_hi, true},
    {"fx_umull_hi", "flexlib", b_umull_hi, true},
};

const BenchGroup bench_group_arch = {cases, BENCH_COUNT(cases)};
