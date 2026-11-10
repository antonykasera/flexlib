/**
 * @file cases_fd_tone.c
 * @brief Bench cases for fd_window.h, fd_tone.h and fd_noise.h
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fd_noise.h"
#include "fd_tone.h"
#include "fd_window.h"
#include "fm_trig.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define WN 256U
#define BLK 64U
#define POOL 128U

static fm_q15 x15[POOL];
static fm_q31 x31[POOL];
static union {
  fm_q15 q15[WN];
  fm_q31 q31[WN];
  float f[WN];
} out;
static fm_q15 out2[BLK];
static FD_Goertzel g15, g31;
static FD_Nco nco;
static FD_Osc osc;
static FU_Xoshiro128 rng;

static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x13579BDU;
  for (uint32_t i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    x31[i] = (fm_q31)s >> 4;
    x15[i] = (fm_q15)((int32_t)s >> 20);
  }
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

/* untimed, before each sample: the same state every run */
static void prep(void) {
  fd_goertzel_init(&g15, 0x0B000000U, 4U);
  fd_goertzel_init(&g31, 0x0B000000U, 6U);
  fd_nco_init(&nco, 0U, 0x0123456BU, 0);
  fd_osc_init(&osc, 0U, 0x0123456BU);
  fu_xoshiro128_seed(&rng, 7U);
}

#define OUT(bytes) h = bench_hash(h, &out, (bytes))
#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes),  \
                  prep())

/* ---- candidates ---- */

/* a 16-bit phase into fm_sin_q15: up to 1.6 LSB from the truncated phase */
BENCH_CALL static void nco_q15_short(fm_q15 *y, uint32_t n) {
  uint32_t ph = nco.phase;
  for (uint32_t i = 0U; i < n; i++) {
    y[i] = fm_sin_q15((uint16_t)((ph + 0x8000U) >> 16));
    ph += nco.inc;
  }
  nco.phase = ph;
}

/* sum of 12 uniforms (Irwin-Hall): unit variance, tails cut at 6 sigma */
BENCH_CALL static void gauss_clt_q15(fm_q15 sigma, fm_q15 *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    int32_t s = 0;
    for (uint32_t j = 0U; j < 6U; j++) {
      uint32_t u = fu_xoshiro128ss(&rng);
      s += (int32_t)(u & 0xFFFFU) + (int32_t)(u >> 16);
    }
    int32_t g = s - 6 * 65536 + 6; /* sum - 6, Q16 */
    int64_t v = ((int64_t)g * sigma + 0x8000) >> 16;
    y[i] = v > 32767 ? 32767 : (v < -32768 ? -32768 : (fm_q15)v);
  }
}

BLOCK(b_win_hann15, fd_window_q15(out.q15, WN, FD_WIN_HANN, true), WN * 2U)
BLOCK(b_win_bh31, fd_window_q31(out.q31, WN, FD_WIN_BLACKMAN_HARRIS, true),
      WN * 4U)
BLOCK(b_win_kaiser15, fd_window_kaiser_q15(out.q15, WN, 8.0f, true), WN * 2U)
BLOCK(b_goertzel15, fd_goertzel_q15(&g15, x15 + x, BLK),
      (out.q31[0] = g15.s1, out.q31[1] = g15.s2, 8U))
BLOCK(b_goertzel31, fd_goertzel_q31(&g31, x31 + x, BLK),
      (out.q31[0] = g31.s1, out.q31[1] = g31.s2, 8U))
BLOCK(b_nco15, fd_nco_q15(&nco, out.q15, NULL, BLK), BLK * 2U)
BLOCK(b_nco15_short, nco_q15_short(out.q15, BLK), BLK * 2U)
BLOCK(b_nco31, fd_nco_q31(&nco, out.q31, out.q31 + BLK, BLK), BLK * 8U)
BLOCK(b_osc15, fd_osc_q15(&osc, out.q15, NULL, BLK), BLK * 2U)
BLOCK(b_osc31, fd_osc_q31(&osc, out.q31, out.q31 + BLK, BLK), BLK * 8U)
BLOCK(b_uni15, fd_noise_uniform_q15(&rng, out.q15, BLK), BLK * 2U)
BLOCK(b_gauss15, fd_noise_gauss_q15(&rng, 8192, out.q15, BLK), BLK * 2U)
BLOCK(b_gauss15_clt, gauss_clt_q15(8192, out.q15, BLK), BLK * 2U)
BLOCK(b_tpdf15, fd_noise_tpdf_q15(&rng, 4U, out2, BLK),
      (memcpy(out.q15, out2, sizeof out2), BLK * 2U))

#ifdef BENCH_HAVE_CMSIS_DSP
BLOCK(c_hann, arm_hanning_f32(out.f, WN), 0U)
#endif

static const BenchCase cases[] = {
    {"fd_window_q15 Hann, 256", "flexlib", b_win_hann15, true},
    {"fd_window_q31 Blackman-Harris, 256", "flexlib", b_win_bh31, true},
    {"fd_window_kaiser_q15 beta 8, 256", "flexlib", b_win_kaiser15, true},
    {"fd_goertzel_q15, 64", "flexlib", b_goertzel15, true},
    {"fd_goertzel_q31, 64", "flexlib", b_goertzel31, true},
    {"fd_nco_q15 sin, 64", "flexlib", b_nco15, true},
    {"nco q15, 16-bit phase into fm_sin_q15", "candidate", b_nco15_short, true},
    {"fd_nco_q31 sin + cos, 64", "flexlib", b_nco31, true},
    {"fd_osc_q15 sin, 64", "flexlib", b_osc15, true},
    {"fd_osc_q31 sin + cos, 64", "flexlib", b_osc31, true},
    {"fd_noise_uniform_q15, 64", "flexlib", b_uni15, true},
    {"fd_noise_gauss_q15 (Box-Muller), 64", "flexlib", b_gauss15, true},
    {"gauss q15, sum of 12 uniforms", "candidate", b_gauss15_clt, true},
    {"fd_noise_tpdf_q15, 64", "flexlib", b_tpdf15, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_hanning_f32, 256", "CMSIS-DSP", c_hann, false},
#endif
};

const BenchGroup bench_group_fd_tone = {cases, BENCH_COUNT(cases)};
