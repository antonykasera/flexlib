/**
 * @file cases_fd_biquad.c
 * @brief Bench cases for fd_biquad.h and fd_filt.h: 4 sections x 64 samples
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_biquad.h"
#include "fd_filt.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define NS 4U
#define BLK 64U
#define POOL 128U

static fm_q31 c31[5U * NS], x31[POOL];
static fm_q15 c15[5U * NS], cw15[5U * NS], x15[POOL];
static uint32_t sh31, sh15;
static fm_q31 st31[FD_BIQUAD_STATE_LEN(NS)], ste31[FD_BIQUAD_STATE_LEN(NS)];
static fm_q15 st15[FD_BIQUAD_STATE_LEN(NS)], stw15[FD_BIQUAD_STATE_LEN(NS)];
static fm_q15 mb15[32];
static fm_q15 mb15b[30];
static fm_q31 mb31[32];
static FD_BiquadQ31 bq31, bqe31;
static FD_BiquadQ15 bq15, bqw15;
/* 48 kHz EQs: sum |c| 6-7 per section; the boost's high shelf needs bshift 1 */
static fm_q31 eq31[2][5U * NS];
static fm_q15 eq15[2][5U * NS];
static fm_q31 sq31[3][FD_BIQUAD_STATE_LEN(NS)];
static fm_q15 sq15[2][FD_BIQUAD_STATE_LEN(NS)];
static FD_BiquadQ31 beq31[3];
static FD_BiquadQ15 beq15[2];
static FD_DcBlockQ15 dc15;
static FD_DcBlockQ31 dc31;
static FD_OnePoleQ15 op15;
static FD_OnePoleQ31 op31;
static FD_MovAvgQ15 ma15, ma15b;
static FD_MovAvgQ31 ma31;
static fm_q15 hist15[4U * NS]; /* the sample-major candidate's own history */
static union {
  fm_q15 q15[BLK];
  fm_q31 q31[BLK];
} out;

/* 8th-order Butterworth lowpass at 0.05; the wide copy: sum |c| > 4 */
static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  FD_BiquadDesign d[NS];
  (void)fd_biquad_butter(d, 2U * NS, FD_LOWPASS, 0.05f);
  (void)fd_biquad_quantize_q31(d, NS, c31, &sh31);
  (void)fd_biquad_quantize_q15(d, NS, c15, &sh15);
  for (uint32_t i = 0U; i < 5U * NS; i++) {
    cw15[i] = (i % 5U) < 3U ? (fm_q15)(c15[i] * 4) : c15[i];
    cw15[i] = (i % 5U) == 3U ? 32767 : ((i % 5U) == 4U ? -32768 : cw15[i]);
  }
  uint32_t s = 0x2468ACEU;
  for (uint32_t i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    x31[i] = (fm_q31)s >> 1;
    x15[i] = (fm_q15)((int32_t)s >> 17);
  }
  static const float eq[2][NS][4] = {
      {{FD_BQ_LOWSHELF, 100.0f, 0.7f, 6.0f},
       {FD_BQ_PEAK, 1000.0f, 1.4f, -4.0f},
       {FD_BQ_PEAK, 3000.0f, 2.0f, 5.0f},
       {FD_BQ_HIGHSHELF, 8000.0f, 0.7f, 3.0f}},
      {{FD_BQ_LOWSHELF, 100.0f, 0.7f, 9.0f},
       {FD_BQ_PEAK, 1000.0f, 1.4f, -4.0f},
       {FD_BQ_PEAK, 3000.0f, 2.0f, 5.0f},
       {FD_BQ_HIGHSHELF, 8000.0f, 0.7f, 12.0f}}};
  for (uint32_t e = 0U; e < 2U; e++) {
    uint32_t s31, s15;
    for (uint32_t k = 0U; k < NS; k++) {
      (void)fd_biquad_design(&d[k], (FD_BiquadType)eq[e][k][0],
                             eq[e][k][1] / 48000.0f, eq[e][k][2], eq[e][k][3]);
    }
    (void)fd_biquad_quantize_q31(d, NS, eq31[e], &s31);
    (void)fd_biquad_quantize_q15(d, NS, eq15[e], &s15);
    fd_biquad_init_q31(&beq31[e], eq31[e], NS, s31, sq31[e]);
    fd_biquad_init_q15(&beq15[e], eq15[e], NS, s15, sq15[e]);
    if (e == 0U) {
      fd_biquad_init_ef_q31(&beq31[2], eq31[0], NS, s31, sq31[2]);
    }
  }
  fd_biquad_init_q31(&bq31, c31, NS, sh31, st31);
  fd_biquad_init_ef_q31(&bqe31, c31, NS, sh31, ste31);
  fd_biquad_init_q15(&bq15, c15, NS, sh15, st15);
  fd_biquad_init_q15(&bqw15, cw15, NS, sh15, stw15);
  fd_dcblock_init_q15(&dc15, 32604);
  fd_dcblock_init_q31(&dc31, 2136746229);
  fd_onepole_init_q15(&op15, 1024, 0);
  fd_onepole_init_q31(&op31, 67108864, 0);
  fd_movavg_init_q15(&ma15, mb15, 32U);
  fd_movavg_init_q15(&ma15b, mb15b, 30U);
  fd_movavg_init_q31(&ma31, mb31, 32U);
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

