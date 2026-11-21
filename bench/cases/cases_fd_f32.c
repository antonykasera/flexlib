/**
 * @file cases_fd_f32.c
 * @brief Bench cases for float blocks and CMSIS-DSP's f32 functions
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_bfp.h"
#include "fd_fft.h"
#include "fd_fir.h"
#include "fd_vec.h"
#include "fm_sum.h"
#include "fx_arch.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#define NB 256U
#define T 32U
#define BLK 64U
#define SEC 4U
#define POOL 512U

static float pool[POOL]; /* (-1, 1) */
static int32_t pool31[NB + 64U];

typedef union {
  float f[2U * POOL];
  int32_t q[NB];
  struct {
    float x[POOL];
    int32_t w[POOL];
  } t;
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

/* fd_fir_f32 (A) and the exact sum per product (B) */

static float hf[T];
static fm_q31 fhq[T], fst[T - 1U + BLK];
static float fxs[T - 1U + BLK];
static FD_FirF32 fir;
static int32_t bhm[T], bhe[T], bxm[T - 1U + BLK], bxe[T - 1U + BLK];

static void fir_init(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  setup();
  for (unsigned i = 0U; i < T; i++) {
    hf[i] = pool[i] * (1.0f / 16.0f);
  }
  fd_fir_init_f32(&fir, hf, fhq, T, fxs, fst, FD_FIR_STATE_LEN(T, BLK));
  for (unsigned k = 0U; k < T; k++) { /* oldest first */
    uint32_t b, bad = 0U;
    memcpy(&b, &hf[T - 1U - k], sizeof b);
    bhm[k] = fm_unp_f32(b, 5U, &bhe[k], &bad);
  }
}

static inline uint32_t in_off_f(uint32_t u) {
  fir_init();
  return u % 64U;
}

/* steady state: |x| < 1 settles at E = 0 */
static void steady(void) {
  fd_fir_reset_f32(&fir);
  memset(fst, 0, sizeof fst); /* reset clears only the history */
  fir.es = 0;
  fir.since = 0U;
}

static void fprep(void) {
  steady();
  fir.fir.w = fir.fir.len;
  memset(bxm, 0, sizeof bxm);
  memset(bxe, 0, sizeof bxe);
}

static void fprep_one(void) {
  steady();
  fir.fir.w = fir.fir.len - 1U;
}

/* 5 + 4 bits of headroom: 32 products fit 64 bits */
BENCH_CALL static void fir_b(const float *x, float *y) {
  int32_t *m = bxm, *e = bxe;
  FX_OPAQUE(m);
  for (uint32_t i = 0U; i < T - 1U; i++) {
    m[i] = m[i + BLK];
    e[i] = e[i + BLK];
  }
  for (uint32_t i = 0U; i < BLK; i++) {
    uint32_t b, bad = 0U;
    memcpy(&b, &x[i], sizeof b);
    m[T - 1U + i] = fm_unp_f32(b, 4U, &e[T - 1U + i], &bad);
  }
  for (uint32_t i = 0U; i < BLK; i++) {
    int32_t em;
    int64_t s = fm_sum_f32_s(bhm, bhe, m + i, e + i, 1U, T, &em);
    uint32_t b = fm_f32_of_sum(s, em + 4);
    memcpy(&y[i], &b, sizeof b);
  }
}

#define FBLOCK(name, call)                                                     \
  BENCH_UNARY_DEF(name, uint32_t, in_off_f, uint32_t, ((call), 0U),            \
                  OUT(BLK * 4U), fprep())

FBLOCK(b_fir, fd_fir_f32(&fir, pool + x, work.f, BLK))
FBLOCK(b_fir_b, fir_b(pool + x, work.f))
BENCH_UNARY_DEF(b_fir_sample, uint32_t, in_off_f, float,
                fd_fir_sample_f32(&fir, pool[x]), (void)0, fprep_one())

BENCH_UNARY(b_exp, uint32_t, in_off, int32_t, fd_bfp_exp(pool + x, NB))
BLOCK(b_to_q31, fd_bfp_to_q31(pool + x, work.q, NB, 0), NB * 4U, (void)0)
BLOCK(b_to_f32, fd_bfp_to_f32(pool31 + x, work.f, NB, 0), NB * 4U, (void)0)

#ifdef BENCH_HAVE_CMSIS_DSP
static float fir_st[T - 1U + BLK];
static arm_fir_instance_f32 cfir;
/* CMSIS's sign for a1, a2 */
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
    fir_init();
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
FX_INLINE float mean(const float *a) {
  float r;
  arm_mean_f32(a, BLK, &r);
  return r;
}
FX_INLINE float rms(const float *a) {
  float r;
  arm_rms_f32(a, BLK, &r);
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
BENCH_UNARY(c_mean, uint32_t, in_off_c, float, mean(pool + x))
BENCH_UNARY(c_rms, uint32_t, in_off_c, float, rms(pool + x))
#endif

static void tload(void) { memcpy(work.t.x, pool, sizeof pool); }

#define TBLOCK(name, call)                                                     \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U),              \
                  h = bench_hash(h, work.t.x, sizeof work.t.x), tload())

TBLOCK(b_cfft, fd_cfft_f32((fd_cf32 *)(void *)work.t.x, 256U,
                           (fd_cq31 *)(void *)work.t.w))
TBLOCK(b_icfft, fd_icfft_f32((fd_cf32 *)(void *)work.t.x, 256U,
                             (fd_cq31 *)(void *)work.t.w))
TBLOCK(b_rfft, fd_rfft_f32(work.t.x, 512U, work.t.w))
TBLOCK(b_irfft, fd_irfft_f32(work.t.x, 512U, work.t.w))
BENCH_UNARY(b_dot, uint32_t, in_off, float,
            fd_dot_f32(pool + x, pool + 64U + x, BLK))
BENCH_UNARY(b_power, uint32_t, in_off, float, fd_power_f32(pool + x, BLK))
BENCH_UNARY(b_sum, uint32_t, in_off, float, fd_sum_f32(pool + x, BLK))
BENCH_UNARY(b_mean, uint32_t, in_off, float, fd_mean_f32(pool + x, BLK))
BENCH_UNARY(b_rms, uint32_t, in_off, float, fd_rms_f32(pool + x, BLK))

static const BenchCase cases[] = {
    {"fd_bfp_exp, 256 floats", "flexlib", b_exp, true},
    {"fd_bfp_to_q31, 256 floats", "flexlib", b_to_q31, true},
    {"fd_bfp_to_f32, 256 floats", "flexlib", b_to_f32, true},
    {"fd_fir_f32, 32 taps x 64", "flexlib", b_fir, true},
    {"fir f32 exact sum per product (B)", "candidate", b_fir_b, true},
    {"fd_fir_sample_f32, 32 taps", "flexlib", b_fir_sample, true},
    {"fd_cfft_f32, 256", "flexlib", b_cfft, true},
    {"fd_icfft_f32, 256", "flexlib", b_icfft, true},
    {"fd_rfft_f32, 512", "flexlib", b_rfft, true},
    {"fd_irfft_f32, 512", "flexlib", b_irfft, true},
    {"fd_dot_f32, 64", "flexlib", b_dot, true},
    {"fd_power_f32, 64", "flexlib", b_power, true},
    {"fd_sum_f32, 64", "flexlib", b_sum, true},
    {"fd_mean_f32, 64", "flexlib", b_mean, true},
    {"fd_rms_f32, 64", "flexlib", b_rms, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_fir_f32, 32 taps x 64", "CMSIS-DSP", c_fir, false},
    {"arm_biquad_cascade_df2T_f32, 4 x 64", "CMSIS-DSP", c_biquad, false},
    {"arm_cfft_f32, 256", "CMSIS-DSP", c_cfft, false},
    {"arm_rfft_fast_f32, 512", "CMSIS-DSP", c_rfft, false},
    {"arm_dot_prod_f32, 64", "CMSIS-DSP", c_dot, false},
    {"arm_power_f32, 64", "CMSIS-DSP", c_power, false},
    {"arm_mean_f32, 64", "CMSIS-DSP", c_mean, false},
    {"arm_rms_f32, 64", "CMSIS-DSP", c_rms, false},
#endif
};

const BenchGroup bench_group_fd_f32 = {cases, BENCH_COUNT(cases)};
