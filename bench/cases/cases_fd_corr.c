/**
 * @file cases_fd_corr.c
 * @brief Bench cases for convolution, correlation, LMS and Levinson
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_corr.h"
#include "fd_fir_core.h"
#include "fd_lms.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define NA 256U
#define POOL (NA + 64U)
#define NB 64U
#define LT 32U  /* LMS taps */
#define LB 64U  /* LMS block */
#define ORD 10U /* Levinson order */

/* a, c: full-scale noise; b: sum |b| < 2 */
static fm_q15 a15[POOL], c15[POOL], b15[NB], d15[POOL], a12[POOL], c12[POOL];
static fm_q31 a31[POOL / 2U], b31[NB], b31f[NB];
static fm_q31 r31[ORD + 1U], lev_a[ORD], lev_k[ORD];
static fm_q31 ac[16];
static int32_t lw[LT];
static fm_q15 ls15[FD_LMS_STATE_LEN(LT, LB)];
static FD_LmsQ15 lms15;
static FD_LmsQ31 lms31;
static fm_q31 ls31[FD_LMS_STATE_LEN(LT, LB)], a31d[POOL / 2U];
_Static_assert(BENCH_SCRATCH_SIZE >= 3072U, "two 512-point blocks, the output");

/* the outputs, after the FFT case's two blocks in the scratch */
typedef union {
  fm_q15 q15[2U * NA];
  fm_q31 q31[NA];
} Out;
#define out (*(Out *)(void *)(bench_scratch + 2048))

static uint32_t rnd(uint32_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x6C078965U;
  for (unsigned i = 0U; i < POOL; i++) {
    a15[i] = (fm_q15)(rnd(&s) >> 16);
    c15[i] = (fm_q15)(rnd(&s) >> 16);
    a12[i] = (fm_q15)(a15[i] / 16); /* a 12-bit ADC's range */
    c12[i] = (fm_q15)(c15[i] / 16);
  }
  for (unsigned i = 0U; i < POOL / 2U; i++) {
    a31[i] = (fm_q31)rnd(&s);
  }
  for (unsigned i = 0U; i < NB; i++) {
    b15[i] = (fm_q15)((int32_t)(rnd(&s) >> 16) % 1000);
    b31[i] = (fm_q31)((int32_t)rnd(&s) / 40);
    b31f[i] = (fm_q31)(uint32_t)((uint32_t)b31[i] & 0xFFFF0000U);
  }
  for (unsigned i = 2U; i < POOL / 2U; i++) {
    a31d[i] = a31[i - 1U] / 2 + a31[i - 2U] / 4;
  }
  /* the LMS target: x through a short filter */
  for (unsigned i = 2U; i < POOL; i++) {
    d15[i] = (fm_q15)((a15[i - 1U] + a15[i - 2U] / 2) / 2);
  }
  /* an autocorrelation (lags 0..ORD) of a smoothed block, r[0] near 2^30 */
  static int64_t acc[ORD + 1U];
  for (unsigned k = 0U; k <= ORD; k++) {
    for (unsigned j = 0U; j + k < NA; j++) {
      acc[k] += (int64_t)d15[j + 2U] * d15[j + 2U + k];
    }
  }
  int sh = 0;
  while ((acc[0] >> sh) >= (1LL << 30)) {
    sh++;
  }
  for (unsigned k = 0U; k <= ORD; k++) {
    r31[k] = (fm_q31)(acc[k] >> sh);
  }
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

#define OUT(bytes) h = bench_hash(h, &out, (bytes))
#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes),  \
                  (void)0)

/* the interior of a convolution: the FIR kernel over the input, no state */
BENCH_CALL static void conv15_inner(const fm_q15 *a, fm_q15 *y) {
  k15(b15, 1U, false, 32U, a, y, 1U, NA - 31U, 0U, 0U, 0U, NULL, 0);
}

BENCH_CALL static void conv31_inner(const fm_q31 *a, fm_q31 *y) {
  k31(b31, 1U, false, 32U, a, y, 1U, 97U, 0U, 0U, 0U, NULL, 0);
}

BENCH_CALL static void conv31_inner_f(const fm_q31 *a, fm_q31 *y) {
  k31(b31f, 1U, false, 32U, a, y, 1U, 97U, 0U, 0U, 0U, NULL, 0);
}

/* the 64-bit kernel over full-scale coefficients, and two candidates */
BENCH_CALL static void conv15w_inner(const fm_q15 *a, fm_q15 *y) {
  k15w2(c15, 1U, false, 32U, a, y, 1U, NA - 31U, 0U, 0U, 0U, NULL, 0);
}

