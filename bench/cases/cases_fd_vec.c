/**
 * @file cases_fd_vec.c
 * @brief Bench cases for fd_vec.h: blocks of 64
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_vec.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define BLK 64U
#define POOL 128U

static fm_q15 p15[POOL] FX_ALIGN(4);
static fm_q31 p31[POOL];
static fd_cq15 c15[POOL];
static fd_cq31 c31[POOL];
static float pf[BLK];
static union {
  fm_q15 q15[BLK];
  fm_q31 q31[BLK];
  uint32_t u32[BLK];
  uint16_t u16[BLK];
  float f[BLK];
  fd_cq15 c15[BLK];
  fd_cq31 c31[BLK];
} out;

static void pools_init(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x2545F491U;
  for (unsigned i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    p31[i] = (fm_q31)s;
    p15[i] = (fm_q15)(s >> 16);
    c15[i].re = (fm_q15)s;
    c15[i].im = (fm_q15)(s >> 16);
    c31[i].re = (fm_q31)s;
    c31[i].im = (fm_q31)fx_rotl32(s, 13U);
  }
  for (unsigned i = 0U; i < BLK; i++) {
    pf[i] = (float)p15[i] * (1.0f / 32768.0f);
  }
}

/* even offsets keep q15 pairs word-aligned: a at x, b at 62 - x */
static inline uint32_t in_off(uint32_t u) {
  pools_init();
  return (u % 32U) * 2U;
}

#define A15 (p15 + x)
#define B15 (p15 + 62U - x)
#define A31 (p31 + x)
#define B31 (p31 + 62U - x)
#define CA15 (c15 + x)
#define CB15 (c15 + 62U - x)
#define CA31 (c31 + x)
#define CB31 (c31 + 62U - x)
#define OUT(bytes) h = bench_hash(h, &out, (bytes))

#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_POST(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes))

/* ---- candidates ---- */

/* word pairs: two LDR, split, two SSAT, pack, one STR (n even, aligned) */
BENCH_CALL static void add_q15_pairs(const fm_q15 *a, const fm_q15 *b,
                                     fm_q15 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i += 2U) {
    int32_t pa, pb;
    memcpy(&pa, a + i, 4U);
    memcpy(&pb, b + i, 4U);
    int32_t r = fx_pack16(FX_SSAT(fx_lo16(pa) + fx_lo16(pb), 16),
                          FX_SSAT(fx_hi16(pa) + fx_hi16(pb), 16));
    memcpy(y + i, &r, 4U);
  }
}

BENCH_CALL static void mul_q15_pairs(const fm_q15 *a, const fm_q15 *b,
                                     fm_q15 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i += 2U) {
    int32_t pa, pb;
    memcpy(&pa, a + i, 4U);
    memcpy(&pb, b + i, 4U);
    int32_t lo = (fx_lo16(pa) * fx_lo16(pb) + 0x4000) >> 15;
    int32_t hi = (fx_hi16(pa) * fx_hi16(pb) + 0x4000) >> 15;
    int32_t r = fx_pack16(FX_SSAT(lo, 16), FX_SSAT(hi, 16));
    memcpy(y + i, &r, 4U);
  }
}

/* MUL to 32 bits, then a 64-bit add (GCC fuses the library loop into SMLAL) */
BENCH_CALL static int64_t dot_q15_mul_add(const fm_q15 *a, const fm_q15 *b,
                                          uint32_t n) {
  int64_t s = 0;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t p = a[i] * b[i];
    FX_OPAQUE(p);
    s += p;
  }
  return s;
}

/* ---- element-wise ---- */