/* before each sample, untimed: zero state, so every run hashes the same */
static void prep(void) {
  fd_biquad_reset_q31(&bq31);
  fd_biquad_reset_q31(&bqe31);
  fd_biquad_reset_q15(&bq15);
  fd_biquad_reset_q15(&bqw15);
  for (uint32_t e = 0U; e < 3U; e++) {
    fd_biquad_reset_q31(&beq31[e]);
  }
  fd_biquad_reset_q15(&beq15[0]);
  fd_biquad_reset_q15(&beq15[1]);
  memset(hist15, 0, sizeof hist15);
  fd_dcblock_init_q15(&dc15, 32604);
  fd_dcblock_init_q31(&dc31, 2136746229);
  fd_onepole_init_q15(&op15, 1024, 0);
  fd_onepole_init_q31(&op31, 67108864, 0);
  fd_movavg_init_q15(&ma15, mb15, 32U);
  fd_movavg_init_q15(&ma15b, mb15b, 30U);
  fd_movavg_init_q31(&ma31, mb31, 32U);
}

#define X15 (x15 + x)
#define X31 (x31 + x)
#define OUT(bytes) h = bench_hash(h, &out, (bytes))
#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes),  \
                  prep())

/* every section per sample: coefficients and history reloaded each time */
BENCH_CALL static void biquad15_sample_major(const fm_q15 *x, fm_q15 *y) {
  for (uint32_t i = 0U; i < BLK; i++) {
    int32_t v = x[i];
    for (uint32_t k = 0U; k < NS; k++) {
      const fm_q15 *c = c15 + 5U * k;
      fm_q15 *h = hist15 + 4U * k;
      int32_t acc = (c[0] * v + c[1] * h[0] + c[2] * h[1]) * (1 << sh15) +
                    c[3] * h[2] + c[4] * h[3];
      int32_t o = fm_sat_q15((acc + 0x2000) >> 14);
      h[1] = h[0];
      h[0] = (fm_q15)v;
      h[3] = h[2];
      h[2] = (fm_q15)o;
      v = o;
    }
    y[i] = (fm_q15)v;
  }
}