/* three outputs per pass */
BENCH_CALL static void conv15w_three(const fm_q15 *a, fm_q15 *y) {
  for (uint32_t m = 0U; m + 3U <= NA - 31U; m += 3U) {
    int64_t a0 = 0, a1 = 0, a2 = 0;
    int32_t x0 = a[m], x1 = a[m + 1U], x2;
    const fm_q15 *bp = a + m + 2U;
    for (uint32_t k = 32U; k != 0U; k--) {
      int32_t c = c15[k - 1U];
      x2 = *bp++;
      a0 = fx_smlal(a0, c, x0);
      a1 = fx_smlal(a1, c, x1);
      a2 = fx_smlal(a2, c, x2);
      x0 = x1;
      x1 = x2;
    }
    y[m] = out15w(a0);
    y[m + 1U] = out15w(a1);
    y[m + 2U] = out15w(a2);
  }
}

/* two per pass, each product a 32-bit MUL added as 64 bits */
BENCH_CALL static void conv15w_mul(const fm_q15 *a, fm_q15 *y) {
  for (uint32_t m = 0U; m + 2U <= NA - 31U; m += 2U) {
    int64_t a0 = 0, a1 = 0;
    int32_t x0 = a[m], x1;
    const fm_q15 *bp = a + m + 1U;
    for (uint32_t k = 32U; k != 0U; k -= 2U) {
      int32_t c = c15[k - 1U];
      x1 = *bp++;
      a0 += c * x0;
      a1 += c * x1;
      c = c15[k - 2U];
      x0 = *bp++;
      a0 += c * x1;
      a1 += c * x0;
    }
    y[m] = out15w(a0);
    y[m + 1U] = out15w(a1);
  }
}

BLOCK(b_conv15_inner, conv15_inner(a15 + x, out.q15), (NA - 31U) * 2U)
BLOCK(b_conv31_inner, conv31_inner(a31 + x, out.q31), 97U * 4U)
BLOCK(b_conv31_inner_f, conv31_inner_f(a31 + x, out.q31), 97U * 4U)
BLOCK(b_conv15w_inner, conv15w_inner(a15 + x, out.q15), 224U * 2U)
BLOCK(b_conv15w_three, conv15w_three(a15 + x, out.q15), 222U * 2U)
BLOCK(b_conv15w_mul, conv15w_mul(a15 + x, out.q15), 224U * 2U)
BLOCK(b_conv15_64, fd_conv_q15(a15 + x, NB, b15, NB, 0U, out.q15), 254U)
BLOCK(b_conv15_256, fd_conv_q15(a15 + x, NA, b15, 32U, 0U, out.q15), 574U)
BLOCK(b_conv15_wide, fd_conv_q15(a15 + x, NB, c15, NB, 0U, out.q15), 254U)
BLOCK(b_conv31_64, fd_conv_q31(a31 + x, NB, b31, NB, 0U, out.q31), 508U)
BLOCK(b_conv31_wide, fd_conv_q31(a31 + x, NB, a31 + 64, NB, 0U, out.q31), 508U)
BLOCK(b_corr15_64, fd_corr_q15(a15 + x, NA, c15 + x, NA, 0, 64U, 0U, out.q15),
      128U)
BLOCK(b_corr15_32, fd_corr_q15(a15 + x, NA, c15 + x, NA, 0, 32U, 0U, out.q15),
      64U)
BLOCK(b_corr12_32, fd_corr_q15(a12 + x, NA, c12 + x, NA, 0, 32U, 0U, out.q15),
      64U)
BLOCK(b_conv31_128, fd_conv_q31(a31 + x, 128U, b31, 32U, 0U, out.q31), 636U)
BLOCK(b_ac15, fd_autocorr_q15(a15 + x, NA, ac, 16U), 64U)
BLOCK(b_ac12, fd_autocorr_q15(a12 + x, NA, ac, 16U), 64U)
BLOCK(b_ac31, fd_autocorr_q31(a31 + x, 128U, ac, 16U), 64U)
BENCH_UNARY_DEF(b_peak15, uint32_t, in_off, fm_q16,
                fd_peak_q15(c15 + x, 128U, NULL), (void)0, (void)0)
BENCH_UNARY_DEF(b_lev31, uint32_t, in_off, uint32_t,
                (uint32_t)fd_levinson_q31(r31, ORD, 4U, lev_a, lev_k, NULL),
                h = bench_hash(h, lev_a, sizeof lev_a), (void)0)
/* every sample starts from zero weights and history */
#define LMS(name, mode, mu)                                                    \
  BENCH_UNARY_DEF(                                                             \
      name, uint32_t, in_off, uint32_t,                                        \
      (fd_lms_q15(&lms15, a15 + x, d15 + x, out.q15, NULL, LB), 0U),           \
      OUT(LB * 2U),                                                            \
      fd_lms_init_q15(&lms15, lw, LT, ls15, FD_LMS_STATE_LEN(LT, LB), (mu),    \
                      (mode)))
