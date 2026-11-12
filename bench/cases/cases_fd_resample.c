/**
 * @file cases_fd_resample.c
 * @brief Bench cases for sample-rate conversion
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bench.h"
#include "fd_fir_core.h"
#include "fd_resample.h"
#include "fm_float.h"
#ifdef BENCH_HAVE_CMSIS_DSP
#include "arm_math.h"
#endif

#define BLK 64U
#define POOL 128U
#define T3 24U                     /* 3 / 2: 8 taps per branch */
#define TBIG 1280U                 /* 160 / 147: 8 taps per branch */
#define HK 8U                      /* half-band: 31 taps */
#define STLEN (4U * HK - 2U + BLK) /* the longest history: 31 taps */

static fm_q15 h3[T3], hbig[TBIG], x15[POOL];
static fm_q31 h3q31[T3], x31[POOL];
static fm_q15 st15[STLEN];
static fm_q31 st31[STLEN];
static FD_ResampleQ15 rs15;
static FD_ResampleQ31 rs31;

static fm_q15 hb15[2U * HK], full15[4U * HK - 1U], full2_15[4U * HK - 1U];
static fm_q31 hb31[2U * HK];
static FD_HalfbandQ15 hbf15;
static FD_HalfbandQ31 hbf31;
static FD_FirDecimQ15 dec15;
static FD_FirInterpQ15 int15;

#define CN 256U /* CIC inputs per call */
static fm_q15 cx16[CN], cx12[CN];
static FD_Cic cic;

/* the outputs */
typedef union {
  fm_q15 q15[4U * BLK];
  fm_q31 q31[3U * BLK]; /* CMSIS-DSP writes all 3 x 64 */
} Out;
_Static_assert(sizeof(Out) <= BENCH_SCRATCH_SIZE, "outputs in the scratch");
#define out (*(Out *)(void *)bench_scratch)

/* fd_resample_design's 160 / 147 lowpass (fc 0.9, Hamming) tap by tap: its
 * float array would not fit the image's RAM */
static float big_tap(uint32_t k) {
  const float pi = 3.14159265358979f, f = 0.45f / 160.0f;
  float m = (float)k - 0.5f * (float)(TBIG - 1U);
  float w = 0.54f - 0.46f * fm_cosf(2.0f * pi * (float)k / (float)(TBIG - 1U));
  return fm_sinf(2.0f * pi * f * m) / (pi * m) * w;
}

static void setup(void) {
  static bool done;
  if (done) {
    return;
  }
  done = true;
  static float f[T3];
  (void)fd_resample_design(f, T3, 3U, 2U, 0.9f, FD_WIN_HAMMING);
  (void)fd_fir_quantize_q15(f, h3, T3);
  (void)fd_fir_quantize_q31(f, h3q31, T3);
  static float fh[2U * HK];
  (void)fd_halfband_design(fh, HK, FD_WIN_BLACKMAN);
  (void)fd_fir_quantize_q15(fh, hb15, 2U * HK);
  (void)fd_fir_quantize_q31(fh, hb31, 2U * HK);
  /* the same filter in full for fd_fir: zeros, the centre (gain 2: 32767) */
  for (uint32_t j = 0U; j < 2U * HK; j++) {
    full15[2U * j] = hb15[j];
    full2_15[2U * j] = (fm_q15)(2 * hb15[j]);
  }
  full15[2U * HK - 1U] = 16384;
  full2_15[2U * HK - 1U] = 32767;
  float g = 0.0f;
  for (uint32_t k = 0U; k < TBIG; k++) {
    g += big_tap(k);
  }
  for (uint32_t k = 0U; k < TBIG; k++) {
    hbig[k] = fm_f32_to_q15(big_tap(k) * 160.0f / g);
  }
  uint32_t s = 0x2F6B9A31U;
  for (unsigned i = 0U; i < POOL; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    x31[i] = (fm_q31)s;
    x15[i] = (fm_q15)(s >> 16);
  }
  for (unsigned i = 0U; i < CN; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    cx16[i] = (fm_q15)(s >> 16);
    cx12[i] = (fm_q15)(cx16[i] >> 4); /* a 12-bit ADC's range */
  }
}

static inline uint32_t in_off(uint32_t u) {
  setup();
  return u % 64U;
}

#define X15 (x15 + x)
#define X31 (x31 + x)
#define OUT(bytes) h = bench_hash(h, &out, (bytes))

/* untimed, before each sample: zero history and phase, state full, so the
 * timed call moves the history as in steady state */