BLOCK(b_add15, fd_add_q15(A15, B15, out.q15, BLK), 128U)
BLOCK(b_add15_pairs, add_q15_pairs(A15, B15, out.q15, BLK), 128U)
BLOCK(b_add31, fd_add_q31(A31, B31, out.q31, BLK), 256U)
BLOCK(b_mul15, fd_mul_q15(A15, B15, out.q15, BLK), 128U)
BLOCK(b_mul15_pairs, mul_q15_pairs(A15, B15, out.q15, BLK), 128U)
BLOCK(b_mul31, fd_mul_q31(A31, B31, out.q31, BLK), 256U)
BLOCK(b_scale15, fd_scale_q15(A15, 0x6000, 1, out.q15, BLK), 128U)
BLOCK(b_scale31, fd_scale_q31(A31, 0x60000000, 1, out.q31, BLK), 256U)
BLOCK(b_shift15, fd_shift_q15(A15, -3, out.q15, BLK), 128U)
BLOCK(b_offset15, fd_offset_q15(A15, 0x1234, out.q15, BLK), 128U)
BLOCK(b_negate15, fd_negate_q15(A15, out.q15, BLK), 128U)
BLOCK(b_abs15, fd_abs_q15(A15, out.q15, BLK), 128U)
BLOCK(b_clip15, fd_clip_q15(A15, -16384, 16383, out.q15, BLK), 128U)
BLOCK(b_fill15, fd_fill_q15((fm_q15)x, out.q15, BLK), 128U)
BLOCK(b_copy15, fd_copy_q15(A15, out.q15, BLK), 128U)
BLOCK(b_copy15_memcpy, memcpy(out.q15, A15, BLK * 2U), 128U)
BLOCK(b_q31_to_q15, fd_q31_to_q15(A31, out.q15, BLK), 128U)
BLOCK(b_q15_to_f32, fd_q15_to_f32(A15, out.f, BLK), 256U)
BLOCK(b_f32_to_q15, fd_f32_to_q15(pf, out.q15, BLK), 128U)
BLOCK(b_db15, fd_db_q15(A15, out.q31, BLK), 256U)

/* ---- statistics ---- */

BENCH_UNARY(b_sum15, uint32_t, in_off, int64_t, fd_sum_q15(A15, BLK))
BENCH_UNARY(b_mean15, uint32_t, in_off, fm_q15, fd_mean_q15(A15, BLK))
BENCH_UNARY(b_dot15, uint32_t, in_off, int64_t, fd_dot_q15(A15, B15, BLK))
BENCH_UNARY(b_dot15_mul_add, uint32_t, in_off, int64_t,
            dot_q15_mul_add(A15, B15, BLK))
BENCH_UNARY(b_dot31, uint32_t, in_off, int64_t, fd_dot_q31(A31, B31, BLK))
BENCH_UNARY(b_power15, uint32_t, in_off, uint64_t, fd_power_q15(A15, BLK))
BENCH_UNARY(b_power31, uint32_t, in_off, uint64_t, fd_power_q31(A31, BLK))
BENCH_UNARY(b_rms15, uint32_t, in_off, fm_q15, fd_rms_q15(A15, BLK))
BENCH_UNARY(b_rms31, uint32_t, in_off, fm_q31, fd_rms_q31(A31, BLK))
BENCH_UNARY(b_var15, uint32_t, in_off, fm_q15, fd_var_q15(A15, BLK))
BENCH_UNARY(b_std15, uint32_t, in_off, fm_q15, fd_std_q15(A15, BLK))
BENCH_UNARY(b_std31, uint32_t, in_off, fm_q31, fd_std_q31(A31, BLK))
BENCH_UNARY(b_max15, uint32_t, in_off, fm_q15, fd_max_q15(A15, BLK, NULL))
BENCH_UNARY(b_absmax15, uint32_t, in_off, fm_q15,
            fd_absmax_q15(A15, BLK, NULL))

/* ---- complex ---- */

BLOCK(b_cmul15, fd_cmul_q15(CA15, CB15, out.c15, BLK), 256U)
BLOCK(b_cmul31, fd_cmul_q31(CA31, CB31, out.c31, 32U), 256U)
BLOCK(b_cmag15, fd_cmag_q15(CA15, out.u16, BLK), 128U)
BLOCK(b_cmag31, fd_cmag_q31(CA31, out.u32, BLK), 256U)
BLOCK(b_cmag2_15, fd_cmag2_q15(CA15, out.u32, BLK), 256U)
static int64_t cdot[2];
BENCH_UNARY_POST(b_cdot15, uint32_t, in_off, uint32_t,
                 (fd_cdot_q15(CA15, CB15, BLK, &cdot[0], &cdot[1]), 0U),
                 h = bench_hash(h, cdot, sizeof cdot))

