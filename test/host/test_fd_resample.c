/**
 * @file test_fd_resample.c
 * @brief Host tests for fd_resample.h: the resampler against fd_fir_interp
 * followed by keeping every m-th output
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_resample.h"
#include "test_assert.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NX 300U
#define MAXH 1300U
#define MAXL 160U

static uint32_t rng = 0x2545F491U;

static uint32_t rnd(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

static fm_q15 x15[NX], z15[NX * MAXL], y15[NX * MAXL + 1U];
static fm_q31 x31[NX], z31[NX * MAXL], y31[NX * MAXL + 1U];
static fm_q15 h15[MAXH], st15[MAXH + NX], rs15[MAXH + NX];
static fm_q31 h31[MAXH], st31[MAXH + NX], rs31[MAXH + NX];

/* random taps, each branch's sum |h| under 2 unless wide */
static void taps(uint32_t ntaps, uint32_t l, bool wide) {
  uint32_t t = (ntaps + l - 1U) / l;
  int32_t lim = wide ? 32767 : (int32_t)(60000U / t);
  for (uint32_t k = 0U; k < ntaps; k++) {
    int32_t v = (int32_t)(rnd() % (2U * (uint32_t)lim + 1U)) - lim;
    h15[k] = (fm_q15)(v > 32767 ? 32767 : v);
    h31[k] = (fm_q31)((int64_t)h15[k] * 65536 + (int32_t)(rnd() & 0xFFFFU));
  }
}

static void input(int kind) {
  for (uint32_t i = 0U; i < NX; i++) {
    uint32_t u = rnd();
    int32_t v =
        kind == 0 ? (int32_t)u : ((u & 1U) != 0U ? INT32_MAX : INT32_MIN);
    x15[i] = (fm_q15)(v >> 16);
    x31[i] = v;
  }
}

/* one stream cut at random into blocks of 1 .. 17, through a small chunk */
static void check(uint32_t ntaps, uint32_t l, uint32_t m, uint32_t chunk) {
  FD_FirInterpQ15 p;
  FD_FirInterpQ31 P;
  uint32_t t = (ntaps + l - 1U) / l;
  fd_fir_interp_init_q15(&p, h15, ntaps, l, st15, t - 1U + NX);
  fd_fir_interp_init_q31(&P, h31, ntaps, l, st31, t - 1U + NX);
  fd_fir_interp_q15(&p, x15, z15, NX);
  fd_fir_interp_q31(&P, x31, z31, NX);

  FD_ResampleQ15 r;
  FD_ResampleQ31 R;
  uint32_t len = FD_RESAMPLE_STATE_LEN(ntaps, l, chunk);
  fd_resample_init_q15(&r, h15, ntaps, l, m, rs15, len);
  fd_resample_init_q31(&R, h31, ntaps, l, m, rs31, len);
  uint32_t got = 0U, got31 = 0U, past = 0U;
  for (uint32_t j = 0U; j <= NX * l; j++) { /* nothing written past the count */
    y15[j] = 0x5A5A;
    y31[j] = 0x5A5A5A5A;
  }
  for (uint32_t i = 0U; i < NX;) {
    uint32_t q = 1U + rnd() % 17U;
    q = q > NX - i ? NX - i : q;
    uint32_t a = fd_resample_q15(&r, x15 + i, y15 + got, q);
    uint32_t b = fd_resample_q31(&R, x31 + i, y31 + got31, q);
    ASSERT_TRUE(a <= FD_RESAMPLE_OUT_MAX(q, l, m) && a == b);
    got += a;
    got31 += b;
    past += y15[got] != 0x5A5A || y31[got31] != 0x5A5A5A5A;
    i += q;
  }
  ASSERT_EQ(past, 0U);
  ASSERT_EQ(got, (NX * l + m - 1U) / m);
  uint32_t bad = 0U;
  for (uint32_t j = 0U; j < got; j++) {
    bad += y15[j] != z15[j * m];
    bad += y31[j] != z31[j * m];
  }
  ASSERT_EQ(bad, 0U);
}