LMS(b_lms15, FD_LMS_PLAIN, 0x0400)
LMS(b_nlms15, FD_LMS_NORM, 0x4000)
LMS(b_slms15, FD_LMS_SIGN, 0x0010)
#define LMS31(name, mode, mu)                                                  \
  BENCH_UNARY_DEF(                                                             \
      name, uint32_t, in_off, uint32_t,                                        \
      (fd_lms_q31(&lms31, a31 + x, a31d + x, out.q31, NULL, LB), 0U),          \
      OUT(LB * 4U),                                                            \
      fd_lms_init_q31(&lms31, lw, LT, ls31, FD_LMS_STATE_LEN(LT, LB), (mu),    \
                      (mode)))
LMS31(b_lms31, FD_LMS_PLAIN, 0x04000000)
LMS31(b_nlms31, FD_LMS_NORM, 0x40000000)
LMS31(b_slms31, FD_LMS_SIGN, 0x00100000)
/* both blocks zero-padded to 512, rebuilt before every sample */
static fm_q15 *const fa = (fm_q15 *)(void *)bench_scratch;
static fm_q15 *const fb = (fm_q15 *)(void *)(bench_scratch + 1024);
static void fft_fill(uint32_t x) {
  for (uint32_t i = 0U; i < 512U; i++) {
    fa[i] = i < NA ? a15[x + i] : 0;
    fb[i] = i < NA ? c15[x + i] : 0;
  }
}
BENCH_UNARY_DEF(b_corr_fft, uint32_t, in_off, uint32_t,
                (uint32_t)fd_corr_fft_q15(fa, fb, 512U),
                h = bench_hash(h, fa, 1024U), fft_fill(bench_in[i] % 64U))
BLOCK(b_corr15_all,
      fd_corr_q15(a15 + x, NA, c15 + x, NA, 1 - (int32_t)NA, 2U * NA - 1U, 0U,
                  out.q15),
      1022U)
BLOCK(b_corr31_all,
      fd_corr_q31(a31 + x, NB, b31, NB, 1 - (int32_t)NB, 2U * NB - 1U, 0U,
                  out.q31),
      508U)

#ifdef BENCH_HAVE_CMSIS_DSP
static arm_lms_instance_q15 cl15;
static arm_lms_norm_instance_q15 cn15;
static arm_lms_instance_q31 cl31;
static q15_t cw15[LT], cs15[LT + LB - 1U], cwn15[LT], csn15[LT + LB - 1U];
static q31_t cw31[LT], cs31[LT + LB - 1U];
static q15_t err15[LB];
static q31_t err31[LB];
static q31_t clev_a[ORD], lev_err;

static void cmsis_init(void) {
  static bool done;
  if (!done) {
    done = true;
    setup();
    arm_lms_init_q15(&cl15, LT, cw15, cs15, 0x0400, LB, 0U);
    arm_lms_norm_init_q15(&cn15, LT, cwn15, csn15, 0x4000, LB, 0U);
    arm_lms_init_q31(&cl31, LT, cw31, cs31, 0x04000000, LB, 0U);
  }
}

static inline uint32_t in_off_c(uint32_t u) {
  cmsis_init();
  return u % 64U;
}

/* every sample starts from zero weights and history */
static void cprep(void) {
  memset(cw15, 0, sizeof cw15);
  memset(cs15, 0, sizeof cs15);
  memset(cwn15, 0, sizeof cwn15);
  memset(csn15, 0, sizeof csn15);
  cn15.energy = 0;
  cn15.x0 = 0;
  memset(cw31, 0, sizeof cw31);
  memset(cs31, 0, sizeof cs31);
}

#define CBLOCK(name, call, bytes)                                              \
  BENCH_UNARY_DEF(name, uint32_t, in_off_c, uint32_t, ((call), 0U),            \
                  OUT(bytes), cprep())

CBLOCK(c_conv15_64, arm_conv_q15(a15 + x, NB, b15, NB, out.q15), 254U)
CBLOCK(c_conv15f_64, arm_conv_fast_q15(a15 + x, NB, b15, NB, out.q15), 254U)
CBLOCK(c_conv15_256, arm_conv_q15(a15 + x, NA, b15, 32U, out.q15), 574U)
CBLOCK(c_conv31_64, arm_conv_q31(a31 + x, NB, b31, NB, out.q31), 508U)
CBLOCK(c_corr15, arm_correlate_q15(a15 + x, NA, c15 + x, NA, out.q15), 1022U)
CBLOCK(c_corr31, arm_correlate_q31(a31 + x, NB, b31, NB, out.q31), 508U)
CBLOCK(c_lms15, arm_lms_q15(&cl15, a15 + x, d15 + x, out.q15, err15, LB),
       LB * 2U)
