/**
 * @file cases_fd_fir.c
 * @brief Bench cases for fd_fir.h: 32 taps, blocks of 64
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "bench.h"
#include "fd_fir.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define T 32U
#define BLK 64U
#define POOL 128U

static fm_q15 h15[T], hw15[T], x15[POOL];
static fm_q31 h31[T], x31[POOL];
static fm_q15 st15[T - 1U + BLK], stw15[T - 1U + BLK];
static fm_q31 st31[T - 1U + BLK];
static fm_q15 std15[T - 1U + BLK], sti15[T / 4U - 1U + BLK];
static union {
  fm_q15 q15[4U * BLK];
  fm_q31 q31[BLK];
} out;
static FD_FirQ15 fir15, firw15;
static FD_FirQ31 fir31;
static FD_FirDecimQ15 dec15;
static FD_FirInterpQ15 int15;
static fm_q15 cb15[T - 1U + BLK];
static fm_q31 cb31[T - 1U + BLK];

/* symmetric, sum |h| about 1.6 (the wide copy: about 3.2) */
static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  uint32_t s = 0x1234567U;
  for (unsigned i = 0U; i < T / 2U; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    int32_t v = (int32_t)(s >> 16) % 3300;
    h15[i] = h15[T - 1U - i] = (fm_q15)v;
    hw15[i] = hw15[T - 1U - i] = (fm_q15)(2 * v);
    h31[i] = h31[T - 1U - i] = (fm_q31)((uint32_t)v << 16);
  }
  for (unsigned i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    x31[i] = (fm_q31)s;
    x15[i] = (fm_q15)(s >> 16);
  }
  fd_fir_init_q15(&fir15, h15, T, st15, FD_FIR_STATE_LEN(T, BLK));
  fd_fir_init_q15(&firw15, hw15, T, stw15, FD_FIR_STATE_LEN(T, BLK));
  fd_fir_init_q31(&fir31, h31, T, st31, FD_FIR_STATE_LEN(T, BLK));
  fd_fir_decim_init_q15(&dec15, h15, T, 4U, std15, FD_FIR_STATE_LEN(T, BLK));
  fd_fir_interp_init_q15(&int15, h15, T, 4U, sti15,
                         FD_FIR_INTERP_STATE_LEN(T, 4U, BLK));
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

#define X15 (x15 + x)
#define X31 (x31 + x)
#define OUT(bytes) h = bench_hash(h, &out, (bytes))

/* before each sample, untimed: zero history, state full, so the timed call
 * moves the history and copies the block as in steady state */
#define FULL(f)                                                                \
  do {                                                                         \
    memset((f).state, 0, (f).len * sizeof(f).state[0]);                        \
    (f).w = (f).len;                                                           \
  } while (0)

static void prep(void) {
  memset(cb15, 0, sizeof cb15);
  memset(cb31, 0, sizeof cb31);
  FULL(fir15);
  FULL(firw15);
  FULL(fir31);
  FULL(dec15.fir);
  dec15.phase = 0U;
  FULL(int15.fir);
}

#define BLOCK(name, call, bytes)                                               \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, ((call), 0U), OUT(bytes),  \
                  prep())

/* ---- candidates: the library's buffer handling, other kernels ---- */

FX_INLINE fm_q15 o15(int32_t a) { return fm_sat_q15((a + 0x4000) >> 15); }

FX_INLINE fm_q31 o31(int64_t a) {
  uint64_t u = (uint64_t)a + 0x40000000U;
  int32_t hi = (int32_t)(u >> 32);
  return hi >= 0x40000000 ? INT32_MAX
         : hi < -0x40000000
             ? INT32_MIN
             : (fm_q31)(((uint32_t)hi << 1) | ((uint32_t)u >> 31));
}

/* the library's copies: the history moved down, then the block after it */
static const fm_q15 *load15(const fm_q15 *x) {
  fm_q15 *d = cb15;
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < T - 1U; i++) {
    d[i] = d[i + BLK];
  }
  for (uint32_t i = 0U; i < BLK; i++) {
    d[T - 1U + i] = x[i];
  }
  return cb15;
}
static const fm_q31 *load31(const fm_q31 *x) {
  fm_q31 *d = cb31;
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < T - 1U; i++) {
    d[i] = d[i + BLK];
  }
  for (uint32_t i = 0U; i < BLK; i++) {
    d[T - 1U + i] = x[i];
  }
  return cb31;
}

BENCH_CALL static void fir15_one(const fm_q15 *x, fm_q15 *y) {
  const fm_q15 *b = load15(x);
  for (uint32_t i = 0U; i < BLK; i++) {
    int32_t a = 0;
    for (uint32_t k = 0U; k < T; k++) {
      a += h15[T - 1U - k] * b[i + k];
    }
    y[i] = o15(a);
  }
}