void test_fd_resample_oracle(void) {
  static const uint32_t nt[] = {1U, 2U, 5U, 13U, 24U, 31U};
  for (uint32_t l = 1U; l <= 8U; l++) {
    for (uint32_t m = 1U; m <= 8U; m++) {
      for (uint32_t k = 0U; k < sizeof nt / sizeof nt[0]; k++) {
        bool wide = (l + m + k) % 5U == 0U;
        taps(nt[k], l, wide);
        input((int)((l * m + k) % 3U == 0U));
        check(nt[k], l, m, 1U + (l * 7U + m + k) % 40U);
      }
    }
  }
}

/* 48 kHz from 44.1 kHz and back: few outputs per residue in a chunk */
void test_fd_resample_large(void) {
  taps(1280U, 160U, false);
  input(0);
  check(1280U, 160U, 147U, 64U);
  taps(1176U, 147U, false);
  check(1176U, 147U, 160U, 33U);
  taps(1280U, 160U, true);
  input(1);
  check(1280U, 160U, 147U, 64U);
}

void test_fd_resample_reset(void) {
  taps(24U, 3U, false);
  input(0);
  FD_ResampleQ15 r;
  fm_q15 a[100], b[100];
  fd_resample_init_q15(&r, h15, 24U, 3U, 2U, rs15,
                       FD_RESAMPLE_STATE_LEN(24U, 3U, 16U));
  uint32_t na = fd_resample_q15(&r, x15, a, 50U);
  (void)fd_resample_q15(&r, x15 + 50, b, 7U);
  fd_resample_reset_q15(&r);
  uint32_t nb = fd_resample_q15(&r, x15, b, 50U);
  ASSERT_EQ(na, 75U);
  ASSERT_EQ(nb, na);
  ASSERT_TRUE(memcmp(a, b, na * sizeof a[0]) == 0);
  FD_ResampleQ31 R;
  fm_q31 A[100], B[100];
  fd_resample_init_q31(&R, h31, 24U, 3U, 2U, rs31,
                       FD_RESAMPLE_STATE_LEN(24U, 3U, 16U));
  na = fd_resample_q31(&R, x31, A, 50U);
  (void)fd_resample_q31(&R, x31 + 50, B, 7U);
  fd_resample_reset_q31(&R);
  nb = fd_resample_q31(&R, x31, B, 50U);
  ASSERT_EQ(nb, na);
  ASSERT_TRUE(memcmp(A, B, na * sizeof A[0]) == 0);
}

#define TAU 6.283185307179586

/* |H| at f cycles per sample of the l-times rate */
static double resp(const float *h, uint32_t n, double f) {
  double re = 0.0, im = 0.0;
  for (uint32_t k = 0U; k < n; k++) {
    re += (double)h[k] * cos(TAU * f * k);
    im -= (double)h[k] * sin(TAU * f * k);
  }
  return sqrt(re * re + im * im);
}

void test_fd_resample_design(void) {
  static float h[96];
  ASSERT_EQ(fd_resample_design(h, 96U, 3U, 2U, 0.0f, FD_WIN_HANN),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_resample_design(h, 96U, 3U, 2U, 1.0f, FD_WIN_HANN),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_resample_design(h, 96U, 3U, 2U, 0.8f, FD_WIN_BLACKMAN_HARRIS),
            FD_OK);
  /* gain l at DC, the input's band kept, the images and the output's alias
   * band (above 1/(2 max(l, m)) here: 1/6) removed */
  ASSERT_NEAR(resp(h, 96U, 0.0), 3.0, 1e-4);
  ASSERT_NEAR(resp(h, 96U, 0.015), 3.0, 3e-3);
  double worst = 0.0;
  for (double f = 1.0 / 6.0 + 0.04; f <= 0.5; f += 0.002) {
    double v = resp(h, 96U, f) / 3.0;
    worst = v > worst ? v : worst;
  }
  ASSERT_TRUE(20.0 * log10(worst) < -80.0);
  static fm_q15 q[96];
  ASSERT_EQ(fd_fir_quantize_q15(h, q, 96U), FD_OK);
}

