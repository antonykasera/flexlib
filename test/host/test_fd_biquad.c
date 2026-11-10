/**
 * @file test_fd_biquad.c
 * @brief Host tests for fd_biquad.h and fd_filt.h against exact references
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_biquad.h"
#include "fd_filt.h"
#include "test_stats.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NX 400U
#define MAXSEC 4U
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint64_t rng = 99U;

static i128 floor_shr(i128 v, unsigned s) { return v >> s; }

static int32_t clamp(i128 v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : (int32_t)v);
}

/* the DF1 cascade in 128 bits, per-section history (4 each): f fraction
 * bits, ef = error feedback */
static void ref_cascade(const int32_t *c, uint32_t nsec, uint32_t s, uint32_t f,
                        int ef, i128 lo, i128 hi, int32_t *hist, int32_t *res,
                        const int32_t *x, int32_t *y, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = x[i];
    for (uint32_t k = 0U; k < nsec; k++) {
      const int32_t *q = c + 5U * k;
      int32_t *h = hist + 4U * k; /* x1 x2 y1 y2 */
      i128 acc = ((i128)q[0] * v + (i128)q[1] * h[0] + (i128)q[2] * h[1]) *
                     ((i128)1 << s) +
                 (i128)q[3] * h[2] + (i128)q[4] * h[3];
      int32_t out;
      if (ef) {
        i128 t = acc + res[k];
        i128 qv = floor_shr(t, f);
        if (qv > hi || qv < lo) {
          out = clamp(qv, (int32_t)lo, (int32_t)hi);
          res[k] = 0;
        } else {
          out = (int32_t)qv;
          res[k] = (int32_t)(t - qv * ((i128)1 << f));
        }
      } else {
        out = clamp(floor_shr(acc + ((i128)1 << (f - 1U)), f), (int32_t)lo,
                    (int32_t)hi);
      }
      h[1] = h[0];
      h[0] = v;
      h[3] = h[2];
      h[2] = out;
      v = out;
    }
    y[i] = v;
  }
}

static uint32_t piece(uint32_t left) {
  uint32_t r = test_rng_u32(&rng) % 4U;
  uint32_t p = r == 0U ? 1U : test_rng_u32(&rng) % 37U + 1U;
  return p < left ? p : left;
}

/* stable designs plus random coefficient sets (narrow and wide) */
static void coefs(int32_t *c, uint32_t nsec, uint32_t f, int kind,
                  uint32_t *bshift) {
  if (kind == 0) {
    FD_BiquadDesign d[MAXSEC];
    static const FD_BiquadType t[] = {FD_BQ_LOWPASS, FD_BQ_PEAK,
                                      FD_BQ_HIGHSHELF, FD_BQ_NOTCH};
    for (uint32_t k = 0U; k < nsec; k++) {
      ASSERT_EQ(
          fd_biquad_design(&d[k], t[k], 0.01f + 0.1f * (float)k, 2.0f, 12.0f),
          FD_OK);
    }
    int16_t c15[5U * MAXSEC];
    FD_Status st = f == 30U ? fd_biquad_quantize_q31(d, nsec, c, bshift)
                            : fd_biquad_quantize_q15(d, nsec, c15, bshift);
    ASSERT_EQ(st, FD_OK);
    if (f == 14U) {
      for (uint32_t i = 0U; i < 5U * nsec; i++) {
        c[i] = c15[i];
      }
    }
    return;
  }
  *bshift = test_rng_u32(&rng) % 3U;
  for (uint32_t i = 0U; i < 5U * nsec; i++) {
    uint32_t u = test_rng_u32(&rng);
    int32_t v = f == 30U ? (int32_t)u : (int32_t)(int16_t)(u >> 16);
    c[i] = kind == 1 ? v >> 3 : v; /* kind 1: mostly narrow, kind 2: wide */
  }
}

static void signal(int32_t *x, uint32_t n, uint32_t f, int kind) {
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t u = test_rng_u32(&rng);
    int32_t v = f == 30U ? (int32_t)u : (int32_t)(int16_t)(u >> 16);
    x[i] = kind == 0 ? v >> 4 : v;
  }
}