BENCH_CALL static void fir15_two(const fm_q15 *x, fm_q15 *y) {
  const fm_q15 *b = load15(x);
  for (uint32_t i = 0U; i < BLK; i += 2U) {
    int32_t a0 = 0, a1 = 0, x0 = b[i];
    for (uint32_t k = 0U; k < T; k += 2U) {
      int32_t c = h15[T - 1U - k], x1 = b[i + k + 1U];
      a0 += c * x0;
      a1 += c * x1;
      c = h15[T - 2U - k];
      x0 = b[i + k + 2U];
      a0 += c * x1;
      a1 += c * x0;
    }
    y[i] = o15(a0);
    y[i + 1U] = o15(a1);
  }
}

/* symmetric: h[k] (x[n-k] + x[n-T+1+k]), four outputs per pass */
BENCH_CALL static void fir15_sym(const fm_q15 *x, fm_q15 *y) {
  const fm_q15 *b = load15(x);
  for (uint32_t i = 0U; i < BLK; i += 4U) {
    int32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    const fm_q15 *lo = b + i, *hi = b + i + T - 1U;
    for (uint32_t k = 0U; k < T / 2U; k++) {
      int32_t c = h15[k];
      a0 += c * (hi[0] + lo[0]);
      a1 += c * (hi[1] + lo[1]);
      a2 += c * (hi[2] + lo[2]);
      a3 += c * (hi[3] + lo[3]);
      lo++;
      hi--;
    }
    y[i] = o15(a0);
    y[i + 1U] = o15(a1);
    y[i + 2U] = o15(a2);
    y[i + 3U] = o15(a3);
  }
}

BENCH_CALL static void fir31_two(const fm_q31 *x, fm_q31 *y) {
  const fm_q31 *b = load31(x);
  for (uint32_t i = 0U; i < BLK; i += 2U) {
    int64_t a0 = 0, a1 = 0;
    int32_t x0 = b[i];
    for (uint32_t k = 0U; k < T; k += 2U) {
      int32_t c = h31[T - 1U - k], x1 = b[i + k + 1U];
      a0 = fx_smlal(a0, c, x0);
      a1 = fx_smlal(a1, c, x1);
      c = h31[T - 2U - k];
      x0 = b[i + k + 2U];
      a0 = fx_smlal(a0, c, x1);
      a1 = fx_smlal(a1, c, x0);
    }
    y[i] = o31(a0);
    y[i + 1U] = o31(a1);
  }
}

BENCH_CALL static void fir31_four(const fm_q31 *x, fm_q31 *y) {
  const fm_q31 *b = load31(x);
  for (uint32_t i = 0U; i < BLK; i += 4U) {
    int64_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int32_t x0 = b[i], x1 = b[i + 1U], x2 = b[i + 2U];
    for (uint32_t k = 0U; k < T; k++) {
      int32_t c = h31[T - 1U - k], x3 = b[i + k + 3U];
      a0 = fx_smlal(a0, c, x0);
      a1 = fx_smlal(a1, c, x1);
      a2 = fx_smlal(a2, c, x2);
      a3 = fx_smlal(a3, c, x3);
      x0 = x1;
      x1 = x2;
      x2 = x3;
    }
    y[i] = o31(a0);
    y[i + 1U] = o31(a1);
    y[i + 2U] = o31(a2);
    y[i + 3U] = o31(a3);
  }
}

BENCH_CALL static void design_lp(fm_q15 *q) {
  static float f[T];
  (void)fd_fir_design(f, T, FD_LOWPASS, 0.1f, 0.0f, FD_WIN_HAMMING);
  (void)fd_fir_quantize_q15(f, q, T);
}

