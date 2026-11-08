/**
 * @file cases_debounce.c
 * @brief Bench cases for fu_debounce.h: per tick, with 16 integrators as the
 *        alternative to the vertical counter
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fu_debounce.h"

static FU_Debounce32 vert;
static FU_Debounce one;
static FU_Debounce pins[16];

/* 16 inputs, one integrator each */
BENCH_CALL static uint32_t deb16(uint32_t raw) {
  uint32_t changed = 0U;
  for (unsigned i = 0U; i < 16U; i++) {
    changed |= (fu_debounce(&pins[i], ((raw >> i) & 1U) != 0U) ? 1U : 0U) << i;
  }
  return changed;
}

/* Raw samples: each input changes rarely and bounces after (random bits
 * masked by a decaying noise mask). Everything restarts at the first input,
 * since the board runs a case twice and the host once. */
static inline uint32_t in_raw(uint32_t u) {
  static unsigned slot;
  static uint32_t level;
  static uint32_t noise;
  static uint32_t s;
  if (slot++ % BENCH_N == 0U) {
    level = 0U;
    noise = 0U;
    s = 0x6D2B79F5U;
  }
  if ((u & 0xFFU) == 0U) {
    level ^= u >> 8;
  }
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  noise = (noise | (u >> 20)) & s;
  fu_debounce32_init(&vert, 0U);
  fu_debounce_init(&one, 5U, false);
  for (unsigned i = 0U; i < 16U; i++) {
    fu_debounce_init(&pins[i], 4U, false);
  }
  return level ^ (noise & u);
}

BENCH_UNARY(d_vert, uint32_t, in_raw, uint32_t, fu_debounce32(&vert, x))
BENCH_UNARY(d_one, uint32_t, in_raw, uint32_t,
            fu_debounce(&one, (x & 1U) != 0U) ? 1U : 0U)
BENCH_UNARY(d_16, uint32_t, in_raw, uint32_t, deb16(x))

static const BenchCase cases[] = {
    {"fu_debounce32, 32 inputs per tick (inline)", "flexlib", d_vert, true},
    {"fu_debounce, 1 input per tick (inline)", "flexlib", d_one, true},
    {"16 x fu_debounce, 16 inputs per tick", "candidate", d_16, true},
};

const BenchGroup bench_group_debounce = {cases, BENCH_COUNT(cases)};
