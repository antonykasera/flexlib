/**
 * @file cases_bits.c
 * @brief Bench cases for fu_bits.h, with the popcount candidates it was
 *        chosen from
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_bits.h"

/* ---- popcount candidates (fu_popcount is the SWAR one) ---- */

static const uint8_t pop8[256] __attribute__((aligned(32))) = {
#define P2(n) n, n + 1, n + 1, n + 2
#define P4(n) P2(n), P2(n + 1), P2(n + 1), P2(n + 2)
#define P6(n) P4(n), P4(n + 1), P4(n + 1), P4(n + 2)
    P6(0), P6(1), P6(1), P6(2)
#undef P2
#undef P4
#undef P6
};

static const uint8_t pop4[16] = {0, 1, 1, 2, 1, 2, 2, 3,
                                 1, 2, 2, 3, 2, 3, 3, 4};

__attribute__((noinline)) static uint32_t pop_table8(uint32_t x) {
  return (uint32_t)pop8[x & 0xFFU] + pop8[(x >> 8) & 0xFFU] +
         pop8[(x >> 16) & 0xFFU] + pop8[x >> 24];
}

__attribute__((noinline)) static uint32_t pop_table4(uint32_t x) {
  uint32_t n = 0U;
  for (unsigned i = 0U; i < 8U; i++) {
    n += pop4[(x >> (4U * i)) & 0xFU];
  }
  return n;
}

/* one iteration per set bit */
__attribute__((noinline)) static uint32_t pop_kernighan(uint32_t x) {
  uint32_t n = 0U;
  for (; x != 0U; x &= x - 1U) {
    n++;
  }
  return n;
}

__attribute__((noinline)) static uint32_t pop_swar_call(uint32_t x) {
  return fu_popcount(x);
}

/* ---- inputs ---- */

static inline uint32_t in_u32(uint32_t u) { return u; }
static inline uint32_t in_bits(uint32_t u) { return 1U + (u & 31U); }
/* varied leading and trailing zero counts */
static inline uint64_t in_u64(uint32_t u) {
  return ((uint64_t)u * 0x9E3779B97F4A7C15ULL) >> (u & 63U);
}

static uint8_t buf[16] __attribute__((aligned(4))) = {
    0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,
    0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED, 0xFE, 0x0F};

static inline const uint8_t *in_ptr0(uint32_t u) { return &buf[(u & 1U) * 4U]; }
static inline const uint8_t *in_ptr1(uint32_t u) {
  return &buf[1U + (u & 1U) * 4U];
}
static inline const uint8_t *in_ptr2(uint32_t u) {
  return &buf[2U + (u & 1U) * 4U];
}

BENCH_UNARY(b_popcount, uint32_t, in_u32, uint32_t, fu_popcount(x))
BENCH_UNARY(b_pop_swar_call, uint32_t, in_u32, uint32_t, pop_swar_call(x))
BENCH_UNARY(b_pop_libgcc, uint32_t, in_u32, uint32_t,
            (uint32_t)__builtin_popcount(x))
BENCH_UNARY(b_pop_table8, uint32_t, in_u32, uint32_t, pop_table8(x))
BENCH_UNARY(b_pop_table4, uint32_t, in_u32, uint32_t, pop_table4(x))
BENCH_UNARY(b_pop_kernighan, uint32_t, in_u32, uint32_t, pop_kernighan(x))
BENCH_UNARY(b_clz64, uint64_t, in_u64, uint32_t, fu_clz64(x))
BENCH_UNARY(b_ctz64, uint64_t, in_u64, uint32_t, fu_ctz64(x))
BENCH_UNARY(b_load_be32_0, const uint8_t *, in_ptr0, uint32_t,
            fu_load_be32(x))
BENCH_UNARY(b_load_be32_1, const uint8_t *, in_ptr1, uint32_t,
            fu_load_be32(x))
BENCH_UNARY(b_load_be32_2, const uint8_t *, in_ptr2, uint32_t,
            fu_load_be32(x))
BENCH_UNARY(b_sext12, uint32_t, in_u32, int32_t, fu_sext(x, 12U))
BENCH_BINARY(b_sext, uint32_t, in_u32, uint32_t, in_bits, int32_t,
             fu_sext(a, b))

/* Inline cases time the kernel alone; the "call" and candidate rows include
 * a call and return, so compare those with each other. */
static const BenchCase cases[] = {
    {"fu_popcount", "flexlib", b_popcount, true},
    {"popcount SWAR, call", "flexlib", b_pop_swar_call, true},
    {"popcount __builtin", "libgcc", b_pop_libgcc, true},
    {"popcount table 256 B, call", "candidate", b_pop_table8, true},
    {"popcount table 16 B, call", "candidate", b_pop_table4, true},
    {"popcount x &= x - 1, call", "candidate", b_pop_kernighan, true},
    {"fu_clz64", "flexlib", b_clz64, true},
    {"fu_ctz64", "flexlib", b_ctz64, true},
    {"fu_load_be32, aligned", "flexlib", b_load_be32_0, true},
    {"fu_load_be32, +1", "flexlib", b_load_be32_1, true},
    {"fu_load_be32, +2", "flexlib", b_load_be32_2, true},
    {"fu_sext(x, 12)", "flexlib", b_sext12, true},
    {"fu_sext(x, n)", "flexlib", b_sext, true},
};

const BenchGroup bench_group_bits = {cases, BENCH_COUNT(cases)};