void test_fd_biquad_q31(void) {
  static int32_t c[5U * MAXSEC], x[NX], r[NX], hist[4U * MAXSEC], res[MAXSEC];
  static fm_q31 y[NX], st[FD_BIQUAD_STATE_LEN(MAXSEC)];
  for (uint32_t nsec = 1U; nsec <= MAXSEC; nsec++) {
    for (int kind = 0; kind < 3; kind++) {
      for (int ef = 0; ef < 2; ef++) {
        for (int rep = 0; rep < 6; rep++) {
          uint32_t sh;
          coefs(c, nsec, 30U, kind, &sh);
          signal(x, NX, 30U, rep & 1);
          memset(hist, 0, sizeof hist);
          memset(res, 0, sizeof res);
          ref_cascade(c, nsec, sh, 30U, ef, INT32_MIN, INT32_MAX, hist, res, x,
                      r, NX);
          FD_BiquadQ31 b;
          if (ef) {
            fd_biquad_init_ef_q31(&b, c, nsec, sh, st);
          } else {
            fd_biquad_init_q31(&b, c, nsec, sh, st);
          }
          for (uint32_t i = 0U, p; i < NX; i += p) {
            p = piece(NX - i);
            fd_biquad_q31(&b, x + i, y + i, p);
          }
          ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
          fd_biquad_reset_q31(&b); /* in place, from a reset */
          memcpy(y, x, sizeof y);
          fd_biquad_q31(&b, y, y, NX);
          ASSERT_TRUE(memcmp(y, r, sizeof y) == 0);
        }
      }
    }
  }
}

void test_fd_biquad_q15(void) {
  static int32_t c[5U * MAXSEC], x[NX], r[NX], hist[4U * MAXSEC], res[MAXSEC];
  static fm_q15 c15[5U * MAXSEC], x15[NX], y[NX],
      st[FD_BIQUAD_STATE_LEN(MAXSEC)];
  for (uint32_t nsec = 1U; nsec <= MAXSEC; nsec++) {
    for (int kind = 0; kind < 3; kind++) {
      for (int rep = 0; rep < 8; rep++) {
        uint32_t sh;
        coefs(c, nsec, 14U, kind, &sh);
        signal(x, NX, 14U, rep & 1);
        for (uint32_t i = 0U; i < 5U * nsec; i++) {
          c15[i] = (fm_q15)c[i];
        }
        for (uint32_t i = 0U; i < NX; i++) {
          x15[i] = (fm_q15)x[i];
        }
        memset(hist, 0, sizeof hist);
        ref_cascade(c, nsec, sh, 14U, 0, -32768, 32767, hist, res, x, r, NX);
        FD_BiquadQ15 b;
        fd_biquad_init_q15(&b, c15, nsec, sh, st);
        for (uint32_t i = 0U, p; i < NX; i += p) {
          p = piece(NX - i);
          fd_biquad_q15(&b, x15 + i, y + i, p);
        }
        for (uint32_t i = 0U; i < NX; i++) {
          ASSERT_EQ(y[i], r[i]);
        }
        fd_biquad_reset_q15(&b);
        memcpy(y, x15, sizeof y);
        fd_biquad_q15(&b, y, y, NX);
        for (uint32_t i = 0U; i < NX; i++) {
          ASSERT_EQ(y[i], r[i]);
        }
      }
    }
  }
}

/* the sum |c| = 4 boundary: the narrow sums would wrap */
void test_fd_biquad_gain4(void) {
  static const fm_q31 c[5] = {INT32_MIN, INT32_MIN, 0, 0, 0}; /* b = -2, -2 */
  static const fm_q15 c15[5] = {-32768, -32768, 0, 0, 0};
  fm_q31 x[4] = {INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN}, y[4],
         st[FD_BIQUAD_STATE_LEN(1U)];
  fm_q15 x15[4] = {-32768, -32768, -32768, -32768}, y15[4],
         st15[FD_BIQUAD_STATE_LEN(1U)];
  FD_BiquadQ31 b;
  FD_BiquadQ15 b15;
  fd_biquad_init_q31(&b, c, 1U, 0U, st);
  fd_biquad_init_q15(&b15, c15, 1U, 0U, st15);
  fd_biquad_q31(&b, x, y, 4U);
  fd_biquad_q15(&b15, x15, y15, 4U);
  ASSERT_EQ(b.wide, 1U);
  ASSERT_EQ(b15.wide, 1U);
  for (int i = 0; i < 4; i++) { /* 4 x full scale, saturated */
    ASSERT_EQ(y[i], INT32_MAX);
    ASSERT_EQ(y15[i], 32767);
  }
}

