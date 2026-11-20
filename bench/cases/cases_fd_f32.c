/**
 * @file cases_fd_f32.c
 * @brief Bench cases for float blocks: the block floating point conversions,
 * and the CMSIS-DSP f32 functions the float forms replace
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_bfp.h"
#include "fx_arch.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#define NB 256U /* conversion block */
#define T 32U
#define BLK 64U
#define SEC 4U
#define POOL 512U

static float pool[POOL];        /* uniform in (-1, 1) */
static int32_t pool31[NB + 64U]; /* pool at block exponent 0 */

/* outputs, and the transforms' in-place blocks */
typedef union {
  float f[2U * POOL];
  int32_t q[NB];
} Work;
_Static_assert(sizeof(Work) <= BENCH_SCRATCH_SIZE, "work in the scratch");
#define work (*(Work *)(void *)bench_scratch)

static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x2468ACEU;
  for (unsigned i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    pool[i] = (float)(int32_t)s * 0x1p-31f;
  }
  fd_bfp_to_q31(pool, pool31, NB + 64U, 0);
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

#define OUT(bytes) h = bench_hash(h, &work, (bytes))

#define BLOCK(name, call, bytes, before)                                       \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes),  \
                  before)

/* every |x| < 1: block exponent 0 for any offset */
BENCH_UNARY(b_exp, uint32_t, in_off, int32_t, fd_bfp_exp(pool + x, NB))
BLOCK(b_to_q31, fd_bfp_to_q31(pool + x, work.q, NB, 0), NB * 4U, (void)0)
BLOCK(b_to_f32, fd_bfp_to_f32(pool31 + x, work.f, NB, 0), NB * 4U, (void)0)

#ifdef BENCH_HAVE_CMSIS_DSP
static float hf[T], fir_st[T - 1U + BLK];
static arm_fir_instance_f32 cfir;
/* four second-order lowpass sections, CMSIS's sign for a1, a2 */
static const float bq_c[5U * SEC] = {
    0.0675f, 0.135f,  0.0675f, 1.1430f, -0.4128f, 0.0675f, 0.135f,
    0.0675f, 1.1430f, -0.4128f, 0.0675f, 0.135f,  0.0675f, 1.1430f,
    -0.4128f, 0.0675f, 0.135f,  0.0675f, 1.1430f, -0.4128f};
static float bq_st[2U * SEC];
static arm_biquad_cascade_df2T_instance_f32 cbq;
static arm_rfft_fast_instance_f32 crfft;

static void cmsis_init(void) {
  static bool done;
  if (!done) {
    done = true;
    setup();
    for (unsigned i = 0U; i < T; i++) {
      hf[i] = pool[i] * (1.0f / 16.0f);
    }
    arm_fir_init_f32(&cfir, T, hf, fir_st, BLK);
    arm_biquad_cascade_df2T_init_f32(&cbq, SEC, bq_c, bq_st);
    (void)arm_rfft_fast_init_512_f32(&crfft);
  }
}

static inline uint32_t in_off_c(uint32_t u) {
  cmsis_init();
  return u % 64U;
}

static void cprep(void) {
  memset(fir_st, 0, sizeof fir_st);
  memset(bq_st, 0, sizeof bq_st);
}

/* the transforms work in place: a fresh copy before each sample */
static void cload(void) { memcpy(work.f, pool, sizeof pool); }

#define CBLOCK(name, call, bytes, before)                                      \
  BENCH_UNARY_DEF(name, uint32_t, in_off_c, uint32_t, ((call), 0U),            \
                  OUT(bytes), before)

FX_INLINE float dot(const float *a, const float *b) {
  float r;
  arm_dot_prod_f32(a, b, BLK, &r);
  return r;
}
FX_INLINE float power(const float *a) {
  float r;
  arm_power_f32(a, BLK, &r);
  return r;
}

CBLOCK(c_fir, arm_fir_f32(&cfir, pool + x, work.f, BLK), BLK * 4U, cprep())
CBLOCK(c_biquad, arm_biquad_cascade_df2T_f32(&cbq, pool + x, work.f, BLK),
       BLK * 4U, cprep())
CBLOCK(c_cfft, arm_cfft_f32(&arm_cfft_sR_f32_len256, work.f, 0U, 1U),
       2U * 256U * 4U, cload())
CBLOCK(c_rfft, arm_rfft_fast_f32(&crfft, work.f, work.f + POOL, 0U),
       POOL * 4U, cload())
BENCH_UNARY(c_dot, uint32_t, in_off_c, float, dot(pool + x, pool + 64U + x))
BENCH_UNARY(c_power, uint32_t, in_off_c, float, power(pool + x))
#endif

static const BenchCase cases[] = {
    {"fd_bfp_exp, 256 floats", "flexlib", b_exp, true},
    {"fd_bfp_to_q31, 256 floats", "flexlib", b_to_q31, true},
    {"fd_bfp_to_f32, 256 floats", "flexlib", b_to_f32, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_fir_f32, 32 taps x 64", "CMSIS-DSP", c_fir, false},
    {"arm_biquad_cascade_df2T_f32, 4 x 64", "CMSIS-DSP", c_biquad, false},
    {"arm_cfft_f32, 256", "CMSIS-DSP", c_cfft, false},
    {"arm_rfft_fast_f32, 512", "CMSIS-DSP", c_rfft, false},
    {"arm_dot_prod_f32, 64", "CMSIS-DSP", c_dot, false},
    {"arm_power_f32, 64", "CMSIS-DSP", c_power, false},
#endif
};

const BenchGroup bench_group_fd_f32 = {cases, BENCH_COUNT(cases)};
