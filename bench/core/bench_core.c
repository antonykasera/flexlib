/**
 * @file bench_core.c
 * @brief Inputs, case groups
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include <string.h>

volatile uint32_t bench_in[BENCH_N];
uint8_t bench_arg_a[BENCH_N * 8U] __attribute__((aligned(8)));
uint8_t bench_arg_b[BENCH_N * 8U] __attribute__((aligned(8)));
uint8_t bench_scratch[BENCH_SCRATCH_SIZE] __attribute__((aligned(32)));
uintptr_t bench_cold_keep;

bool bench_scratch_claim(const void *id) {
  static const void *owner;
  bool changed = owner != id;
  owner = id;
  return changed;
}

/* one image per BENCH_SET_x; no set (golden, lint) builds every group */
#if !defined(BENCH_SET_utils) && !defined(BENCH_SET_math) &&                   \
    !defined(BENCH_SET_struct) && !defined(BENCH_SET_dsp) &&                   \
    !defined(BENCH_SET_dsp2) && !defined(BENCH_SET_dsp3) &&                    \
    !defined(BENCH_SET_dsp4) && !defined(BENCH_SET_dsp5)
#define BENCH_SET_ALL 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_utils)
#define BENCH_UTILS 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_math)
#define BENCH_MATH 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_struct)
#define BENCH_STRUCT 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_dsp)
#define BENCH_DSP 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_dsp2)
#define BENCH_DSP2 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_dsp3)
#define BENCH_DSP3 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_dsp4)
#define BENCH_DSP4 1
#endif
#if defined(BENCH_SET_ALL) || defined(BENCH_SET_dsp5)
#define BENCH_DSP5 1
#endif

extern const BenchGroup bench_group_arch;
extern const BenchGroup bench_group_bits;
extern const BenchGroup bench_group_fmt;
extern const BenchGroup bench_group_crc;
extern const BenchGroup bench_group_ring;
extern const BenchGroup bench_group_parse;
extern const BenchGroup bench_group_cobs;
extern const BenchGroup bench_group_debounce;
extern const BenchGroup bench_group_rand;
extern const BenchGroup bench_group_fixed;
extern const BenchGroup bench_group_sqrt;
extern const BenchGroup bench_group_div;
extern const BenchGroup bench_group_trig;
extern const BenchGroup bench_group_atan;
extern const BenchGroup bench_group_exp;
extern const BenchGroup bench_group_float;
extern const BenchGroup bench_group_libm;
extern const BenchGroup bench_group_linalg;
extern const BenchGroup bench_group_quat;
extern const BenchGroup bench_group_pid;
extern const BenchGroup bench_group_pairs;
extern const BenchGroup bench_group_fd_vec;
extern const BenchGroup bench_group_fd_fir;
extern const BenchGroup bench_group_fd_biquad;
extern const BenchGroup bench_group_fd_tone;
extern const BenchGroup bench_group_fd_fft;
extern const BenchGroup bench_group_fd_corr;
extern const BenchGroup bench_group_fd_resample;
extern const BenchGroup bench_group_cmsis;

const BenchGroup *const bench_groups[] = {
#ifdef BENCH_UTILS
    &bench_group_arch,
    &bench_group_bits,
    &bench_group_fmt,
    &bench_group_crc,
    &bench_group_ring,
    &bench_group_parse,
    &bench_group_cobs,
    &bench_group_debounce,
    &bench_group_rand,
#endif
#ifdef BENCH_MATH
    &bench_group_fixed,
    &bench_group_sqrt,
    &bench_group_div,
    &bench_group_trig,
    &bench_group_atan,
    &bench_group_exp,
    &bench_group_float,
    &bench_group_libm,
#endif
#ifdef BENCH_STRUCT
    &bench_group_linalg,
    &bench_group_quat,
    &bench_group_pid,
#endif
#ifdef BENCH_DSP
    &bench_group_pairs,
    &bench_group_fd_vec,
    &bench_group_fd_fir,
#endif
#ifdef BENCH_DSP2
    &bench_group_fd_biquad,
    &bench_group_fd_tone,
#endif
#ifdef BENCH_DSP3
    &bench_group_fd_fft,
#endif
#ifdef BENCH_DSP4
    &bench_group_fd_corr,
#endif
#ifdef BENCH_DSP5
    &bench_group_fd_resample,
#endif
#if defined(BENCH_HAVE_CMSIS_DSP) && defined(BENCH_MATH)
    &bench_group_cmsis,
#endif
    NULL, /* an image may have no groups of a kind */
};
const unsigned bench_groups_n = BENCH_COUNT(bench_groups) - 1U;

extern const BenchGroup bench_group_crc_cold;
extern const BenchGroup bench_group_sqrt_cold;
extern const BenchGroup bench_group_trig_cold;
extern const BenchGroup bench_group_atan_cold;
extern const BenchGroup bench_group_exp_cold;
extern const BenchGroup bench_group_float_cold;
extern const BenchGroup bench_group_libm_cold;
extern const BenchGroup bench_group_linalg_cold;
extern const BenchGroup bench_group_quat_cold;
extern const BenchGroup bench_group_cmsis_cold;

const BenchGroup *const bench_cold_groups[] = {
#ifdef BENCH_UTILS
    &bench_group_crc_cold,
#endif
#ifdef BENCH_MATH
    &bench_group_sqrt_cold,
    &bench_group_trig_cold,
    &bench_group_atan_cold,
    &bench_group_exp_cold,
    &bench_group_float_cold,
    &bench_group_libm_cold,
#endif
#ifdef BENCH_STRUCT
    &bench_group_linalg_cold,
    &bench_group_quat_cold,
#endif
#if defined(BENCH_HAVE_CMSIS_DSP) && defined(BENCH_MATH)
    &bench_group_cmsis_cold,
#endif
    NULL,
};
const unsigned bench_cold_groups_n = BENCH_COUNT(bench_cold_groups) - 1U;

void bench_inputs_init(void) {
  uint32_t s = 0x2545F491U; /* xorshift32 */
  for (unsigned i = 0U; i < BENCH_N; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    bench_in[i] = s;
  }
}

static inline uint32_t in_u32(uint32_t u) { return u; }

BENCH_UNARY_COLD(b_empty, uint32_t, in_u32, uint32_t, x)

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

/* a _cold case reads fn's code back after each flush */
static uint32_t run_case(BenchFn fn, uint32_t cyc[BENCH_N]) {
  bench_cold_keep = (uintptr_t)fn;
  return fn(cyc);
}

uint32_t bench_overhead(uint32_t cyc[BENCH_N], bool cold) {
  BenchFn fn = cold ? b_empty_cold : b_empty;
  (void)run_case(fn, cyc);
  (void)run_case(fn, cyc);
  sort_u32(cyc, BENCH_N);
  return cyc[BENCH_N / 2U];
}

BenchResult bench_run(const BenchCase *c, uint32_t overhead,
                      uint32_t cyc[BENCH_N]) {
  static uint32_t sorted[BENCH_N];
  BenchResult r;
  (void)run_case(c->fn, cyc);
  r.hash = run_case(c->fn, cyc);
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