/* RBJ in double, the textbook way */
static void ref_rbj(double c[5], FD_BiquadType t, double f0, double q,
                    double g) {
  const double pi = 3.14159265358979323846;
  double w = 2 * pi * f0, cs = cos(w), sn = sin(w), al = sn / (2 * q);
  double A = pow(10.0, g / 40.0), sa = 2 * sqrt(A) * al;
  double b0, b1, b2, a0, a1, a2;
  switch (t) {
  case FD_BQ_LOWPASS:
    b0 = b2 = (1 - cs) / 2, b1 = 1 - cs, a0 = 1 + al, a1 = -2 * cs, a2 = 1 - al;
    break;
  case FD_BQ_HIGHPASS:
    b0 = b2 = (1 + cs) / 2, b1 = -(1 + cs), a0 = 1 + al, a1 = -2 * cs,
    a2 = 1 - al;
    break;
  case FD_BQ_BANDPASS:
    b0 = al, b1 = 0, b2 = -al, a0 = 1 + al, a1 = -2 * cs, a2 = 1 - al;
    break;
  case FD_BQ_NOTCH:
    b0 = b2 = 1, b1 = -2 * cs, a0 = 1 + al, a1 = -2 * cs, a2 = 1 - al;
    break;
  case FD_BQ_PEAK:
    b0 = 1 + al * A, b1 = -2 * cs, b2 = 1 - al * A, a0 = 1 + al / A,
    a1 = -2 * cs, a2 = 1 - al / A;
    break;
  case FD_BQ_ALLPASS:
    b0 = 1 - al, b1 = -2 * cs, b2 = 1 + al, a0 = 1 + al, a1 = -2 * cs,
    a2 = 1 - al;
    break;
  case FD_BQ_LOWSHELF:
    b0 = A * ((A + 1) - (A - 1) * cs + sa),
    b1 = 2 * A * ((A - 1) - (A + 1) * cs),
    b2 = A * ((A + 1) - (A - 1) * cs - sa), a0 = (A + 1) + (A - 1) * cs + sa,
    a1 = -2 * ((A - 1) + (A + 1) * cs), a2 = (A + 1) + (A - 1) * cs - sa;
    break;
  default:
    b0 = A * ((A + 1) + (A - 1) * cs + sa),
    b1 = -2 * A * ((A - 1) + (A + 1) * cs),
    b2 = A * ((A + 1) + (A - 1) * cs - sa), a0 = (A + 1) - (A - 1) * cs + sa,
    a1 = 2 * ((A - 1) - (A + 1) * cs), a2 = (A + 1) - (A - 1) * cs - sa;
    break;
  }
  c[0] = b0 / a0, c[1] = b1 / a0, c[2] = b2 / a0;
  c[3] = -a1 / a0, c[4] = -a2 / a0; /* CMSIS signs */
}

