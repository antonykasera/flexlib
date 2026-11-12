/**
 * @file cases_pairs.c
 * @brief Bench cases for the fx_arch.h packed pairs (C path on the M3)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fx_arch.h"

static inline int32_t in_s32(uint32_t u) { return (int32_t)u; }

BENCH_BINARY(b_pkhbt, int32_t, in_s32, int32_t, in_s32, int32_t, fx_pkhbt(a, b))
BENCH_BINARY(b_pkhtb, int32_t, in_s32, int32_t, in_s32, int32_t, fx_pkhtb(a, b))
BENCH_BINARY(b_smlad, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_smlad(a, a, b))
BENCH_BINARY(b_smlald, int32_t, in_s32, int32_t, in_s32, int64_t,
             fx_smlald((int64_t)a, a, b))
BENCH_BINARY(b_smuad, int32_t, in_s32, int32_t, in_s32, int32_t, fx_smuad(a, b))
BENCH_BINARY(b_smusdx, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_smusdx(a, b))
BENCH_BINARY(b_qadd16, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_qadd16(a, b))
BENCH_BINARY(b_qsub16, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_qsub16(a, b))
BENCH_BINARY(b_shadd16, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_shadd16(a, b))
BENCH_UNARY(b_sxtb16, int32_t, in_s32, int32_t, fx_sxtb16(x))
BENCH_UNARY(b_ssat16x8, int32_t, in_s32, int32_t, FX_SSAT16(x, 8))
BENCH_BINARY(b_smmulr, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_smmulr(a, b))
BENCH_BINARY(b_smulwb, int32_t, in_s32, int32_t, in_s32, int32_t,
             fx_smulwb(a, b))

static const BenchCase cases[] = {
    {"fx_pkhbt", "flexlib", b_pkhbt, true},
    {"fx_pkhtb", "flexlib", b_pkhtb, true},
    {"fx_smlad", "flexlib", b_smlad, true},
    {"fx_smlald", "flexlib", b_smlald, true},
    {"fx_smuad", "flexlib", b_smuad, true},
    {"fx_smusdx", "flexlib", b_smusdx, true},
    {"fx_qadd16", "flexlib", b_qadd16, true},
    {"fx_qsub16", "flexlib", b_qsub16, true},
    {"fx_shadd16", "flexlib", b_shadd16, true},
    {"fx_sxtb16", "flexlib", b_sxtb16, true},
    {"FX_SSAT16(x, 8)", "flexlib", b_ssat16x8, true},
    {"fx_smmulr", "flexlib", b_smmulr, true},
    {"fx_smulwb", "flexlib", b_smulwb, true},
};

const BenchGroup bench_group_pairs = {cases, BENCH_COUNT(cases)};
