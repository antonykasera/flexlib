/**
 * @file cases_atan.c
 * @brief Bench cases for fm_atan.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fm_atan.h"
#include <stdbool.h>

/* ---- candidates: the library's octant fold, another atan(mn / mx) ---- */

FX_INLINE uint32_t mag(int32_t x) {
  return x < 0 ? 0U - (uint32_t)x : (uint32_t)x;
}

FX_INLINE uint32_t unfold(uint32_t th, bool swap, bool xneg, bool yneg) {
  th = swap ? 0x4000U - th : th;
  th = xneg ? 0x8000U - th : th;
  return yneg ? 0U - th : th;
}

/* odd near-minimax atan on [0, 1], degree 11 (0.02 LSB): no table */
FX_INLINE uint32_t atan_poly(uint32_t mn, uint32_t mx) {
  int32_t x = (int32_t)((((mn << 16) + (mx >> 1)) / mx) << 14); /* Q30 */
  int32_t x2 = fx_smull_hi(x, x) << 2;
  int32_t t = -12583312;
  t = 56529625 + (fx_smull_hi(t, x2) << 2);
  t = -125011946 + (fx_smull_hi(t, x2) << 2);
  t = 207812380 + (fx_smull_hi(t, x2) << 2);
  t = -357151039 + (fx_smull_hi(t, x2) << 2);
  t = 1073717363 + (fx_smull_hi(t, x2) << 2);
  uint32_t p = (uint32_t)(fx_smull_hi(t, x) << 2); /* radians, Q30 */
  return (fx_umull_hi(p, 2734261102U) + 0x8000U) >> 16;
}

/* vectoring CORDIC, 18 steps, no divide: atan(2^-i) in 2^-24 turns */
static const int32_t cordic_atan[18] BENCH_TABLE = {
    2097152, 1238021, 654136, 332050, 166669, 83416, 41718, 20860, 10430,
    5215,    2608,    1304,   652,    326,    163,   81,    41,    20};

FX_INLINE uint32_t atan_cordic(uint32_t mn, uint32_t mx) {
  int32_t x = (int32_t)(mx << 14); /* the 1.65 gain stays below 2^31 */
  int32_t y = (int32_t)(mn << 14);
  int32_t z = 0;
  for (unsigned i = 0U; i < 18U; i++) {
    int32_t xs = x >> i;
    int32_t ys = y >> i;
    if (y >= 0) {
      x += ys;
      y -= xs;
      z += cordic_atan[i];
    } else {
      x -= ys;
      y += xs;
      z -= cordic_atan[i];
    }
  }
  return (uint32_t)(z + 0x80) >> 8;
}

#define ATAN2_15(name, core)                                                   \
  BENCH_CALL static int16_t name(int16_t y, int16_t x) {                       \
    uint32_t ay = mag(y);                                                      \
    uint32_t ax = mag(x);                                                      \
    bool swap = ay > ax;                                                       \
    uint32_t mn = swap ? ax : ay;                                              \
    uint32_t mx = swap ? ay : ax;                                              \
    if (mx == 0U) {                                                            \
      return 0;                                                                \
    }                                                                          \
    return (int16_t)unfold(core(mn, mx), swap, x < 0, y < 0);                  \
  }

ATAN2_15(atan2_poly, atan_poly)
ATAN2_15(atan2_cordic, atan_cordic)

/* ---- inputs ---- */

static inline int16_t in_q15(uint32_t u) { return (int16_t)(u >> 16); }
/* every magnitude */
static inline int32_t in_q31(uint32_t u) { return (int32_t)u >> (u & 15U); }
static inline int32_t in_unit31(uint32_t u) { return (int32_t)u; }

BENCH_BINARY_COLD(a_15, int16_t, in_q15, int16_t, in_q15, int16_t,
                  fm_atan2_q15(a, b))
BENCH_BINARY_COLD(a_15_poly, int16_t, in_q15, int16_t, in_q15, int16_t,
                  atan2_poly(a, b))
BENCH_BINARY(a_15_cordic, int16_t, in_q15, int16_t, in_q15, int16_t,
             atan2_cordic(a, b))
BENCH_UNARY(a_asin15, int16_t, in_q15, int16_t, fm_asin_q15(x))
BENCH_UNARY(a_acos15, int16_t, in_q15, uint16_t, fm_acos_q15(x))
BENCH_BINARY_COLD(a_31, int32_t, in_q31, int32_t, in_q31, int32_t,
                  fm_atan2_q31(a, b))
BENCH_UNARY(a_asin31, int32_t, in_unit31, int32_t, fm_asin_q31(x))
BENCH_UNARY(a_acos31, int32_t, in_unit31, uint32_t, fm_acos_q31(x))

/* the candidates round differently, so they hash differently */
static const BenchCase cases[] = {
    {"fm_atan2_q15", "flexmath", a_15, true},
    {"atan2 q15, degree-11 polynomial", "candidate", a_15_poly, true},
    {"atan2 q15, CORDIC 18 steps", "candidate", a_15_cordic, true},
    {"fm_asin_q15", "flexmath", a_asin15, true},
    {"fm_acos_q15", "flexmath", a_acos15, true},
    {"fm_atan2_q31", "flexmath", a_31, true},
    {"fm_asin_q31", "flexmath", a_asin31, true},
    {"fm_acos_q31", "flexmath", a_acos31, true},
};

const BenchGroup bench_group_atan = {cases, BENCH_COUNT(cases)};

static const BenchCase cold[] = {
    {"fm_atan2_q15", "flexmath", a_15_cold, true},
    {"atan2 q15, degree-11 polynomial", "candidate", a_15_poly_cold, true},
    {"fm_atan2_q31", "flexmath", a_31_cold, true},
};

const BenchGroup bench_group_atan_cold = {cold, BENCH_COUNT(cold)};
