/**
 * @file cases_rand.c
 * @brief Bench cases for fu_rand.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_rand.h"
#include <stdlib.h>

static FU_Xorshift32 xs;
static FU_Xoshiro128 xo;
static volatile uint32_t hundred = 100U; /* a range the compiler can't see */

/* the input is ignored */
static inline uint32_t in_tick(uint32_t u) {
  static unsigned slot;
  if (slot++ % BENCH_N == 0U) {
    fu_xorshift32_seed(&xs, 1U);
    fu_xoshiro128_seed(&xo, 1U);
    srand(1U);
  }
  return u;
}

/* x is pinned in the window, so fold it in to keep it live at no cost */
#define USE_X(v) ((v) + (x & 0U))

BENCH_UNARY(r_xs, uint32_t, in_tick, uint32_t, USE_X(fu_xorshift32(&xs)))
BENCH_UNARY(r_xo, uint32_t, in_tick, uint32_t, USE_X(fu_xoshiro128ss(&xo)))
BENCH_UNARY(r_below, uint32_t, in_tick, uint32_t,
            USE_X(fu_xoshiro128_below(&xo, hundred)))
BENCH_UNARY(r_mod, uint32_t, in_tick, uint32_t,
            USE_X(fu_xoshiro128ss(&xo) % hundred))
BENCH_UNARY(r_rand, uint32_t, in_tick, uint32_t, USE_X((uint32_t)rand()))
BENCH_UNARY(r_jump, uint32_t, in_tick, uint32_t,
            (fu_xoshiro128_jump(&xo), USE_X(xo.s[0])))

/* rand() differs between newlib and glibc, so it isn't golden */
static const BenchCase cases[] = {
    {"fu_xorshift32 (inline)", "flexlib", r_xs, true},
    {"fu_xoshiro128ss (inline)", "flexlib", r_xo, true},
    {"fu_xoshiro128_below(100), unbiased", "flexlib", r_below, true},
    {"xoshiro128** % 100 (UDIV, biased)", "candidate", r_mod, true},
    {"rand()", "newlib", r_rand, false},
    {"fu_xoshiro128_jump (2^64 steps)", "flexlib", r_jump, true},
};

const BenchGroup bench_group_rand = {cases, BENCH_COUNT(cases)};
