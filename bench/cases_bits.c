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

BENCH_CALL static uint32_t pop_table8(uint32_t x) {
  return (uint32_t)pop8[x & 0xFFU] + pop8[(x >> 8) & 0xFFU] +
         pop8[(x >> 16) & 0xFFU] + pop8[x >> 24];
}

BENCH_CALL static uint32_t pop_table4(uint32_t x) {
  uint32_t n = 0U;
  for (unsigned i = 0U; i < 8U; i++) {
    n += pop4[(x >> (4U * i)) & 0xFU];
  }
  return n;
}

/* one iteration per set bit */
BENCH_CALL static uint32_t pop_kernighan(uint32_t x) {
  uint32_t n = 0U;
  for (; x != 0U; x &= x - 1U) {
    n++;
  }
  return n;
}

BENCH_CALL static uint32_t pop_swar_call(uint32_t x) {
  return fu_popcount(x);
}

/* ---- clz64 / ctz64 candidates: branchless, since clz(w) >> 5 is 1 exactly
 * when w == 0 (GCC turns the multiply-by-0-or-1 into one MLA) ---- */

static inline uint32_t clz64_mla(uint64_t x) {
  uint32_t h = fx_clz((uint32_t)(x >> 32));
  return h + fx_clz((uint32_t)x) * (h >> 5);
}

static inline uint32_t ctz64_mla(uint64_t x) {
  uint32_t l = fx_ctz((uint32_t)x);
  return l + fx_ctz((uint32_t)(x >> 32)) * (l >> 5);
}

/* ---- inputs ---- */

static inline uint32_t in_u32(uint32_t u) { return u; }
static inline uint32_t in_bits(uint32_t u) { return 1U + (u & 31U); }
/* about half have a zero high word (clz64) or zero low word (ctz64) */
static inline uint64_t in_u64_right(uint32_t u) {
  return ((uint64_t)u * 0x9E3779B97F4A7C15ULL) >> (u & 63U);
}
static inline uint64_t in_u64_left(uint32_t u) {
  return ((uint64_t)u * 0x9E3779B97F4A7C15ULL) << (u & 63U);
}
/* both words nonzero, with varied counts */
static inline uint64_t in_u64_both(uint32_t u) {
  return (((uint64_t)u * 0x9E3779B97F4A7C15ULL) >> (u & 31U)) |
         (1ULL << (32U + (u >> 27))) | (1ULL << ((u >> 22) & 31U));
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
BENCH_UNARY(b_clz64_half, uint64_t, in_u64_right, uint32_t, fu_clz64(x))
BENCH_UNARY(b_clz64_both, uint64_t, in_u64_both, uint32_t, fu_clz64(x))
BENCH_UNARY(b_clz64m_half, uint64_t, in_u64_right, uint32_t, clz64_mla(x))
BENCH_UNARY(b_clz64m_both, uint64_t, in_u64_both, uint32_t, clz64_mla(x))
BENCH_UNARY(b_ctz64_half, uint64_t, in_u64_left, uint32_t, fu_ctz64(x))
BENCH_UNARY(b_ctz64_both, uint64_t, in_u64_both, uint32_t, fu_ctz64(x))
BENCH_UNARY(b_ctz64m_half, uint64_t, in_u64_left, uint32_t, ctz64_mla(x))
BENCH_UNARY(b_ctz64m_both, uint64_t, in_u64_both, uint32_t, ctz64_mla(x))
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
    {"fu_clz64, half hi = 0", "flexlib", b_clz64_half, true},
    {"fu_clz64, hi != 0", "flexlib", b_clz64_both, true},
    {"clz64 branchless, half hi = 0", "candidate", b_clz64m_half, true},
    {"clz64 branchless, hi != 0", "candidate", b_clz64m_both, true},
    {"fu_ctz64, half lo = 0", "flexlib", b_ctz64_half, true},
    {"fu_ctz64, lo != 0", "flexlib", b_ctz64_both, true},
    {"ctz64 branchless, half lo = 0", "candidate", b_ctz64m_half, true},
    {"ctz64 branchless, lo != 0", "candidate", b_ctz64m_both, true},
    {"fu_load_be32, aligned", "flexlib", b_load_be32_0, true},
    {"fu_load_be32, +1", "flexlib", b_load_be32_1, true},
    {"fu_load_be32, +2", "flexlib", b_load_be32_2, true},
    {"fu_sext(x, 12)", "flexlib", b_sext12, true},
    {"fu_sext(x, n)", "flexlib", b_sext, true},
};

const BenchGroup bench_group_bits = {cases, BENCH_COUNT(cases)};
