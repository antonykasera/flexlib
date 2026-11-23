/**
 * @file cases_fd_mfcc.c
 * @brief Bench cases for fd_mfcc.h, and CMSIS-DSP's MFCC on the same filters
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_fft.h"
#include "fd_mfcc.h"
#include "fd_vec.h"
#include "fd_window.h"
#include <string.h>
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define N 512U
#define BANDS 40U
#define COEFS 10U

/* The image has no RAM left for these outside the scratch. */
#define src ((fm_q15 *)(void *)bench_scratch)
#define work ((fm_q15 *)(void *)(bench_scratch + 1024U))
#define tab ((fm_q31 *)(void *)(bench_scratch + 4096U))
#define win ((fm_q15 *)(void *)(bench_scratch + 5696U))
#define wts ((uint16_t *)(void *)(bench_scratch + 6720U))
#define cnt ((uint16_t *)(void *)(bench_scratch + 7236U))
_Static_assert(BENCH_SCRATCH_SIZE >= 8120U, "the frame's buffers and tables");

static FD_Mel mel;
static FD_Dct dct;
static uint64_t en[BANDS];
static fm_q31 logs[BANDS], mfcc[COEFS];
static int32_t e_spec, e_en;
static int scratch_id;

static uint32_t rnd(uint32_t *s) {
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

#ifdef BENCH_HAVE_CMSIS_DSP
static void c_setup(void);
#endif

static void setup(void) {
  if (!bench_scratch_claim(&scratch_id)) {
    return;
  }
  fd_window_q15(win, N, FD_WIN_HANN, true);
  (void)fd_mel_init(&mel, cnt, wts, N, 16000U, BANDS, 20U, 4000U);
  (void)fd_dct2_init(&dct, tab, BANDS, COEFS);
  uint32_t s = 0x1F2E3D4CU;
  for (uint32_t i = 0U; i < N; i++) {
    int32_t v = (int32_t)(rnd(&s) >> 22) - 512;
    v += (int32_t)(3000 * ((i * 37U) % 64U < 32U ? 1 : -1));
    v += (int32_t)((i * 9U) % 100U) * 40 - 2000;
    src[i] = (fm_q15)v;
  }
  memcpy(work, src, 2U * N);
  fd_mul_q15(work, win, work, N);
  e_spec = fd_rfft_q15(work, N);
  e_en = fd_mel_q15(&mel, work, e_spec, en);
  fd_log_q16(en, e_en, logs, BANDS);
#ifdef BENCH_HAVE_CMSIS_DSP
  c_setup();
#endif
}

static inline uint32_t in_u(uint32_t u) {
  setup();
  return u;
}

static void fresh(void) { memcpy(work, src, 2U * N); }

static void spectrum(void) {
  fresh();
  fd_mul_q15(work, win, work, N);
  e_spec = fd_rfft_q15(work, N);
}

#define CASE(name, call, out, bytes, before)                                   \
  BENCH_UNARY_DEF(name, uint32_t, in_u, int32_t, (call),                       \
                  h = bench_hash(h, (out), (bytes)), before)

static int32_t frame(void) {
  fd_mul_q15(work, win, work, N);
  int32_t e = fd_rfft_q15(work, N);
  e = fd_mel_q15(&mel, work, e, en);
  fd_log_q16(en, e, logs, BANDS);
  fd_dct2_q31(&dct, logs, mfcc);
  return 0;
}

CASE(b_mel, ((void)x, fd_mel_q15(&mel, work, e_spec, en)), en, sizeof en,
     spectrum())
CASE(b_log, ((void)x, fd_log_q16(en, e_en, logs, BANDS), 0), logs,
     sizeof logs, (void)0)
CASE(b_dct, ((void)x, fd_dct2_q31(&dct, logs, mfcc), 0), mfcc, sizeof mfcc,
     (void)0)
CASE(b_frame, ((void)x, frame()), mfcc, sizeof mfcc, fresh())

#ifdef BENCH_HAVE_CMSIS_DSP
/* Our filters, window and DCT as CMSIS-DSP's q15 tables */
#define tmp ((q31_t *)(void *)(bench_scratch + 2048U))
#define c_dct ((q15_t *)(void *)(bench_scratch + 7320U))
static q15_t c_coefs[2U * (N / 2U + 1U)];
static uint32_t c_pos[BANDS], c_len[BANDS];
static arm_mfcc_instance_q15 c_mfcc;
static q15_t c_out[COEFS];

static void c_setup(void) {
  for (uint32_t i = 0U; i < BANDS * COEFS; i++) {
    int32_t v = (tab[i] + 32768) >> 16;
    c_dct[i] = (q15_t)(v > 32767 ? 32767 : v);
  }
  uint32_t k = mel.k0, i = 0U, at = 0U;
  for (uint32_t j = 0U; j < BANDS; j++) {
    c_pos[j] = k;
    c_len[j] = (uint32_t)cnt[j] + cnt[j + 1U];
    for (uint32_t c = 0U; c < cnt[j]; c++) {
      c_coefs[at++] = (q15_t)wts[i + c];
    }
    for (uint32_t c = 0U; c < cnt[j + 1U]; c++) {
      uint32_t f = 32768U - wts[i + cnt[j] + c];
      c_coefs[at++] = (q15_t)(f > 32767U ? 32767U : f);
    }
    k += cnt[j];
    i += cnt[j];
  }
  (void)arm_mfcc_init_512_q15(&c_mfcc, BANDS, COEFS, c_dct, c_pos, c_len,
                              c_coefs, win);
}

BENCH_UNARY_DEF(c_frame, uint32_t, in_u, int32_t,
                ((void)x, (int32_t)arm_mfcc_q15(&c_mfcc, work, c_out, tmp)),
                h = bench_hash(h, c_out, sizeof c_out), fresh())
#endif

static const BenchCase cases[] = {
    {"fd_mel_q15, 512, 40 bands", "flexlib", b_mel, true},
    {"fd_log_q16, 40", "flexlib", b_log, true},
    {"fd_dct2_q31, 40 to 10", "flexlib", b_dct, true},
    {"MFCC frame: window, fd_rfft_q15, mel, log, DCT; 512, 40, 10", "flexlib",
     b_frame, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_mfcc_q15, 512, 40, 10", "CMSIS-DSP", c_frame, false},
#endif
};

const BenchGroup bench_group_fd_mfcc = {cases, BENCH_COUNT(cases)};
