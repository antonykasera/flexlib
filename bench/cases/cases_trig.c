/**
 * @file cases_trig.c
 * @brief Bench cases for fm_trig.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fm_tables.h"
#include "fm_trig.h"
#include <stdbool.h>

/* ---- candidates ---- */

/* odd minimax sin(pi x / 2) on [0, 1], degree 7 (2e-2 LSB): no table */
BENCH_CALL static fm_q15 sin15_poly(uint16_t a) {
  int32_t p = a & 0x3FFF;
  if ((a & 0x4000U) != 0U) {
    p = 0x4000 - p;
  }
  int32_t x = p << 16; /* Q30 */
  int32_t x2 = fx_smull_hi(x, x) << 2;
  int32_t t = -4652626;
  t = 85291978 + (fx_smull_hi(t, x2) << 2);
  t = -693522166 + (fx_smull_hi(t, x2) << 2);
  t = 1686624005 + (fx_smull_hi(t, x2) << 2);
  int32_t m = ((fx_smull_hi(t, x) << 2) + 0x4000) >> 15;
  return (fm_q15)FX_SSAT((a & 0x8000U) != 0U ? -m : m, 16);
}

/* the library's kernels with the table as an argument, for a copy in SRAM */
FX_INLINE fm_q15 sin15_tab(uint32_t a, const uint32_t *tab) {
  uint32_t p = a & 0x3FFFU;
  if ((a & 0x4000U) != 0U) {
    p = 0x4000U - p;
  }
  uint32_t i = p >> 6;
  uint32_t lo = tab[i];
  uint32_t mag = lo + (((tab[i + 1U] - lo) * (p & 63U)) >> 6);
  int32_t m = (int32_t)((mag + 0x8000U) >> 16);
  return (fm_q15)FX_SSAT((a & 0x8000U) != 0U ? -m : m, 16);
}

FX_INLINE int32_t sin31_tab(uint32_t angle, const uint32_t *tab) {
  uint32_t q = angle >> 30;
  uint32_t p = angle & 0x3FFFFFFFU;
  bool cosine = (q & 1U) != 0U;
  uint32_t i = p >> 22;
  uint32_t s = tab[i];
  uint32_t c = tab[256U - i];
  uint32_t x = cosine ? c : s;
  uint32_t y = cosine ? s : c;
  uint32_t d = fx_umull_hi((p & 0x3FFFFFU) << 10, 1686629713U);
  uint32_t d2 = fx_umull_hi(d, d) << 2;
  int32_t yd = (int32_t)fx_umull_hi(y, d);
  int32_t yd6 = (int32_t)((((uint32_t)yd >> 14) * 43691U) >> 10);
  if (cosine) {
    yd = -yd;
    yd6 = -yd6;
  }
  int32_t corr = (int32_t)(fx_umull_hi(d2, (x >> 1) + (uint32_t)yd6) >> 8);
  uint32_t m = x + (uint32_t)((yd - corr + 32) >> 6);
  if ((q & 2U) != 0U) {
    return (int32_t)(0U - m);
  }
  return m == 0x80000000U ? INT32_MAX : (int32_t)m;
}

/* the SRAM copy, made by the (untimed) input conversion */
#define RAM_TAB ((const uint32_t *)(const void *)bench_scratch)

static void ram_tab_fill(void) {
  static const char id = 0;
  if (bench_scratch_claim(&id)) {
    memcpy(bench_scratch, fm_sin_tab, sizeof fm_sin_tab);
  }
}

BENCH_CALL static fm_q15 sin15_ram(uint16_t a, const uint32_t *tab) {
  return sin15_tab(a, tab);
}

BENCH_CALL static int32_t sin31_ram(uint32_t a, const uint32_t *tab) {
  return sin31_tab(a, tab);
}