static void prep15(const fm_q15 *h, uint32_t ntaps, uint32_t l, uint32_t m) {
  memset(st15, 0, sizeof st15);
  fd_resample_init_q15(&rs15, h, ntaps, l, m, st15,
                       FD_RESAMPLE_STATE_LEN(ntaps, l, BLK));
  rs15.ip.fir.w = rs15.ip.fir.len;
}

static void prep31(const fm_q31 *h, uint32_t ntaps, uint32_t l, uint32_t m) {
  memset(st31, 0, sizeof st31);
  fd_resample_init_q31(&rs31, h, ntaps, l, m, st31,
                       FD_RESAMPLE_STATE_LEN(ntaps, l, BLK));
  rs31.ip.fir.w = rs31.ip.fir.len;
}

static void prep_hb(void) {
  memset(st15, 0, sizeof st15);
  memset(st31, 0, sizeof st31);
  fd_halfband_decim_init_q15(&hbf15, hb15, HK, st15,
                             FD_HALFBAND_DECIM_STATE_LEN(HK, BLK));
  hbf15.fir.w = hbf15.fir.len;
  fd_halfband_decim_init_q31(&hbf31, hb31, HK, st31,
                             FD_HALFBAND_DECIM_STATE_LEN(HK, BLK));
  hbf31.fir.w = hbf31.fir.len;
}

static void prep_hbi(void) {
  memset(st15, 0, sizeof st15);
  memset(st31, 0, sizeof st31);
  fd_halfband_interp_init_q15(&hbf15, hb15, HK, st15,
                              FD_HALFBAND_INTERP_STATE_LEN(HK, BLK));
  hbf15.fir.w = hbf15.fir.len;
  fd_halfband_interp_init_q31(&hbf31, hb31, HK, st31,
                              FD_HALFBAND_INTERP_STATE_LEN(HK, BLK));
  hbf31.fir.w = hbf31.fir.len;
}

static void prep_fir(void) {
  memset(st15, 0, sizeof st15);
  fd_fir_decim_init_q15(&dec15, full15, 4U * HK - 1U, 2U, st15,
                        FD_FIR_STATE_LEN(4U * HK - 1U, BLK));
  dec15.fir.w = dec15.fir.len;
}

static void prep_firi(void) {
  memset(st15, 0, sizeof st15);
  fd_fir_interp_init_q15(&int15, full2_15, 4U * HK - 1U, 2U, st15,
                         FD_FIR_INTERP_STATE_LEN(4U * HK - 1U, 2U, BLK));
  int15.fir.w = int15.fir.len;
}

#define BLOCK(name, call, bytes, before)                                       \
  BENCH_UNARY_DEF(name, uint32_t, in_off, uint32_t, (call), OUT(bytes), before)

/* ---- candidates: the shipped driver with path forced ---- */

/* by residue (grouped) or one output at a time in phase order */
BENCH_CALL static uint32_t cand15(FD_ResampleQ15 *r, const fm_q15 *x, fm_q15 *y,
                                  uint32_t n, bool grouped) {
  FD_FirQ15 *f = &r->ip.fir;
  const uint32_t l = r->ip.l, m = r->m, t = f->ntaps;
  const uint32_t r0 = r->ip.ntaps - (t - 1U) * l, mq = m / l, mr = m % l;
  uint32_t got = 0U;
  while (n != 0U) {
    uint32_t c = take15(f, x, n), span = c * l;
    if (r->ph < span) {
      uint32_t cnt = (span - 1U - r->ph) / m + 1U;
      const fm_q15 *b = f->state + f->w;
      uint32_t i = r->ph / l, br = r->ph % l;
      if (grouped) {
        const uint32_t nr = cnt < l ? cnt : l, per = cnt / l, extra = cnt % l;
        for (uint32_t q = 0U; q < nr; q++) {
          uint32_t tb = br < r0 ? t : t - 1U;
          k15d(f->h + br, l, tb, b + i + 1U - tb, m, y + q, l,
               per + (q < extra ? 1U : 0U));
          i += mq;
          br += mr;
          if (br >= l) {
            br -= l;
            i++;
          }
        }
      } else {
        for (uint32_t k = 0U; k < cnt; k++) {
          uint32_t tb = br < r0 ? t : t - 1U;
          const fm_q15 *wb = b + i + 1U - tb, *hp = f->h + br + tb * l;
          int32_t a = 0;
          FX_UNROLL(4)
          for (uint32_t j = 0U; j < tb; j++) {
            hp -= l;
            a += *hp * wb[j];
          }
          y[k] = out15(a);
          i += mq;
          br += mr;
          if (br >= l) {
            br -= l;
            i++;
          }
        }
      }
      r->ph += cnt * m;
      y += cnt;
      got += cnt;
    }
    r->ph -= span;
    f->w += c;
    x += c;
    n -= c;
  }
  return got;
}