void test_fd_biquad_design(void) {
  static const float f0s[] = {1e-4f, 0.003f, 0.05f, 0.2f, 0.45f};
  for (int t = 0; t <= (int)FD_BQ_ALLPASS; t++) {
    for (unsigned fi = 0U; fi < COUNT(f0s); fi++) {
      for (int g = -1; g <= 1; g++) {
        FD_BiquadDesign d;
        float q = 0.4f + 2.0f * (float)fi, gain = 9.0f * (float)g;
        ASSERT_EQ(fd_biquad_design(&d, (FD_BiquadType)t, f0s[fi], q, gain),
                  FD_OK);
        double r[5];
        ref_rbj(r, (FD_BiquadType)t, (double)f0s[fi], (double)q, (double)gain);
        double b[3] = {(double)d.b0, (double)d.b1, (double)d.b2};
        for (int i = 0; i < 3; i++) {
          ASSERT_TRUE(fabs(b[i] - r[i]) <= 2e-6 * (1.0 + fabs(r[i])));
        }
        /* the poles to float precision of the deviation, not of a1 */
        double d1 = 2.0 - r[3], d2 = r[4] + 1.0;
        ASSERT_TRUE(fabs((double)d.d1 - d1) <= 1e-5 * d1 + 1e-12);
        ASSERT_TRUE(fabs((double)d.d2 - d2) <= 1e-5 * d2 + 1e-12);
      }
    }
  }
  FD_BiquadDesign d;
  ASSERT_EQ(fd_biquad_design(&d, FD_BQ_LOWPASS, 0.5f, 1.0f, 0.0f),
            FD_ERR_RANGE);
  ASSERT_EQ(fd_biquad_design(&d, FD_BQ_LOWPASS, 0.1f, 0.0f, 0.0f),
            FD_ERR_RANGE);

  /* Butterworth: -3.01 dB at f0, flat at DC; odd orders are refused */
  FD_BiquadDesign s[4];
  ASSERT_EQ(fd_biquad_butter(s, 8U, FD_LOWPASS, 0.1f), FD_OK);
  double re = 1.0, im = 0.0;
  const double w = 2 * 3.14159265358979323846 * 0.1;
  for (int k = 0; k < 4; k++) { /* H(e^jw), section by section */
    double b0 = (double)s[k].b0, b1 = (double)s[k].b1, b2 = (double)s[k].b2;
    double nr = b0 + b1 * cos(w) + b2 * cos(2 * w);
    double ni = -b1 * sin(w) - b2 * sin(2 * w);
    double a1 = 2.0 - (double)s[k].d1, a2 = (double)s[k].d2 - 1.0;
    double dr = 1 - a1 * cos(w) - a2 * cos(2 * w);
    double di = a1 * sin(w) + a2 * sin(2 * w);
    double hr = (nr * dr + ni * di) / (dr * dr + di * di);
    double hi = (ni * dr - nr * di) / (dr * dr + di * di);
    double tr = re * hr - im * hi;
    im = re * hi + im * hr;
    re = tr;
  }
  ASSERT_NEAR(10 * log10(re * re + im * im), -3.0103, 1e-3);
  ASSERT_EQ(fd_biquad_butter(s, 3U, FD_LOWPASS, 0.1f), FD_ERR_RANGE);

  /* quantise: rounding, the least b shift, and instability after rounding */
  fm_q31 c[10];
  fm_q15 c15[10];
  uint32_t sh;
  ASSERT_EQ(fd_biquad_design(&s[0], FD_BQ_PEAK, 0.1f, 1.0f, 30.0f), FD_OK);
  ASSERT_TRUE(s[0].b0 > 2.0f && s[0].b0 < 4.0f);
  ASSERT_EQ(fd_biquad_quantize_q31(s, 1U, c, &sh), FD_OK);
  ASSERT_EQ(sh, 1U); /* b0 ~ 2.5 */
  ASSERT_EQ(c[0], (int32_t)llround((double)s[0].b0 * (1 << 29)));
  ASSERT_EQ(c[3],
            (int32_t)(2147483648LL - llround((double)s[0].d1 * (1 << 30))));
  ASSERT_EQ(c[4], (int32_t)(llround((double)s[0].d2 * (1 << 30)) - (1 << 30)));
  ASSERT_EQ(fd_biquad_quantize_q15(s, 1U, c15, &sh), FD_OK);
  ASSERT_EQ(sh, 1U);
  ASSERT_EQ(c15[0], (int32_t)lround((double)s[0].b0 * (1 << 13)));
  FD_BiquadDesign u = {0.0f, 0.0f, 0.0f, 1e-12f, 1e-12f}; /* rounds to z = 1 */
  ASSERT_EQ(fd_biquad_quantize_q31(&u, 1U, c, &sh), FD_ERR_UNSTABLE);
  u.d1 = 4.5f;
  ASSERT_EQ(fd_biquad_quantize_q31(&u, 1U, c, &sh), FD_ERR_RANGE);
}