static inline uint32_t pack15(fm_q15 s, fm_q15 c) {
  return (uint32_t)(uint16_t)s << 16 | (uint16_t)c;
}
static inline uint32_t sincos15_split(uint16_t a) {
  return pack15(fm_sin_q15(a), fm_cos_q15(a));
}
static inline uint32_t sincos15(uint16_t a) {
  fm_q15 s, c;
  fm_sincos_q15(a, &s, &c);
  return pack15(s, c);
}
static inline int32_t sincos31_split(uint32_t a) {
  return fm_sin_q31(a) ^ fm_cos_q31(a);
}
static inline int32_t sincos31(uint32_t a) {
  fm_q31 s, c;
  fm_sincos_q31(a, &s, &c);
  return s ^ c;
}

/* ---- inputs: uniform angles ---- */

static inline uint16_t in_a16(uint32_t u) { return (uint16_t)(u >> 16); }
static inline uint32_t in_a32(uint32_t u) { return u; }
static inline uint16_t in_a16_ram(uint32_t u) {
  ram_tab_fill();
  return in_a16(u);
}
static inline uint32_t in_a32_ram(uint32_t u) {
  ram_tab_fill();
  return u;
}

BENCH_UNARY_COLD(t_s15, uint16_t, in_a16, int16_t, fm_sin_q15(x))
BENCH_UNARY_COLD(t_s15_poly, uint16_t, in_a16, int16_t, sin15_poly(x))
BENCH_UNARY_COLD(t_s15_ram, uint16_t, in_a16_ram, int16_t,
                 sin15_ram(x, RAM_TAB))
BENCH_UNARY(t_c15, uint16_t, in_a16, int16_t, fm_cos_q15(x))
BENCH_UNARY(t_sc15, uint16_t, in_a16, uint32_t, sincos15(x))
BENCH_UNARY(t_sc15_split, uint16_t, in_a16, uint32_t, sincos15_split(x))
BENCH_UNARY_COLD(t_s31, uint32_t, in_a32, int32_t, fm_sin_q31(x))
BENCH_UNARY_COLD(t_s31_ram, uint32_t, in_a32_ram, int32_t,
                 sin31_ram(x, RAM_TAB))
BENCH_UNARY(t_c31, uint32_t, in_a32, int32_t, fm_cos_q31(x))
BENCH_UNARY_COLD(t_sc31, uint32_t, in_a32, int32_t, sincos31(x))
BENCH_UNARY(t_sc31_split, uint32_t, in_a32, int32_t, sincos31_split(x))

/* the polynomial rounds differently, so it has a hash of its own */
static const BenchCase cases[] = {
    {"fm_sin_q15", "flexmath", t_s15, true},
    {"sin q15, degree-7 polynomial", "candidate", t_s15_poly, true},
    {"sin q15, table in SRAM", "candidate", t_s15_ram, true},
    {"fm_cos_q15", "flexmath", t_c15, true},
    {"fm_sincos_q15", "flexmath", t_sc15, true},
    {"fm_sin_q15 + fm_cos_q15", "candidate", t_sc15_split, true},
    {"fm_sin_q31", "flexmath", t_s31, true},
    {"sin q31, table in SRAM", "candidate", t_s31_ram, true},
    {"fm_cos_q31", "flexmath", t_c31, true},
    {"fm_sincos_q31", "flexmath", t_sc31, true},
    {"fm_sin_q31 + fm_cos_q31", "candidate", t_sc31_split, true},
};

const BenchGroup bench_group_trig = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_sin_q15", "flexmath", t_s15_cold, true},
    {"sin q15, degree-7 polynomial", "candidate", t_s15_poly_cold, true},
    {"sin q15, table in SRAM", "candidate", t_s15_ram_cold, true},
    {"fm_sin_q31", "flexmath", t_s31_cold, true},
    {"sin q31, table in SRAM", "candidate", t_s31_ram_cold, true},
    {"fm_sincos_q31", "flexmath", t_sc31_cold, true},
};

const BenchGroup bench_group_trig_cold = {cold, BENCH_COUNT(cold)};