BLOCK(b_rs15, fd_resample_q15(&rs15, X15, out.q15, BLK), 96U * 2U,
      prep15(h3, T3, 3U, 2U))
BLOCK(b_rs31, fd_resample_q31(&rs31, X31, out.q31, BLK), 96U * 4U,
      prep31(h3q31, T3, 3U, 2U))
BLOCK(b_rs15_big, fd_resample_q15(&rs15, X15, out.q15, BLK), 70U * 2U,
      prep15(hbig, TBIG, 160U, 147U))

BLOCK(b_hbd15, fd_halfband_decim_q15(&hbf15, X15, out.q15, BLK), BLK, prep_hb())
BLOCK(b_hbd31, fd_halfband_decim_q31(&hbf31, X31, out.q31, BLK), BLK * 2U,
      prep_hb())
BLOCK(b_hbi15, (fd_halfband_interp_q15(&hbf15, X15, out.q15, BLK), 0U),
      BLK * 4U, prep_hbi())
BLOCK(b_hbi31, (fd_halfband_interp_q31(&hbf31, X31, out.q31, BLK), 0U),
      BLK * 8U, prep_hbi())
BLOCK(b_fird15, fd_fir_decim_q15(&dec15, X15, out.q15, BLK), BLK, prep_fir())
BLOCK(b_firi15, (fd_fir_interp_q15(&int15, X15, out.q15, BLK), 0U), BLK * 4U,
      prep_firi())

static void prep_cic(uint32_t order, uint32_t ratio, uint32_t bits) {
  (void)fd_cic_init(&cic, order, ratio, bits);
}

/* candidate: the order-3 32-bit integrators unrolled by 8 (the library: 4) */
BENCH_CALL static uint32_t cic3_unroll8(FD_Cic *c, const fm_q15 *x, fm_q31 *y,
                                        uint32_t n) {
  uint32_t got = 0U, a0 = (uint32_t)c->integ[0], a1 = (uint32_t)c->integ[1],
           a2 = (uint32_t)c->integ[2];
  while (n != 0U) {
    uint32_t run = c->ratio - c->count;
    run = run < n ? run : n;
    FX_UNROLL(8)
    for (uint32_t i = 0U; i < run; i++) {
      int32_t v = x[i];
      a0 += (uint32_t)v;
      a1 += a0;
      a2 += a1;
    }
    x += run;
    n -= run;
    c->count += run;
    if (c->count == c->ratio) {
      c->count = 0U;
      uint32_t w = a2;
      for (uint32_t k = 0U; k < 3U; k++) {
        uint32_t d = w - (uint32_t)c->comb[k];
        c->comb[k] = w;
        w = d;
      }
      y[got++] = (fm_q31)(w << c->shift);
    }
  }
  c->integ[0] = a0;
  c->integ[1] = a1;
  c->integ[2] = a2;
  return got;
}

BLOCK(b_cic12, fd_cic_decim_q15(&cic, cx12, out.q31, CN), 4U * 4U,
      prep_cic(3U, 64U, 12U))
BLOCK(b_cic16, fd_cic_decim_q15(&cic, cx16, out.q31, CN), 4U * 4U,
      prep_cic(3U, 64U, 16U))
BLOCK(b_cic5, fd_cic_decim_q15(&cic, cx12, out.q31, CN), 16U * 4U,
      prep_cic(5U, 16U, 12U))
BLOCK(c_cic8, cic3_unroll8(&cic, cx12, out.q31, CN), 4U * 4U,
      prep_cic(3U, 64U, 12U))

/* the crossover: 8 taps per branch, l / (l - 1), 64 in */
#define CAND(name, l, grouped)                                                 \
  BLOCK(name, cand15(&rs15, X15, out.q15, BLK, grouped),                       \
        (BLK * (l) + (l) - 2U) / ((l) - 1U) * 2U,                              \
        prep15(hbig, 8U * (l), l, (l) - 1U))
CAND(c_g3, 3U, true)
CAND(c_o3, 3U, false)
CAND(c_g8, 8U, true)
CAND(c_o8, 8U, false)
CAND(c_g16, 16U, true)
CAND(c_o16, 16U, false)
CAND(c_g32, 32U, true)
CAND(c_o32, 32U, false)
CAND(c_g64, 64U, true)
CAND(c_o64, 64U, false)
CAND(c_g160, 160U, true)
CAND(c_o160, 160U, false)