/* error feedback lowers the noise of a low-frequency q31 lowpass */
void test_fd_biquad_ef(void) {
  FD_BiquadDesign s[2];
  fm_q31 c[10], st[FD_BIQUAD_STATE_LEN(2U)], st2[FD_BIQUAD_STATE_LEN(2U)];
  uint32_t sh;
  ASSERT_EQ(fd_biquad_butter(s, 4U, FD_LOWPASS, 0.0005f), FD_OK);
  ASSERT_EQ(fd_biquad_quantize_q31(s, 2U, c, &sh), FD_OK);
  FD_BiquadQ31 p, e;
  fd_biquad_init_q31(&p, c, 2U, sh, st);
  fd_biquad_init_ef_q31(&e, c, 2U, sh, st2);
  /* reference: the quantised coefficients in double */
  double h[8] = {0}, cf[10];
  for (int i = 0; i < 10; i++) {
    cf[i] = c[i] / 1073741824.0 * ((i % 5) < 3 ? (double)(1 << sh) : 1.0);
  }
  SnrStats sp, se;
  snr_init(&sp, "biquad q31, fc 5e-4");
  snr_init(&se, "biquad q31 ef, fc 5e-4");
  for (int i = 0; i < 200000; i++) {
    double xv = 0.5 * sin(2 * 3.14159265358979 * 1e-4 * i);
    fm_q31 x = (fm_q31)lround(xv * 2147483648.0), yp, ye;
    fd_biquad_q31(&p, &x, &yp, 1U);
    fd_biquad_q31(&e, &x, &ye, 1U);
    double v = x;
    for (int k = 0; k < 2; k++) {
      const double *q = cf + 5 * k;
      double *g = h + 4 * k; /* x1 x2 y1 y2 */
      double out =
          q[0] * v + q[1] * g[0] + q[2] * g[1] + q[3] * g[2] + q[4] * g[3];
      g[1] = g[0];
      g[0] = v;
      g[3] = g[2];
      g[2] = out;
      v = out;
    }
    if (i > 20000) {
      snr_add(&sp, yp, v);
      snr_add(&se, ye, v);
    }
  }
  snr_print(&sp);
  snr_print(&se);
  ASSERT_TRUE(snr_db(&se) > snr_db(&sp) + 40.0);
}

/* ---- fd_filt ---- */