#ifdef BENCH_HAVE_CMSIS_DSP
BLOCK(c_add15, arm_add_q15(A15, B15, out.q15, BLK), 128U)
BLOCK(c_add31, arm_add_q31(A31, B31, out.q31, BLK), 256U)
BLOCK(c_mul15, arm_mult_q15(A15, B15, out.q15, BLK), 128U)
BLOCK(c_mul31, arm_mult_q31(A31, B31, out.q31, BLK), 256U)
BLOCK(c_scale15, arm_scale_q15(A15, 0x6000, 1, out.q15, BLK), 128U)
BLOCK(c_scale31, arm_scale_q31(A31, 0x60000000, 1, out.q31, BLK), 256U)
BLOCK(c_shift15, arm_shift_q15(A15, -3, out.q15, BLK), 128U)
BLOCK(c_offset15, arm_offset_q15(A15, 0x1234, out.q15, BLK), 128U)
BLOCK(c_negate15, arm_negate_q15(A15, out.q15, BLK), 128U)
BLOCK(c_abs15, arm_abs_q15(A15, out.q15, BLK), 128U)
BLOCK(c_clip15, arm_clip_q15(A15, out.q15, -16384, 16383, BLK), 128U)
BLOCK(c_fill15, arm_fill_q15((q15_t)x, out.q15, BLK), 128U)
BLOCK(c_copy15, arm_copy_q15(A15, out.q15, BLK), 128U)
BLOCK(c_q31_to_q15, arm_q31_to_q15(A31, out.q15, BLK), 128U)
BLOCK(c_q15_to_f32, arm_q15_to_float(A15, out.f, BLK), 256U)
BLOCK(c_f32_to_q15, arm_float_to_q15(pf, out.q15, BLK), 128U)
BLOCK(c_mean15, arm_mean_q15(A15, BLK, out.q15), 2U)
BLOCK(c_dot15, arm_dot_prod_q15(A15, B15, BLK, (q63_t *)out.u32), 8U)
BLOCK(c_dot31, arm_dot_prod_q31(A31, B31, BLK, (q63_t *)out.u32), 8U)
BLOCK(c_power15, arm_power_q15(A15, BLK, (q63_t *)out.u32), 8U)
BLOCK(c_power31, arm_power_q31(A31, BLK, (q63_t *)out.u32), 8U)
BLOCK(c_rms15, arm_rms_q15(A15, BLK, out.q15), 2U)
BLOCK(c_rms31, arm_rms_q31(A31, BLK, out.q31), 4U)
BLOCK(c_var15, arm_var_q15(A15, BLK, out.q15), 2U)
BLOCK(c_std15, arm_std_q15(A15, BLK, out.q15), 2U)
BLOCK(c_max15, arm_max_q15(A15, BLK, out.q15, &out.u32[1]), 8U)
BLOCK(c_absmax15, arm_absmax_q15(A15, BLK, out.q15, &out.u32[1]), 8U)
BLOCK(c_cmul15,
      arm_cmplx_mult_cmplx_q15((const q15_t *)CA15, (const q15_t *)CB15,
                               out.q15, BLK),
      256U)
BLOCK(c_cmul31,
      arm_cmplx_mult_cmplx_q31((const q31_t *)CA31, (const q31_t *)CB31,
                               out.q31, 32U),
      256U)
BLOCK(c_cmag15, arm_cmplx_mag_q15((const q15_t *)CA15, out.q15, BLK), 128U)
BLOCK(c_cmag31, arm_cmplx_mag_q31((const q31_t *)CA31, out.q31, BLK), 256U)
BLOCK(c_cmag2_15,
      arm_cmplx_mag_squared_q15((const q15_t *)CA15, out.q15, BLK), 128U)
#endif