BLOCK(b_bq15, fd_biquad_q15(&bq15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_bq15_sm, biquad15_sample_major(X15, out.q15), BLK * 2U)
BLOCK(b_bqw15, fd_biquad_q15(&bqw15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_bq31, fd_biquad_q31(&bq31, X31, out.q31, BLK), BLK * 4U)
BLOCK(b_bqe31, fd_biquad_q31(&bqe31, X31, out.q31, BLK), BLK * 4U)
BLOCK(b_eq31, fd_biquad_q31(&beq31[0], X31, out.q31, BLK), BLK * 4U)
BLOCK(b_eqs31, fd_biquad_q31(&beq31[1], X31, out.q31, BLK), BLK * 4U)
BLOCK(b_eqe31, fd_biquad_q31(&beq31[2], X31, out.q31, BLK), BLK * 4U)
BLOCK(b_eq15, fd_biquad_q15(&beq15[0], X15, out.q15, BLK), BLK * 2U)
BLOCK(b_eqs15, fd_biquad_q15(&beq15[1], X15, out.q15, BLK), BLK * 2U)
BLOCK(b_dc15, fd_dcblock_q15(&dc15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_dc31, fd_dcblock_q31(&dc31, X31, out.q31, BLK), BLK * 4U)
BLOCK(b_op15, fd_onepole_q15(&op15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_op31, fd_onepole_q31(&op31, X31, out.q31, BLK), BLK * 4U)
BLOCK(b_ma15, fd_movavg_q15(&ma15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_ma15b, fd_movavg_q15(&ma15b, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_ma31, fd_movavg_q31(&ma31, X31, out.q31, BLK), BLK * 4U)

static inline int32_t in_s32(uint32_t u) { return (int32_t)u; }
BENCH_UNARY(b_med5, int32_t, in_s32, int32_t,
            fd_median5(x, x >> 3, x ^ 0x5555, ~x, x * 3))

#ifdef BENCH_HAVE_CMSIS_DSP
static arm_biquad_casd_df1_inst_q15 cb15;
static arm_biquad_casd_df1_inst_q31 cb31;
static arm_biquad_cas_df1_32x64_ins_q31 cb64;
static q15_t cc15[6U * NS], cs15[4U * NS];
static q31_t cs31[4U * NS];
static q63_t cs64[4U * NS];

/* CMSIS q15 stages are b0, 0, b1, b2, a1, a2 */
static void cmsis_init(void) {
  static bool done;
  if (!done) {
    done = true;
    setup();
    for (uint32_t k = 0U; k < NS; k++) {
      const fm_q15 *c = c15 + 5U * k;
      q15_t *d = cc15 + 6U * k;
      d[0] = c[0];
      d[1] = 0;
      d[2] = c[1];
      d[3] = c[2];
      d[4] = c[3];
      d[5] = c[4];
    }
    arm_biquad_cascade_df1_init_q15(&cb15, NS, cc15, cs15, 1);
    arm_biquad_cascade_df1_init_q31(&cb31, NS, c31, cs31, 1);
    arm_biquad_cas_df1_32x64_init_q31(&cb64, NS, c31, cs64, 1);
  }
}

static inline uint32_t in_off_c(uint32_t u) {
  cmsis_init();
  return u % 64U;
}

static void cprep(void) {
  memset(cs15, 0, sizeof cs15);
  memset(cs31, 0, sizeof cs31);
  memset(cs64, 0, sizeof cs64);
}

#define CBLOCK(name, call, bytes)                                              \
  BENCH_UNARY_DEF(name, uint32_t, in_off_c, uint32_t, ((call), 0U),            \
                  OUT(bytes), cprep())

CBLOCK(c_bq15, arm_biquad_cascade_df1_q15(&cb15, X15, out.q15, BLK), BLK * 2U)
CBLOCK(c_bqf15, arm_biquad_cascade_df1_fast_q15(&cb15, X15, out.q15, BLK),
       BLK * 2U)
CBLOCK(c_bq31, arm_biquad_cascade_df1_q31(&cb31, X31, out.q31, BLK), BLK * 4U)
CBLOCK(c_bqf31, arm_biquad_cascade_df1_fast_q31(&cb31, X31, out.q31, BLK),
       BLK * 4U)
CBLOCK(c_bq64, arm_biquad_cas_df1_32x64_q31(&cb64, X31, out.q31, BLK), BLK * 4U)
#endif

static const BenchCase cases[] = {
    {"fd_biquad_q15, 4 sections x 64", "flexlib", b_bq15, true},
    {"biquad q15 sample-major", "candidate", b_bq15_sm, true},
    {"fd_biquad_q15, sum of c >= 4 (64-bit)", "flexlib", b_bqw15, true},
    {"fd_biquad_q31, 4 sections x 64", "flexlib", b_bq31, true},
    {"fd_biquad_q31 error feedback, 4 x 64", "flexlib", b_bqe31, true},
    {"fd_biquad_q15, 48 kHz EQ, 4 x 64", "flexlib", b_eq15, true},
    {"fd_biquad_q15, 48 kHz EQ, bshift 1, 4 x 64", "flexlib", b_eqs15, true},
    {"fd_biquad_q31, 48 kHz EQ, 4 x 64", "flexlib", b_eq31, true},
    {"fd_biquad_q31, 48 kHz EQ, bshift 1, 4 x 64", "flexlib", b_eqs31, true},
    {"fd_biquad_q31 error feedback, 48 kHz EQ, 4 x 64", "flexlib", b_eqe31, true},
    {"fd_dcblock_q15, 64", "flexlib", b_dc15, true},
    {"fd_dcblock_q31, 64", "flexlib", b_dc31, true},
    {"fd_onepole_q15, 64", "flexlib", b_op15, true},
    {"fd_onepole_q31, 64", "flexlib", b_op31, true},
    {"fd_movavg_q15 n = 32, 64", "flexlib", b_ma15, true},
    {"fd_movavg_q15 n = 30, 64", "flexlib", b_ma15b, true},
    {"fd_movavg_q31 n = 32, 64", "flexlib", b_ma31, true},
    {"fd_median5", "flexlib", b_med5, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_biquad_cascade_df1_q15, 4 x 64", "CMSIS-DSP", c_bq15, false},
    {"arm_biquad_cascade_df1_fast_q15, 4 x 64", "CMSIS-DSP", c_bqf15, false},
    {"arm_biquad_cascade_df1_q31, 4 x 64", "CMSIS-DSP", c_bq31, false},
    {"arm_biquad_cascade_df1_fast_q31, 4 x 64", "CMSIS-DSP", c_bqf31, false},
    {"arm_biquad_cas_df1_32x64_q31, 4 x 64", "CMSIS-DSP", c_bq64, false},
#endif
};

const BenchGroup bench_group_fd_biquad = {cases, BENCH_COUNT(cases)};