void test_fd_filt(void) {
  static fm_q15 x[NX], y[NX], buf[300];
  static fm_q31 X[NX], Y[NX], BUF[300];
  for (int rep = 0; rep < 40; rep++) {
    for (uint32_t i = 0U; i < NX; i++) {
      uint32_t u = test_rng_u32(&rng);
      x[i] = (rep & 1) != 0 ? (fm_q15)((u & 1U) != 0U ? 32767 : -32768)
                            : (fm_q15)(u >> 16);
      X[i] =
          (rep & 1) != 0 ? ((u & 1U) != 0U ? INT32_MAX : INT32_MIN) : (fm_q31)u;
    }
    fm_q15 a = (fm_q15)(test_rng_u32(&rng) >> 17);
    fm_q31 A = (fm_q31)(test_rng_u32(&rng) >> 1);
    if (rep == 2) {
      a = 32767;
      A = INT32_MAX;
    }

    FD_DcBlockQ15 d;
    FD_DcBlockQ31 D;
    fd_dcblock_init_q15(&d, a);
    fd_dcblock_init_q31(&D, A);
    fd_dcblock_q15(&d, x, y, NX / 2U);
    fd_dcblock_q15(&d, x + NX / 2U, y + NX / 2U, NX / 2U);
    fd_dcblock_q31(&D, X, Y, NX);
    i128 x1 = 0, y1 = 0, X1 = 0, Y1 = 0;
    for (uint32_t i = 0U; i < NX; i++) {
      i128 v = ((i128)(x[i] - x1) * 32768 + (i128)a * y1 + 16384) >> 15;
      y1 = clamp(v, -32768, 32767);
      x1 = x[i];
      ASSERT_EQ(y[i], (int64_t)y1);
      i128 V = (((i128)X[i] - X1) * ((i128)1 << 31) + (i128)A * Y1 +
                ((i128)1 << 30)) >>
               31;
      Y1 = clamp(V, INT32_MIN, INT32_MAX);
      X1 = X[i];
      ASSERT_EQ(Y[i], (int64_t)Y1);
    }

    FD_OnePoleQ15 p;
    FD_OnePoleQ31 P;
    fd_onepole_init_q15(&p, a, (fm_q15)x[0]);
    fd_onepole_init_q31(&P, A, X[0]);
    fd_onepole_q15(&p, x, y, NX);
    fd_onepole_q31(&P, X, Y, NX);
    i128 v = x[0], V = X[0];
    for (uint32_t i = 0U; i < NX; i++) {
      v += ((i128)a * (x[i] - v) + 16384) >> 15;
      V += ((i128)A * (X[i] - V) + ((i128)1 << 30)) >> 31;
      ASSERT_EQ(y[i], (int64_t)v);
      ASSERT_EQ(Y[i], (int64_t)V);
    }

    static const uint32_t lens[] = {1U, 2U, 3U, 7U, 16U, 100U, 256U, 300U};
    uint32_t n = lens[(uint32_t)rep % COUNT(lens)];
    FD_MovAvgQ15 m;
    FD_MovAvgQ31 M;
    fd_movavg_init_q15(&m, buf, n);
    fd_movavg_init_q31(&M, BUF, n);
    for (uint32_t i = 0U, q; i < NX; i += q) {
      q = piece(NX - i);
      fd_movavg_q15(&m, x + i, y + i, q);
      fd_movavg_q31(&M, X + i, Y + i, q);
    }
    for (uint32_t i = 0U; i < NX; i++) {
      i128 s = 0, S = 0;
      for (uint32_t j = 0U; j < n && j <= i; j++) {
        s += x[i - j];
        S += X[i - j];
      }
      i128 r = s >= 0 ? (2 * s + n) / (2 * (i128)n)
                      : -((-2 * s - n + 2 * (i128)n - 1) / (2 * (i128)n));
      i128 R = S >= 0 ? (2 * S + n) / (2 * (i128)n)
                      : -((-2 * S - n + 2 * (i128)n - 1) / (2 * (i128)n));
      ASSERT_EQ(y[i], (int64_t)r);
      ASSERT_EQ(Y[i], (int64_t)R);
    }
  }
  fm_q15 big[65536];
  FD_MovAvgQ15 m;
  fd_movavg_init_q15(&m, big, 65536U); /* the widest window: |sum| = 2^31 */
  fm_q15 lo = -32768, out = 0;
  for (int i = 0; i < 70000; i++) {
    fd_movavg_q15(&m, &lo, &out, 1U);
  }
  ASSERT_EQ(out, -32768);

  int32_t v[5];
  for (int rep = 0; rep < 100000; rep++) {
    for (int i = 0; i < 5; i++) {
      v[i] = (int32_t)(test_rng_u32(&rng) % 7U) - 3;
    }
    int32_t s3[3] = {v[0], v[1], v[2]}, s5[5];
    memcpy(s5, v, sizeof s5);
    for (int i = 0; i < 5; i++) { /* insertion sorts */
      for (int j = i; j > 0 && s5[j] < s5[j - 1]; j--) {
        int32_t t = s5[j];
        s5[j] = s5[j - 1];
        s5[j - 1] = t;
      }
    }
    for (int i = 0; i < 3; i++) {
      for (int j = i; j > 0 && s3[j] < s3[j - 1]; j--) {
        int32_t t = s3[j];
        s3[j] = s3[j - 1];
        s3[j - 1] = t;
      }
    }
    ASSERT_EQ(fd_median3(v[0], v[1], v[2]), s3[1]);
    ASSERT_EQ(fd_median5(v[0], v[1], v[2], v[3], v[4]), s5[2]);
  }
  ASSERT_EQ(fd_median3(INT32_MIN, INT32_MAX, 0), 0);
}