#ifdef BENCH_HAVE_CMSIS_DSP
/* CMSIS-DSP has no rational resampler: interpolate by 3, keep every 2nd */
static arm_fir_interpolate_instance_q15 ci15;
static arm_fir_interpolate_instance_q31 ci31;
static q15_t cis15[T3 / 3U - 1U + BLK];
static q31_t cis31[T3 / 3U - 1U + BLK];

static void cprep(void) {
  setup();
  (void)arm_fir_interpolate_init_q15(&ci15, 3U, T3, h3, cis15, BLK);
  (void)arm_fir_interpolate_init_q31(&ci31, 3U, T3, h3q31, cis31, BLK);
}

BENCH_CALL static uint32_t c_interp_drop15(const q15_t *x, q15_t *y) {
  arm_fir_interpolate_q15(&ci15, x, y, BLK);
  for (uint32_t j = 1U; j < 3U * BLK / 2U; j++) {
    y[j] = y[2U * j];
  }
  return 3U * BLK / 2U;
}

BENCH_CALL static uint32_t c_interp_drop31(const q31_t *x, q31_t *y) {
  arm_fir_interpolate_q31(&ci31, x, y, BLK);
  for (uint32_t j = 1U; j < 3U * BLK / 2U; j++) {
    y[j] = y[2U * j];
  }
  return 3U * BLK / 2U;
}

BLOCK(c_rs15, c_interp_drop15(X15, out.q15), 96U * 2U, cprep())
BLOCK(c_rs31, c_interp_drop31(X31, out.q31), 96U * 4U, cprep())
#endif

static const BenchCase cases[] = {
    {"fd_resample_q15 3/2, 24 taps x 64", "flexlib", b_rs15, true},
    {"fd_resample_q31 3/2, 24 taps x 64", "flexlib", b_rs31, true},
    {"fd_resample_q15 160/147, 1280 taps x 64", "flexlib", b_rs15_big, true},
    {"fd_halfband_decim_q15, 31 taps x 64", "flexlib", b_hbd15, true},
    {"fd_halfband_decim_q31, 31 taps x 64", "flexlib", b_hbd31, true},
    {"fd_halfband_interp_q15, 31 taps x 64", "flexlib", b_hbi15, true},
    {"fd_halfband_interp_q31, 31 taps x 64", "flexlib", b_hbi31, true},
    {"fd_fir_decim_q15 by 2, the same 31 taps x 64", "flexlib", b_fird15, true},
    {"fd_fir_interp_q15 by 2, the same 31 taps x 64", "flexlib", b_firi15,
     true},
    {"fd_cic_decim_q15 order 3 by 64, 12-bit x 256", "flexlib", b_cic12, true},
    {"fd_cic_decim_q15 order 3 by 64, 16-bit x 256", "flexlib", b_cic16, true},
    {"fd_cic_decim_q15 order 5 by 16, 12-bit x 256", "flexlib", b_cic5, true},
    {"cic order 3 by 64, integrators unrolled by 8", "candidate", c_cic8, true},
    {"resample q15 3/2 by residue", "candidate", c_g3, true},
    {"resample q15 3/2 one by one", "candidate", c_o3, true},
    {"resample q15 8/7 by residue", "candidate", c_g8, true},
    {"resample q15 8/7 one by one", "candidate", c_o8, true},
    {"resample q15 16/15 by residue", "candidate", c_g16, true},
    {"resample q15 16/15 one by one", "candidate", c_o16, true},
    {"resample q15 32/31 by residue", "candidate", c_g32, true},
    {"resample q15 32/31 one by one", "candidate", c_o32, true},
    {"resample q15 64/63 by residue", "candidate", c_g64, true},
    {"resample q15 64/63 one by one", "candidate", c_o64, true},
    {"resample q15 160/159 by residue", "candidate", c_g160, true},
    {"resample q15 160/159 one by one", "candidate", c_o160, true},
#ifdef BENCH_HAVE_CMSIS_DSP
    {"arm_fir_interpolate_q15 by 3, keep 1 in 2", "CMSIS-DSP", c_rs15, false},
    {"arm_fir_interpolate_q31 by 3, keep 1 in 2", "CMSIS-DSP", c_rs31, false},
#endif
};

const BenchGroup bench_group_fd_resample = {cases, BENCH_COUNT(cases)};