/* ---- half-band ---- */

#define MAXK 16U

static fm_q15 hb15[2U * MAXK], full15[4U * MAXK];
static fm_q31 hb31[2U * MAXK], full31[4U * MAXK];

/* random odd-offset taps (sum |hb| under 1.5 unless wide) and the full filter
 */
static void hb_taps(uint32_t k, bool wide) {
  int32_t lim = wide ? 32767 : (int32_t)(49000U / (2U * k));
  for (uint32_t j = 0U; j < 2U * k; j++) {
    int32_t v = (int32_t)(rnd() % (2U * (uint32_t)lim + 1U)) - lim;
    hb15[j] = (fm_q15)v;
    hb31[j] = (fm_q31)((int64_t)v * 65536 + (int32_t)(rnd() & 0xFFFFU));
    if (!wide && hb31[j] > 0) {
      hb31[j] -= 0xFFFF; /* keep sum |hb31| under 1.5 too */
    }
  }
  memset(full15, 0, sizeof full15);
  memset(full31, 0, sizeof full31);
  for (uint32_t j = 0U; j < 2U * k; j++) {
    full15[2U * j] = hb15[j];
    full31[2U * j] = hb31[j];
  }
  full15[2U * k - 1U] = 16384;
  full31[2U * k - 1U] = 1 << 30;
}

void test_fd_resample_halfband_decim(void) {
  for (uint32_t k = 1U; k <= MAXK; k++) {
    for (int v = 0; v < 4; v++) {
      hb_taps(k, v == 3);
      input(v & 1);
      uint32_t nt = 4U * k - 1U;
      FD_FirDecimQ15 d;
      FD_FirDecimQ31 D;
      fd_fir_decim_init_q15(&d, full15, nt, 2U, st15, nt - 1U + NX);
      fd_fir_decim_init_q31(&D, full31, nt, 2U, st31, nt - 1U + NX);
      uint32_t want = fd_fir_decim_q15(&d, x15, z15, NX);
      ASSERT_EQ(fd_fir_decim_q31(&D, x31, z31, NX), want);
      FD_HalfbandQ15 h;
      FD_HalfbandQ31 H;
      uint32_t chunk = 1U + (k * 5U + (uint32_t)v) % 23U;
      fd_halfband_decim_init_q15(&h, hb15, k, rs15,
                                 FD_HALFBAND_DECIM_STATE_LEN(k, chunk));
      fd_halfband_decim_init_q31(&H, hb31, k, rs31,
                                 FD_HALFBAND_DECIM_STATE_LEN(k, chunk));
      uint32_t got = 0U, got31 = 0U;
      for (uint32_t i = 0U; i < NX;) {
        uint32_t q = 1U + rnd() % 17U;
        q = q > NX - i ? NX - i : q;
        got += fd_halfband_decim_q15(&h, x15 + i, y15 + got, q);
        got31 += fd_halfband_decim_q31(&H, x31 + i, y31 + got31, q);
        i += q;
      }
      ASSERT_EQ(got, want);
      ASSERT_EQ(got31, want);
      ASSERT_TRUE(memcmp(y15, z15, want * sizeof y15[0]) == 0);
      ASSERT_TRUE(memcmp(y31, z31, want * sizeof y31[0]) == 0);
    }
  }
}