/* exact boundaries: saturation at +-2^31, ties, the shift in the gain sum */
void test_fd_biquad_edges(void) {
  fm_q31 st[FD_BIQUAD_STATE_LEN(1U)], y[2];
  FD_BiquadQ31 b;
  /* acc = 2^61 is 2^31 out: saturates */
  static const fm_q31 neg1[5] = {-(1 << 30), 0, 0, 0, 0};
  fm_q31 x[2] = {INT32_MIN, INT32_MIN};
  fd_biquad_init_q31(&b, neg1, 1U, 0U, st);
  fd_biquad_q31(&b, x, y, 1U);
  ASSERT_EQ(y[0], INT32_MAX);
  /* acc = -2^61 + 2^30 is -2^31 + 1 */
  static const fm_q31 c2[5] = {1 << 30, 1, 0, 0, 0};
  x[0] = 1 << 30;
  x[1] = INT32_MIN;
  fd_biquad_init_q31(&b, c2, 1U, 0U, st);
  fd_biquad_q31(&b, x, y, 2U);
  ASSERT_EQ(y[1], INT32_MIN + 1);
  /* ties: acc = +-2^29 rounds up, to 1 and 0 */
  static const fm_q31 tiny[5] = {1, 0, 0, 0, 0};
  x[0] = 1 << 29;
  x[1] = -(1 << 29);
  fd_biquad_init_q31(&b, tiny, 1U, 0U, st);
  fd_biquad_q31(&b, x, y, 2U);
  ASSERT_EQ(y[0], 1);
  ASSERT_EQ(y[1], 0);
  fm_q15 s15[FD_BIQUAD_STATE_LEN(1U)], x15[2] = {1 << 13, -(1 << 13)}, y15[2];
  static const fm_q15 tiny15[5] = {1, 0, 0, 0, 0};
  FD_BiquadQ15 b15;
  fd_biquad_init_q15(&b15, tiny15, 1U, 0U, s15);
  fd_biquad_q15(&b15, x15, y15, 2U);
  ASSERT_EQ(y15[0], 1);
  ASSERT_EQ(y15[1], 0);
  /* sum |c| = 3 stored, 6 with the shift: wide */
  static const fm_q31 big[5] = {1 << 30, 1 << 30, 1 << 30, 0, 0};
  static const fm_q15 big15[5] = {1 << 14, 1 << 14, 1 << 14, 0, 0};
  fd_biquad_init_q31(&b, big, 1U, 1U, st);
  fd_biquad_init_q15(&b15, big15, 1U, 1U, s15);
  ASSERT_EQ(b.wide, 1U);
  ASSERT_EQ(b15.wide, 1U);
  fm_q31 xs[3] = {INT32_MIN, INT32_MIN, INT32_MIN}, ys[3];
  fd_biquad_q31(&b, xs, ys, 3U);
  ASSERT_EQ(ys[2], INT32_MIN); /* 3 x 2 x -1 */

  /* quantise: a b that rounds to exactly 2 in Q2.14 needs the next shift */
  FD_BiquadDesign d = {1.99999988f, 0.0f, 0.0f, 1.0f, 0.5f};
  fm_q15 c15[5];
  fm_q31 c31[5];
  uint32_t sh;
  ASSERT_EQ(fd_biquad_quantize_q15(&d, 1U, c15, &sh), FD_OK);
  ASSERT_EQ(sh, 1U);
  ASSERT_EQ(c15[0], 16384);
  /* a pole exactly at z = 1 (d1 = d2: a1 + a2 = 1) */
  FD_BiquadDesign p = {0.1f, 0.0f, 0.0f, 0.5f, 0.5f};
  ASSERT_EQ(fd_biquad_quantize_q31(&p, 1U, c31, &sh), FD_ERR_UNSTABLE);
  /* d1 rounding to 0 is clamped below 2 */
  FD_BiquadDesign u = {0.0f, 0.0f, 0.0f, 1e-12f, 0.5f};
  ASSERT_EQ(fd_biquad_quantize_q31(&u, 1U, c31, &sh), FD_ERR_UNSTABLE);
  ASSERT_EQ(c31[3], INT32_MAX);

  /* fd_filt ties: a = 0.5 times 1 LSB rounds up */
  FD_DcBlockQ31 dc;
  FD_OnePoleQ31 op;
  fm_q31 one[2] = {1, 1}, o[2];
  fd_dcblock_init_q31(&dc, 1 << 30);
  fd_dcblock_q31(&dc, one, o, 2U);
  ASSERT_EQ(o[1], 1);
  fd_onepole_init_q31(&op, 1 << 30, 0);
  fd_onepole_q31(&op, one, o, 1U);
  ASSERT_EQ(o[0], 1);

  /* the one-pole coefficient puts -3.01 dB at fc */
  const float fcs[3] = {0.001f, 0.05f, 0.3f};
  for (int i = 0; i < 3; i++) {
    double a = fd_onepole_coef_q31(fcs[i]) / 2147483648.0;
    double w = 2 * 3.14159265358979323846 * (double)fcs[i];
    double re = 1 - (1 - a) * cos(w), im = (1 - a) * sin(w);
    ASSERT_NEAR(20 * log10(a / sqrt(re * re + im * im)), -3.0103, 1e-3);
    double a15 = fd_onepole_coef_q15(fcs[i]) / 32768.0;
    ASSERT_NEAR(a15, a, 1.0 / 32768.0);
  }
}