BLOCK(b_fir15, fd_fir_q15(&fir15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_fir15_one, fir15_one(X15, out.q15), BLK * 2U)
BLOCK(b_fir15_two, fir15_two(X15, out.q15), BLK * 2U)
BLOCK(b_fir15_sym, fir15_sym(X15, out.q15), BLK * 2U)
BLOCK(b_fir15_wide, fd_fir_q15(&firw15, X15, out.q15, BLK), BLK * 2U)
BLOCK(b_fir31, fd_fir_q31(&fir31, X31, out.q31, BLK), BLK * 4U)
BLOCK(b_fir31_two, fir31_two(X31, out.q31), BLK * 4U)
BLOCK(b_fir31_four, fir31_four(X31, out.q31), BLK * 4U)
BLOCK(b_decim15, fd_fir_decim_q15(&dec15, X15, out.q15, BLK), BLK / 2U)
BLOCK(b_interp15, fd_fir_interp_q15(&int15, X15, out.q15, BLK), BLK * 8U)
/* a single sample in steady state: room for it, no history move */
static void prep_one(void) {
  prep();
  fir15.w = fir15.len - 1U;
}
BENCH_UNARY_DEF(b_sample15, uint32_t, in_off, fm_q15,
                fd_fir_sample_q15(&fir15, X15[0]), (void)0, prep_one())
BLOCK(b_design, design_lp(out.q15), T * 2U)

#ifdef BENCH_HAVE_CMSIS_DSP
static arm_fir_instance_q15 c15;
static arm_fir_instance_q31 c31;
static arm_fir_decimate_instance_q15 cd15;
static arm_fir_interpolate_instance_q15 ci15;
static q15_t cs15[T + BLK], cds15[T + BLK - 1U], cis15[T / 4U + BLK - 1U];
static q31_t cs31[T + BLK];

static void cmsis_init(void) {
  static bool done;
  if (!done) {
    done = true;
    setup();
    (void)arm_fir_init_q15(&c15, T, h15, cs15, BLK);
    arm_fir_init_q31(&c31, T, h31, cs31, BLK);
    (void)arm_fir_decimate_init_q15(&cd15, T, 4U, h15, cds15, BLK);
    (void)arm_fir_interpolate_init_q15(&ci15, 4U, T, h15, cis15, BLK);
  }
}

static inline uint32_t in_off_c(uint32_t u) {
  cmsis_init();
  return u % 64U;
}

static void cprep(void) {
  memset(cs15, 0, sizeof cs15);
  memset(cs31, 0, sizeof cs31);
  memset(cds15, 0, sizeof cds15);
  memset(cis15, 0, sizeof cis15);
}

#define CBLOCK(name, call, bytes)                                              \
  BENCH_UNARY_DEF(name, uint32_t, in_off_c, uint32_t, ((call), 0U),            \
                  OUT(bytes), cprep())

CBLOCK(c_fir15, arm_fir_q15(&c15, X15, out.q15, BLK), BLK * 2U)
CBLOCK(c_fir15_fast, arm_fir_fast_q15(&c15, X15, out.q15, BLK), BLK * 2U)
CBLOCK(c_fir31, arm_fir_q31(&c31, X31, out.q31, BLK), BLK * 4U)
CBLOCK(c_fir31_fast, arm_fir_fast_q31(&c31, X31, out.q31, BLK), BLK * 4U)
CBLOCK(c_decim15, arm_fir_decimate_q15(&cd15, X15, out.q15, BLK), BLK / 2U)
CBLOCK(c_interp15, arm_fir_interpolate_q15(&ci15, X15, out.q15, BLK), BLK * 8U)
#endif

static const BenchCase cases[] = {
    {"fd_fir_q15, 32 taps x 64", "flexlib", b_fir15, true},
    {"fir q15 one output per pass", "candidate", b_fir15_one, true},
    {"fir q15 two outputs per pass", "candidate", b_fir15_two, true},
    {"fir q15 symmetric, four per pass", "candidate", b_fir15_sym, true},
    {"fd_fir_q15, gain 2 or more (64-bit)", "flexlib", b_fir15_wide, true},
    {"fd_fir_q31, 32 taps x 64", "flexlib", b_fir31, true},
    {"fir q31 two outputs per pass", "candidate", b_fir31_two, true},
    {"fir q31 four outputs per pass", "candidate", b_fir31_four, true},
    {"fd_fir_decim_q15 by 4, 32 taps x 64", "flexlib", b_decim15, true},
    {"fd_fir_interp_q15 by 4, 32 taps x 64", "flexlib", b_interp15, true},
    {"fd_fir_sample_q15, 32 taps", "flexlib", b_sample15, true},
    {"fd_fir_design + quantize, 32 taps", "flexlib", b_design, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_fir_q15, 32 taps x 64", "CMSIS-DSP", c_fir15, false},
    {"arm_fir_fast_q15, 32 taps x 64", "CMSIS-DSP", c_fir15_fast, false},
    {"arm_fir_q31, 32 taps x 64", "CMSIS-DSP", c_fir31, false},
    {"arm_fir_fast_q31, 32 taps x 64", "CMSIS-DSP", c_fir31_fast, false},
    {"arm_fir_decimate_q15 by 4, 32 x 64", "CMSIS-DSP", c_decim15, false},
    {"arm_fir_interpolate_q15 by 4, 32 x 64", "CMSIS-DSP", c_interp15, false},
#endif
};

const BenchGroup bench_group_fd_fir = {cases, BENCH_COUNT(cases)};