static int32_t sat(int64_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* y[2i] = round(sum hb[j] x[i - j] / 2^14), y[2i + 1] = x[i - k + 1] */
void test_fd_resample_halfband_interp(void) {
  for (uint32_t k = 1U; k <= MAXK; k++) {
    for (int v = 0; v < 4; v++) {
      hb_taps(k, v == 3);
      input(v & 1);
      FD_HalfbandQ15 h;
      FD_HalfbandQ31 H;
      uint32_t chunk = 1U + (k * 3U + (uint32_t)v) % 19U;
      fd_halfband_interp_init_q15(&h, hb15, k, rs15,
                                  FD_HALFBAND_INTERP_STATE_LEN(k, chunk));
      fd_halfband_interp_init_q31(&H, hb31, k, rs31,
                                  FD_HALFBAND_INTERP_STATE_LEN(k, chunk));
      for (uint32_t i = 0U; i < NX;) {
        uint32_t q = 1U + rnd() % 17U;
        q = q > NX - i ? NX - i : q;
        fd_halfband_interp_q15(&h, x15 + i, y15 + 2U * i, q);
        fd_halfband_interp_q31(&H, x31 + i, y31 + 2U * i, q);
        i += q;
      }
      uint32_t bad = 0U;
      for (uint32_t i = 0U; i < NX; i++) {
        int64_t a = 0;
        i128 A = 0;
        for (uint32_t j = 0U; j < 2U * k && j <= i; j++) {
          a += (int64_t)hb15[j] * x15[i - j];
          A += (i128)hb31[j] * x31[i - j];
        }
        int32_t c15 = i + 1U >= k ? x15[i + 1U - k] : 0;
        int32_t c31 = i + 1U >= k ? x31[i + 1U - k] : 0;
        bad += y15[2U * i] != sat((a + 8192) >> 14, -32768, 32767);
        bad += y15[2U * i + 1U] != c15;
        int64_t r = (int64_t)((A + ((i128)1 << 29)) >> 30);
        bad += y31[2U * i] != sat(r, INT32_MIN, INT32_MAX);
        bad += y31[2U * i + 1U] != c31;
      }
      ASSERT_EQ(bad, 0U);
    }
  }
}

/* |H| of the full 4k - 1 tap filter */
static double hb_resp(const float *hb, uint32_t k, double f) {
  double re = 0.5 * cos(TAU * f * (2.0 * k - 1.0)), im = 0.0;
  im = -0.5 * sin(TAU * f * (2.0 * k - 1.0));
  for (uint32_t j = 0U; j < 2U * k; j++) {
    re += (double)hb[j] * cos(TAU * f * 2.0 * j);
    im -= (double)hb[j] * sin(TAU * f * 2.0 * j);
  }
  return sqrt(re * re + im * im);
}

void test_fd_resample_halfband_design(void) {
  static float hb[2U * MAXK];
  ASSERT_EQ(fd_halfband_design(hb, 1U, FD_WIN_RECT), FD_OK);
  ASSERT_NEAR((double)hb[0], 0.25, 1e-7);
  ASSERT_NEAR((double)hb[1], 0.25, 1e-7);
  ASSERT_EQ(fd_halfband_design(hb, 8U, FD_WIN_BLACKMAN), FD_OK);
  double s = 0.0;
  for (uint32_t j = 0U; j < 16U; j++) {
    s += (double)hb[j];
    ASSERT_NEAR((double)hb[j], (double)hb[15U - j], 1e-7);
  }
  ASSERT_NEAR(s, 0.5, 1e-6);
  ASSERT_NEAR(hb_resp(hb, 8U, 0.0), 1.0, 1e-6);
  ASSERT_NEAR(hb_resp(hb, 8U, 0.5), 0.0, 1e-6);
  ASSERT_NEAR(hb_resp(hb, 8U, 0.25), 0.5, 1e-5);
  double worst = 0.0;
  for (double f = 0.36; f <= 0.5; f += 0.001) {
    double v = hb_resp(hb, 8U, f);
    worst = v > worst ? v : worst;
  }
  ASSERT_TRUE(20.0 * log10(worst) < -70.0);
  static fm_q15 q[16];
  ASSERT_EQ(fd_fir_quantize_q15(hb, q, 16U), FD_OK);
}

/* ---- CIC ---- */

/* order moving sums of ratio over x (zero before it), sampled after every
 * ratio-th input, aligned to q31 as the header states */
static uint32_t cic_model(const int32_t *x, uint32_t n, uint32_t order,
                          uint32_t ratio, uint32_t in_bits, int32_t *y) {
  static i128 s[7][NX + 1U];
  for (uint32_t i = 0U; i < n; i++) {
    s[0][i] = x[i];
  }
  for (uint32_t o = 1U; o <= order; o++) {
    i128 run = 0;
    for (uint32_t i = 0U; i < n; i++) {
      run += s[o - 1U][i];
      if (i >= ratio) {
        run -= s[o - 1U][i - ratio];
      }
      s[o][i] = run;
    }
  }
  uint64_t p = 1U;
  uint32_t g = 0U;
  for (uint32_t o = 0U; o < order; o++) {
    p *= ratio;
  }
  while (((i128)1 << g) < (i128)p) {
    g++;
  }
  uint32_t b = in_bits + g, out = 0U;
  for (uint32_t i = ratio - 1U; i < n; i += ratio) {
    i128 v = s[order][i];
    if (b <= 32U) {
      y[out++] = (int32_t)(v * ((i128)1 << (32U - b)));
    } else {
      i128 r = (v + ((i128)1 << (b - 33U))) >> (b - 32U);
      y[out++] = r > INT32_MAX ? INT32_MAX : (int32_t)r;
    }
  }
  return out;
}

static void cic_check(uint32_t order, uint32_t ratio, uint32_t in_bits,
                      int kind) {
  static int32_t xi[NX], want[NX], got[NX];
  static fm_q15 xs[NX];
  const int32_t hi = (int32_t)((1ULL << (in_bits - 1U)) - 1U), lo = -hi - 1;
  for (uint32_t i = 0U; i < NX; i++) {
    uint32_t u = rnd();
    int64_t span = (int64_t)hi - lo + 1;
    xi[i] = kind == 0   ? (int32_t)(lo + (int64_t)(u % (uint64_t)span))
            : kind == 1 ? hi
            : kind == 2 ? lo
                        : ((u & 1U) != 0U ? hi : lo);
    xs[i] = (fm_q15)xi[i];
  }
  FD_Cic c;
  ASSERT_EQ(fd_cic_init(&c, order, ratio, in_bits), FD_OK);
  uint32_t nw = cic_model(xi, NX, order, ratio, in_bits, want), ng = 0U;
  for (uint32_t i = 0U; i < NX;) {
    uint32_t q = 1U + rnd() % 37U;
    q = q > NX - i ? NX - i : q;
    if (in_bits <= 16U) {
      ng += fd_cic_decim_q15(&c, xs + i, got + ng, q);
    } else {
      ng += fd_cic_decim_q31(&c, xi + i, got + ng, q);
    }
    i += q;
  }
  ASSERT_EQ(ng, nw);
  ASSERT_TRUE(memcmp(got, want, nw * sizeof got[0]) == 0);
  if (in_bits <= 16U) { /* the q31 path on the same samples */
    FD_Cic d;
    (void)fd_cic_init(&d, order, ratio, in_bits);
    ASSERT_EQ(fd_cic_decim_q31(&d, xi, got, NX), nw);
    ASSERT_TRUE(memcmp(got, want, nw * sizeof got[0]) == 0);
  }
}

void test_fd_resample_cic(void) {
  static const uint32_t ratios[] = {1U, 2U, 3U, 5U, 8U, 16U, 25U, 64U};
  static const uint32_t bits[] = {1U, 2U, 12U, 16U, 24U, 32U};
  for (uint32_t order = 1U; order <= FD_CIC_MAX_ORDER; order++) {
    for (uint32_t r = 0U; r < sizeof ratios / sizeof ratios[0]; r++) {
      for (uint32_t b = 0U; b < sizeof bits / sizeof bits[0]; b++) {
        FD_Cic c;
        FD_Status st = fd_cic_init(&c, order, ratios[r], bits[b]);
        if (st != FD_OK) {
          continue;
        }
        for (int kind = 0; kind < 4; kind++) {
          cic_check(order, ratios[r], bits[b], kind);
        }
      }
    }
  }
}

/* register width at 32 and 33 bits, the range limit, the gain */
void test_fd_resample_cic_edges(void) {
  FD_Cic c;
  ASSERT_EQ(fd_cic_init(&c, 3U, 64U, 14U), FD_OK); /* 14 + 18 = 32 */
  ASSERT_EQ(c.wide, 0U);
  ASSERT_EQ(fd_cic_init(&c, 3U, 64U, 15U), FD_OK);
  ASSERT_EQ(c.wide, 1U);
  for (int kind = 0; kind < 4; kind++) {
    cic_check(3U, 64U, 14U, kind);
    cic_check(3U, 64U, 15U, kind);
    cic_check(4U, 255U, 32U, kind); /* 32 + 32 = 64 */
  }
  ASSERT_EQ(fd_cic_init(&c, 4U, 256U, 32U), FD_OK);        /* 64 bits */
  ASSERT_EQ(fd_cic_init(&c, 4U, 257U, 32U), FD_ERR_RANGE); /* 65 bits */
  ASSERT_EQ(fd_cic_init(&c, 6U, 65536U, 1U), FD_ERR_RANGE);
  ASSERT_EQ(fd_cic_init(&c, 3U, 64U, 12U), FD_OK);
  ASSERT_EQ(fd_cic_gain(&c), 1 << 30);
  ASSERT_EQ(fd_cic_init(&c, 3U, 48U, 12U), FD_OK); /* 110592 / 2^17 */
  ASSERT_EQ(fd_cic_gain(&c), (int32_t)((110592ULL << 30) >> 17));
  ASSERT_EQ(fd_cic_init(&c, 5U, 3U, 12U), FD_OK); /* 243 / 256 */
  ASSERT_EQ(fd_cic_gain(&c), (int32_t)((243ULL << 30) >> 8));
  ASSERT_EQ(fd_cic_init(&c, 6U, 1000U, 4U), FD_OK); /* 10^18 / 2^60 */
  double want = 1e18 / ldexp(1.0, 60) * ldexp(1.0, 30);
  ASSERT_NEAR((double)fd_cic_gain(&c), want, 0.5);
  /* reset: the same outputs again */
  int32_t a[8], b[8];
  static fm_q15 xs[64];
  for (uint32_t i = 0U; i < 64U; i++) {
    xs[i] = (fm_q15)(rnd() >> 21);
  }
  (void)fd_cic_init(&c, 3U, 8U, 12U);
  ASSERT_EQ(fd_cic_decim_q15(&c, xs, a, 64U), 8U);
  (void)fd_cic_decim_q15(&c, xs, b, 5U);
  fd_cic_reset(&c);
  ASSERT_EQ(fd_cic_decim_q15(&c, xs, b, 64U), 8U);
  ASSERT_TRUE(memcmp(a, b, sizeof a) == 0);
}

void test_fd_resample_cic_comp(void) {
  static float h[33];
  ASSERT_EQ(fd_cic_comp_design(h, 33U, 3U, 16U, 0.0f, FD_WIN_HAMMING),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_cic_comp_design(h, 33U, 3U, 16U, 0.5f, FD_WIN_HAMMING),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_cic_comp_design(h, 33U, 3U, 16U, 0.2f, FD_WIN_HAMMING), FD_OK);
  /* CIC times the compensator: flat to within 0.1 dB over most of the band */
  double worst = 0.0;
  for (double f = 0.0; f <= 0.15; f += 0.005) {
    double cic = 1.0;
    if (f > 0.0) {
      cic =
          pow(fabs(sin((TAU / 2.0) * f) / (16.0 * sin((TAU / 2.0) * f / 16.0))),
              3.0);
    }
    double db = 20.0 * log10(cic * resp(h, 33U, f));
    worst = fabs(db) > worst ? fabs(db) : worst;
  }
  ASSERT_TRUE(worst < 0.1);
  ASSERT_NEAR(resp(h, 33U, 0.0), 1.0, 1e-5);
  ASSERT_TRUE(resp(h, 33U, 0.4) < 0.02);
}