static const BenchCase cases[] = {
    {"fd_add_q15, 64", "flexlib", b_add15, true},
    {"add q15 word pairs, 64", "candidate", b_add15_pairs, true},
    {"fd_add_q31, 64", "flexlib", b_add31, true},
    {"fd_mul_q15, 64", "flexlib", b_mul15, true},
    {"mul q15 word pairs, 64", "candidate", b_mul15_pairs, true},
    {"fd_mul_q31, 64", "flexlib", b_mul31, true},
    {"fd_scale_q15, 64", "flexlib", b_scale15, true},
    {"fd_scale_q31, 64", "flexlib", b_scale31, true},
    {"fd_shift_q15 -3, 64", "flexlib", b_shift15, true},
    {"fd_offset_q15, 64", "flexlib", b_offset15, true},
    {"fd_negate_q15, 64", "flexlib", b_negate15, true},
    {"fd_abs_q15, 64", "flexlib", b_abs15, true},
    {"fd_clip_q15, 64", "flexlib", b_clip15, true},
    {"fd_fill_q15, 64", "flexlib", b_fill15, true},
    {"fd_copy_q15, 64", "flexlib", b_copy15, true},
    {"memcpy 128 B", "newlib", b_copy15_memcpy, true},
    {"fd_q31_to_q15, 64", "flexlib", b_q31_to_q15, true},
    {"fd_q15_to_f32, 64", "flexlib", b_q15_to_f32, true},
    {"fd_f32_to_q15, 64", "flexlib", b_f32_to_q15, true},
    {"fd_db_q15, 64", "flexlib", b_db15, true},
    {"fd_sum_q15, 64", "flexlib", b_sum15, true},
    {"fd_mean_q15, 64", "flexlib", b_mean15, true},
    {"fd_dot_q15, 64", "flexlib", b_dot15, true},
    {"dot q15 MUL + 64-bit add, 64", "candidate", b_dot15_mul_add, true},
    {"fd_dot_q31, 64", "flexlib", b_dot31, true},
    {"fd_power_q15, 64", "flexlib", b_power15, true},
    {"fd_power_q31, 64", "flexlib", b_power31, true},
    {"fd_rms_q15, 64", "flexlib", b_rms15, true},
    {"fd_rms_q31, 64", "flexlib", b_rms31, true},
    {"fd_var_q15, 64", "flexlib", b_var15, true},
    {"fd_std_q15, 64", "flexlib", b_std15, true},
    {"fd_std_q31, 64", "flexlib", b_std31, true},
    {"fd_max_q15, 64", "flexlib", b_max15, true},
    {"fd_absmax_q15, 64", "flexlib", b_absmax15, true},
    {"fd_cmul_q15, 64", "flexlib", b_cmul15, true},
    {"fd_cmul_q31, 32", "flexlib", b_cmul31, true},
    {"fd_cmag_q15, 64", "flexlib", b_cmag15, true},
    {"fd_cmag_q31, 64", "flexlib", b_cmag31, true},
    {"fd_cmag2_q15, 64", "flexlib", b_cmag2_15, true},
    {"fd_cdot_q15, 64", "flexlib", b_cdot15, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_add_q15, 64", "CMSIS-DSP", c_add15, false},
    {"arm_add_q31, 64", "CMSIS-DSP", c_add31, false},
    {"arm_mult_q15, 64", "CMSIS-DSP", c_mul15, false},
    {"arm_mult_q31, 64", "CMSIS-DSP", c_mul31, false},
    {"arm_scale_q15, 64", "CMSIS-DSP", c_scale15, false},
    {"arm_scale_q31, 64", "CMSIS-DSP", c_scale31, false},
    {"arm_shift_q15 -3, 64", "CMSIS-DSP", c_shift15, false},
    {"arm_offset_q15, 64", "CMSIS-DSP", c_offset15, false},
    {"arm_negate_q15, 64", "CMSIS-DSP", c_negate15, false},
    {"arm_abs_q15, 64", "CMSIS-DSP", c_abs15, false},
    {"arm_clip_q15, 64", "CMSIS-DSP", c_clip15, false},
    {"arm_fill_q15, 64", "CMSIS-DSP", c_fill15, false},
    {"arm_copy_q15, 64", "CMSIS-DSP", c_copy15, false},
    {"arm_q31_to_q15, 64", "CMSIS-DSP", c_q31_to_q15, false},
    {"arm_q15_to_float, 64", "CMSIS-DSP", c_q15_to_f32, false},
    {"arm_float_to_q15, 64", "CMSIS-DSP", c_f32_to_q15, false},
    {"arm_mean_q15, 64", "CMSIS-DSP", c_mean15, false},
    {"arm_dot_prod_q15, 64", "CMSIS-DSP", c_dot15, false},
    {"arm_dot_prod_q31, 64", "CMSIS-DSP", c_dot31, false},
    {"arm_power_q15, 64", "CMSIS-DSP", c_power15, false},
    {"arm_power_q31, 64", "CMSIS-DSP", c_power31, false},
    {"arm_rms_q15, 64", "CMSIS-DSP", c_rms15, false},
    {"arm_rms_q31, 64", "CMSIS-DSP", c_rms31, false},
    {"arm_var_q15, 64", "CMSIS-DSP", c_var15, false},
    {"arm_std_q15, 64", "CMSIS-DSP", c_std15, false},
    {"arm_max_q15, 64", "CMSIS-DSP", c_max15, false},
    {"arm_absmax_q15, 64", "CMSIS-DSP", c_absmax15, false},
    {"arm_cmplx_mult_cmplx_q15, 64", "CMSIS-DSP", c_cmul15, false},
    {"arm_cmplx_mult_cmplx_q31, 32", "CMSIS-DSP", c_cmul31, false},
    {"arm_cmplx_mag_q15, 64", "CMSIS-DSP", c_cmag15, false},
    {"arm_cmplx_mag_q31, 64", "CMSIS-DSP", c_cmag31, false},
    {"arm_cmplx_mag_squared_q15, 64", "CMSIS-DSP", c_cmag2_15, false},
#endif
};

const BenchGroup bench_group_fd_vec = {cases, BENCH_COUNT(cases)};
