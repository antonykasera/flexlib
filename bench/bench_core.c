/**
 * @file bench_core.c
 * @brief Inputs, case groups, statistics and the golden check (target + host)
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include <string.h>

volatile uint32_t bench_in[BENCH_N];

extern const BenchGroup bench_group_arch;
extern const BenchGroup bench_group_libm;
#ifdef BENCH_HAVE_CMSIS_DSP
extern const BenchGroup bench_group_cmsis;
#endif

const BenchGroup *const bench_groups[] = {
    &bench_group_arch,
    &bench_group_libm,
#ifdef BENCH_HAVE_CMSIS_DSP
    &bench_group_cmsis,
#endif
};
const unsigned bench_groups_n = BENCH_COUNT(bench_groups);

void bench_inputs_init(void) {
  uint32_t s = 0x2545F491U; /* xorshift32; any nonzero seed */
  for (unsigned i = 0U; i < BENCH_N; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    bench_in[i] = s;
  }
}

static inline uint32_t in_u32(uint32_t u) { return u; }

BENCH_UNARY(b_empty, uint32_t, in_u32, uint32_t, x)

static void sort_u32(uint32_t *v, unsigned n) {
  for (unsigned i = 1U; i < n; i++) {
    uint32_t k = v[i];
    unsigned j = i;
    while (j > 0U && v[j - 1U] > k) {
      v[j] = v[j - 1U];
      j--;
    }
    v[j] = k;
  }
}

uint32_t bench_overhead(uint32_t cyc[BENCH_N]) {
  (void)b_empty(cyc);
  (void)b_empty(cyc);
  sort_u32(cyc, BENCH_N);
  return cyc[BENCH_N / 2U];
}

BenchResult bench_run(const BenchCase *c, uint32_t overhead,
                      uint32_t cyc[BENCH_N]) {
  static uint32_t sorted[BENCH_N];
  BenchResult r;
  /* untimed warm-up: the first pass over code and tables pays cold flash
   * fetches (about 110 cycles per 32-byte line on the bench board) */
  (void)c->fn(cyc);
  r.hash = c->fn(cyc);
  for (unsigned i = 0U; i < BENCH_N; i++) {
    cyc[i] = cyc[i] > overhead ? cyc[i] - overhead : 0U;
    sorted[i] = cyc[i];
  }
  sort_u32(sorted, BENCH_N);
  r.min = sorted[0];
  r.median = sorted[BENCH_N / 2U];
  r.max = sorted[BENCH_N - 1U];

  r.n_slow = 0U;
  for (unsigned i = 0U; i < BENCH_N; i++) {
    if (cyc[i] > 2U * r.median + 4U) {
      if (r.n_slow < BENCH_SLOW_LIST) {
        r.slow_at[r.n_slow] = (uint16_t)i;
      }
      r.n_slow++;
    }
  }
  return r;
}

BenchGoldenStatus bench_golden_check(const BenchCase *c, uint32_t hash) {
  if (!c->golden) {
    return BENCH_GOLDEN_SKIP;
  }
  for (unsigned i = 0U; i < bench_golden_n; i++) {
    if (strcmp(bench_golden[i].name, c->name) == 0) {
      return bench_golden[i].hash == hash ? BENCH_GOLDEN_OK : BENCH_GOLDEN_FAIL;
    }
  }
  return BENCH_GOLDEN_MISSING;
}