CBLOCK(c_nlms15, arm_lms_norm_q15(&cn15, a15 + x, d15 + x, out.q15, err15, LB),
       LB * 2U)
CBLOCK(c_lms31, arm_lms_q31(&cl31, a31 + x, a31d + x, out.q31, err31, LB),
       LB * 4U)
CBLOCK(c_lev31, arm_levinson_durbin_q31(r31, clev_a, &lev_err, (int)ORD), 0U)
#endif

static const BenchCase cases[] = {
    {"fd_conv_q15, 64 x 64", "flexlib", b_conv15_64, true},
    {"fd_conv_q15, 256 x 32", "flexlib", b_conv15_256, true},
    {"fd_conv_q15, 64 x 64, both full scale (64-bit)", "flexlib", b_conv15_wide,
     true},
    {"fd_conv_q31, 64 x 64", "flexlib", b_conv31_64, true},
    {"fd_conv_q31, 64 x 64, both full scale (96-bit)", "flexlib", b_conv31_wide,
     true},
    {"fd_corr_q15, 256 x 256, lags 0..63", "flexlib", b_corr15_64, true},
    {"fd_corr_q15, 256 x 256, lags 0..31", "flexlib", b_corr15_32, true},
    {"fd_corr_q15, 256 x 256, lags 0..31, 12-bit data", "flexlib", b_corr12_32,
     true},
    {"fd_conv_q31, 128 x 32", "flexlib", b_conv31_128, true},
    {"fd_corr_q15, 256 x 256, all lags", "flexlib", b_corr15_all, true},
    {"fd_corr_fft_q15, 256 x 256 in 512", "flexlib", b_corr_fft, true},
    {"fd_autocorr_q15, 256, 16 lags", "flexlib", b_ac15, true},
    {"fd_autocorr_q15, 256, 16 lags, 12-bit data", "flexlib", b_ac12, true},
    {"fd_autocorr_q31, 128, 16 lags", "flexlib", b_ac31, true},
    {"fd_peak_q15, 128", "flexlib", b_peak15, true},
    {"fd_levinson_q31, order 10", "flexlib", b_lev31, true},
    {"fd_corr_q31, 64 x 64, all lags", "flexlib", b_corr31_all, true},
    {"fd_lms_q15, 32 taps x 64", "flexlib", b_lms15, true},
    {"fd_lms_q15 normalised, 32 taps x 64", "flexlib", b_nlms15, true},
    {"fd_lms_q15 sign-error, 32 taps x 64", "flexlib", b_slms15, true},
    {"fd_lms_q31, 32 taps x 64", "flexlib", b_lms31, true},
    {"fd_lms_q31 normalised, 32 taps x 64", "flexlib", b_nlms31, true},
    {"fd_lms_q31 sign-error, 32 taps x 64", "flexlib", b_slms31, true},
    {"conv q15 interior, 32 x 225 (FIR kernel)", "kernel", b_conv15_inner,
     true},
    {"conv q31 interior, 32 x 97 (FIR kernel)", "kernel", b_conv31_inner, true},
    {"conv q31 interior, coefficients with zero low halves", "kernel",
     b_conv31_inner_f, true},
    {"conv q15 64-bit interior, 32 x 225", "kernel", b_conv15w_inner, true},
    {"conv q15 64-bit, three outputs per pass", "candidate", b_conv15w_three,
     true},
    {"conv q15 64-bit, MUL + ADDS / ADC", "candidate", b_conv15w_mul, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_conv_q15, 64 x 64", "CMSIS-DSP", c_conv15_64, false},
    {"arm_conv_fast_q15, 64 x 64", "CMSIS-DSP", c_conv15f_64, false},
    {"arm_conv_q15, 256 x 32", "CMSIS-DSP", c_conv15_256, false},
    {"arm_conv_q31, 64 x 64", "CMSIS-DSP", c_conv31_64, false},
    {"arm_correlate_q15, 256 x 256", "CMSIS-DSP", c_corr15, false},
    {"arm_correlate_q31, 64 x 64", "CMSIS-DSP", c_corr31, false},
    {"arm_lms_q15, 32 taps x 64", "CMSIS-DSP", c_lms15, false},
    {"arm_lms_norm_q15, 32 taps x 64", "CMSIS-DSP", c_nlms15, false},
    {"arm_lms_q31, 32 taps x 64", "CMSIS-DSP", c_lms31, false},
    {"arm_levinson_durbin_q31, order 10", "CMSIS-DSP", c_lev31, false},
#endif
};

const BenchGroup bench_group_fd_corr = {cases, BENCH_COUNT(cases)};
