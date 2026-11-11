/**
 * @file test_fd_fft.c
 * @brief Host tests for fd_fft.h: an integer model of every stage, the DFT
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: MIT
 */

#include "fd_fft.h"
#include "fd_fft_core.h"
#include "fd_vec.h"
#include "fm_exp.h"
#include "ref_dsp.h"
#include "test_stats.h"
#include <math.h>
#include <string.h>

__extension__ typedef __int128 i128;

#define NMAX FD_FFT_MAX
#define QP FD_FFT_QP

static uint64_t rng = 0x5EEDF00DU;
static const double two_pi = 6.283185307179586476925286766559;

/* ---- the model: group-major, 128-bit sums, explicit floors ---- */

static unsigned model_overflows; /* stage outputs outside their type */

static i128 fl(i128 v, uint32_t k) {
  i128 d = (i128)1 << k;
  i128 q = v / d;
  return (v % d != 0 && v < 0) ? q - 1 : q;
}

/* nearest, ties down */
static i128 rd(i128 v, uint32_t k) {
  return k == 0U ? v : fl(v + ((i128)1 << (k - 1U)) - 1, k);
}

static int64_t fit(i128 v, unsigned bits) {
  i128 lim = (i128)1 << (bits - 1U);
  if (v < -lim || v >= lim) {
    model_overflows++;
  }
  return (int64_t)v;
}

/* sin, cos of a / (4 QP) turns from the quarter table, by quadrant */
static void m_sincos(uint32_t a, int64_t *c, int64_t *s) {
  const uint32_t *t = FD_FFT_TAB;
  uint32_t r = a % QP;
  switch ((a / QP) & 3U) {
  case 0:
    *s = t[r];
    *c = t[QP - r];
    break;
  case 1:
    *s = t[QP - r];
    *c = -(int64_t)t[r];
    break;
  case 2:
    *s = -(int64_t)t[r];
    *c = -(int64_t)t[QP - r];
    break;
  default:
    *s = -(int64_t)t[QP - r];
    *c = t[r];
    break;
  }
}

/* a Q31 table value over 2^ts, its magnitude rounded */
static int64_t m_scale(int64_t v, uint32_t ts) {
  if (ts == 0U) {
    return v;
  }
  int64_t m = v < 0 ? -v : v;
  m = (m + (1LL << (ts - 1U))) >> ts;
  return v < 0 ? -m : m;
}

static uint32_t m_bits(const int64_t *v, uint32_t cnt) {
  uint32_t b = 0U;
  for (uint32_t i = 0U; i < cnt; i++) {
    while (v[i] < -(1LL << b) || v[i] >= (1LL << b)) {
      b++;
    }
  }
  return b;
}

static uint32_t m_log2(uint32_t n) {
  uint32_t m = 0U;
  while ((1U << m) < n) {
    m++;
  }
  return m;
}

static void m_swap(int64_t *x, uint32_t a, uint32_t b) {
  int64_t r = x[2U * a], i = x[2U * a + 1U];
  x[2U * a] = x[2U * b];
  x[2U * a + 1U] = x[2U * b + 1U];
  x[2U * b] = r;
  x[2U * b + 1U] = i;
}

static void m_bitrev(int64_t *x, uint32_t n) {
  uint32_t m = m_log2(n);
  for (uint32_t i = 0U; i < n; i++) {
    uint32_t r = 0U;
    for (uint32_t k = 0U; k < m; k++) {
      r |= ((i >> k) & 1U) << (m - 1U - k);
    }
    if (i < r) {
      m_swap(x, i, r);
    }
  }
}

static void m_flip(int64_t *x, uint32_t n) {
  for (uint32_t k = 1U; k < n - k; k++) {
    m_swap(x, k, n - k);
  }
}

/* v 2^s: a multiply, since << of a negative value is undefined */
static i128 sl(i128 v, uint32_t s) { return v * ((i128)1 << s); }

static i128 m_shift(i128 s, int32_t sh) {
  return sh > 0 ? rd(s, (uint32_t)sh) : s * ((i128)1 << (uint32_t)-sh);
}

/* P = W x with W = c - i s */
static void m_mul(i128 xr, i128 xi, i128 c, i128 s, i128 *pr, i128 *pi) {
  *pr = xr * c + xi * s;
  *pi = xi * c - xr * s;
}

/* one stage on 2n values: q = 0 is the radix-2 pairs */
static void m_stage(int64_t *x, uint32_t n, uint32_t q, int32_t sh, uint32_t b,
                    int q31) {
  const unsigned bits = q31 ? 32U : 16U;
  if (q == 0U) {
    for (uint32_t k = 0U; k < n; k += 2U) {
      int64_t *p = x + 2U * k;
      i128 ar = p[0], ai = p[1], br = p[2], bi = p[3];
      p[0] = fit(m_shift(ar + br, sh), bits);
      p[1] = fit(m_shift(ai + bi, sh), bits);
      p[2] = fit(m_shift(ar - br, sh), bits);
      p[3] = fit(m_shift(ai - bi, sh), bits);
    }
    return;
  }
  for (uint32_t base = 0U; base < n; base += 4U * q) {
    for (uint32_t j = 0U; j < q; j++) {
      int64_t *v[4];
      for (uint32_t i = 0U; i < 4U; i++) {
        v[i] = x + 2U * (base + j + i * q);
      }
      i128 y[8];
      if (j == 0U) {
        i128 s[8] = {
            (i128)v[0][0] + v[1][0] + v[2][0] + v[3][0],
            (i128)v[0][1] + v[1][1] + v[2][1] + v[3][1],
            (i128)v[0][0] - v[1][0] + v[2][1] - v[3][1],
            (i128)v[0][1] - v[1][1] - v[2][0] + v[3][0],
            (i128)v[0][0] + v[1][0] - v[2][0] - v[3][0],
            (i128)v[0][1] + v[1][1] - v[2][1] - v[3][1],
            (i128)v[0][0] - v[1][0] - v[2][1] + v[3][1],
            (i128)v[0][1] - v[1][1] + v[2][0] - v[3][0],
        };
        for (uint32_t i = 0U; i < 8U; i++) {
          y[i] = m_shift(s[i], sh);
        }
      } else {
        uint32_t ps, k, xs, ts;
        if (!q31) {
          ps = b >= 14U ? 2U : 0U;
          xs = 15U - ps;
          k = (uint32_t)(15 + sh) - ps;
          ts = 16U;
        } else {
          ps = 0U;
          ts = sh >= 1 ? (uint32_t)sh - 1U : 0U;
          xs = 31U - ts;
          k = sh >= 1 ? 32U : (uint32_t)(31 + sh);
        }
        /* x[p + q] by W^2j, x[p + 2q] by W^j, x[p + 3q] by W^3j */
        static const uint32_t mult[4] = {0U, 2U, 1U, 3U};
        i128 pr[4], pi[4];
        for (uint32_t i = 1U; i < 4U; i++) {
          int64_t c, s;
          m_sincos(mult[i] * j * (QP / q), &c, &s);
          m_mul(v[i][0], v[i][1], m_scale(c, ts), m_scale(s, ts), &pr[i],
                &pi[i]);
          pr[i] = fl(pr[i], ps);
          pi[i] = fl(pi[i], ps);
        }
        i128 x0r = sl(v[0][0], xs), x0i = sl(v[0][1], xs);
        i128 s[8] = {
            x0r + pr[1] + pr[2] + pr[3], x0i + pi[1] + pi[2] + pi[3],
            x0r - pr[1] + pi[2] - pi[3], x0i - pi[1] - pr[2] + pr[3],
            x0r + pr[1] - pr[2] - pr[3], x0i + pi[1] - pi[2] - pi[3],
            x0r - pr[1] - pi[2] + pi[3], x0i - pi[1] + pr[2] - pr[3],
        };
        for (uint32_t i = 0U; i < 8U; i++) {
          y[i] = rd(s[i], k);
        }
      }
      for (uint32_t i = 0U; i < 4U; i++) {
        v[i][0] = fit(y[2U * i], bits);
        v[i][1] = fit(y[2U * i + 1U], bits);
      }
    }
  }
}

#define M_ZERO INT32_MIN

static int32_t m_cfft(int64_t *x, uint32_t n, int q31) {
  const int32_t hb = q31 ? 31 : 15; /* bound shifts: hb - 1, -2, -3 */
  uint32_t b = m_bits(x, 2U * n);
  int allzero = 1;
  for (uint32_t i = 0U; i < 2U * n; i++) {
    allzero &= x[i] == 0;
  }
  if (allzero) {
    return M_ZERO;
  }
  m_bitrev(x, n);
  uint32_t m = m_log2(n), q = 1U;
  int32_t e = 0;
  if ((m & 1U) != 0U) {
    int32_t sh = (int32_t)b - (hb - 1);
    m_stage(x, n, 0U, sh, b, q31);
    e = sh;
    q = 2U;
    b = m_bits(x, 2U * n);
  }
  for (; 4U * q <= n; q *= 4U) {
    int32_t sh = (int32_t)b - (q == 1U ? hb - 2 : hb - 3);
    m_stage(x, n, q, sh, b, q31);
    e += sh;
    b = m_bits(x, 2U * n);
  }
  return e;
}

static int32_t m_icfft(int64_t *x, uint32_t n, int q31) {
  int32_t e = m_cfft(x, n, q31);
  if (e == M_ZERO) {
    return 0;
  }
  m_flip(x, n);
  return e - (int32_t)m_log2(n);
}

/* the real transform's pass (join: its inverse) over m = n/2 values */
static void m_pass(int64_t *x, uint32_t n, int32_t sh, uint32_t b, int q31,
                   int join) {
  const uint32_t m = n / 2U;
  const unsigned bits = q31 ? 32U : 16U;
  uint32_t ps, k, xs, ts;
  if (!q31) {
    ps = b >= 14U ? 2U : 0U;
    xs = 15U - ps;
    k = (uint32_t)(15 + sh) - ps;
    ts = 16U;
  } else {
    ps = 0U;
    ts = sh >= 1 ? (uint32_t)sh - 1U : 0U;
    xs = 31U - ts;
    k = sh >= 1 ? 32U : (uint32_t)(31 + sh);
  }
  i128 ar = x[0], ai = x[1];
  i128 f = join ? 1 : 2;
  x[0] = fit(rd(sl(f * (ar + ai), xs), k), bits);
  x[1] = fit(rd(sl(f * (ar - ai), xs), k), bits);
  ar = x[m];
  ai = x[m + 1U];
  x[m] = fit(rd(sl(2 * ar, xs), k), bits);
  x[m + 1U] = fit(rd(sl(-2 * ai, xs), k), bits);
  for (uint32_t i = 1U; i < m / 2U; i++) {
    int64_t c64, s64;
    m_sincos(i * (4U * QP / n), &c64, &s64);
    i128 c = m_scale(c64, ts), s = m_scale(s64, ts);
    int64_t *lo = x + 2U * i, *hi = x + 2U * (m - i);
    i128 pr = lo[0], pi = lo[1], qr = hi[0], qi = hi[1];
    i128 er = sl(pr + qr, xs), ei = sl(pi - qi, xs);
    i128 y0, y1, y2, y3;
    if (!join) {
      i128 u = pi + qi, d = pr - qr;
      i128 br = fl(c * u, ps) - fl(s * d, ps);
      i128 bi = -(fl(c * d, ps) + fl(s * u, ps));
      y0 = er + br;
      y1 = ei + bi;
      y2 = er - br;
      y3 = bi - ei;
    } else {
      i128 dr = pr - qr, di = pi + qi;
      i128 or_ = fl(c * dr, ps) - fl(s * di, ps);
      i128 oi = fl(s * dr, ps) + fl(c * di, ps);
      y0 = er - oi;
      y1 = ei + or_;
      y2 = er + oi;
      y3 = or_ - ei;
    }
    lo[0] = fit(rd(y0, k), bits);
    lo[1] = fit(rd(y1, k), bits);
    hi[0] = fit(rd(y2, k), bits);
    hi[1] = fit(rd(y3, k), bits);
  }
}

static int32_t m_rfft(int64_t *x, uint32_t n, int q31) {
  int32_t e = m_cfft(x, n / 2U, q31);
  if (e == M_ZERO) {
    return 0;
  }
  uint32_t b = m_bits(x, n);
  int32_t sh = (int32_t)b - (q31 ? 28 : 12);
  m_pass(x, n, sh, b, q31, 0);
  return e + sh - 1;
}

static int32_t m_irfft(int64_t *x, uint32_t n, int q31) {
  uint32_t b = m_bits(x, n);
  int32_t sh = (int32_t)b - (q31 ? 28 : 12);
  m_pass(x, n, sh, b, q31, 1);
  int32_t e = m_icfft(x, n / 2U, q31);
  int allzero = 1;
  for (uint32_t i = 0U; i < n; i++) {
    allzero &= x[i] == 0;
  }
  return allzero ? 0 : e + sh - 1;
}

/* ---- inputs ---- */

static int64_t m_x[2U * NMAX], m_y[2U * NMAX];
static fd_cq15 b15[NMAX];
static fd_cq31 b31[NMAX];
static double dre[NMAX], dim[NMAX];

enum { KINDS = 15 };

static int64_t clamp(double v, int64_t lo, int64_t hi) {
  double r = floor(v + 0.5);
  return r < (double)lo ? lo : (r > (double)hi ? hi : (int64_t)r);
}

/* 2 len values of kind k at full scale for the type */
static void fill(int64_t *v, uint32_t len, int kind, int q31) {
  const int64_t hi = q31 ? INT32_MAX : INT16_MAX, lo = -hi - 1;
  const double a = (double)hi;
  double f1 = 1.0 + (double)(test_rng_u32(&rng) % (len > 4U ? len / 4U : 1U));
  double f2 = f1 * 2.37 + 0.31;
  uint32_t shift = test_rng_u32(&rng) % (q31 ? 31U : 15U);
  for (uint32_t i = 0U; i < 2U * len; i++) {
    uint32_t t = i / 2U, re = (i & 1U) == 0U;
    double ph = two_pi * f1 * t / len, ph2 = two_pi * f2 * t / len;
    int64_t r = q31 ? (int64_t)(int32_t)test_rng_u32(&rng)
                    : (int64_t)(int16_t)test_rng_u32(&rng);
    switch (kind) {
    case 0:
      v[i] = 0;
      break;
    case 1:
      v[i] = -1;
      break;
    case 2:
      v[i] = i == 0U ? hi : 0;
      break;
    case 3:
      v[i] = i == 2U * (len / 3U) + 1U ? lo : 0;
      break;
    case 4:
      v[i] = hi;
      break;
    case 5:
      v[i] = lo;
      break;
    case 6:
      v[i] = (t & 1U) != 0U ? lo : hi;
      break;
    case 7:
      v[i] = clamp(a * (re ? cos(ph) : -sin(ph)), lo, hi);
      break;
    case 8:
      v[i] = clamp(a * (re ? cos(ph * 1.013) : sin(ph * 0.987)), lo, hi);
      break;
    case 9:
      v[i] = clamp(a * 0.5 * (re ? cos(ph) + cos(ph2) : sin(ph2)), lo, hi);
      break;
    case 10:
      v[i] = r;
      break;
    case 11:
      v[i] = r / (q31 ? 1048576 : 1000);
      break;
    case 12:
      v[i] = r >> shift;
      break;
    case 13:
      v[i] = (r & 1) != 0 ? hi : lo;
      break;
    default:
      v[i] = r >= 0 ? (r & 1) + hi - 1 : lo + (r & 1);
      break;
    }
  }
}

static void to15(const int64_t *v, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    b15[i].re = (fm_q15)v[2U * i];
    b15[i].im = (fm_q15)v[2U * i + 1U];
  }
}

static void to31(const int64_t *v, uint32_t n) {
  for (uint32_t i = 0U; i < n; i++) {
    b31[i].re = (fm_q31)v[2U * i];
    b31[i].im = (fm_q31)v[2U * i + 1U];
  }
}

static unsigned diff15(const int64_t *v, uint32_t n) {
  unsigned d = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    d += b15[i].re != v[2U * i] || b15[i].im != v[2U * i + 1U];
  }
  return d;
}

static unsigned diff31(const int64_t *v, uint32_t n) {
  unsigned d = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    d += b31[i].re != v[2U * i] || b31[i].im != v[2U * i + 1U];
  }
  return d;
}

/* ---- tests ---- */

void test_fd_fft_scan(void) {
  for (int32_t v = -32768; v <= 32767; v++) {
    int64_t m[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    m[(uint32_t)(v & 7)] = v;
    to15(m, 4U);
    ASSERT_EQ(fd_fft15_scan(&b15[0].re, 4U), m_bits(m, 8U));
    ASSERT_EQ(fd_headroom_q15(&b15[0].re, 8U), 15U - m_bits(m, 8U));
  }
  /* re's sign bit next to im's bit 0 in the word */
  int64_t m[2] = {-32768, 0};
  to15(m, 1U);
  ASSERT_EQ(fd_fft15_scan(&b15[0].re, 1U), 15U);
  m[0] = -1;
  m[1] = 0;
  to15(m, 1U);
  ASSERT_EQ(fd_fft15_scan(&b15[0].re, 1U), 0U);
  for (uint32_t k = 0U; k < 31U; k++) {
    int64_t e[6] = {1LL << k,        (1LL << k) - 1,  -(1LL << k),
                    -(1LL << k) - 1, -(1LL << k) + 1, 0};
    for (uint32_t i = 0U; i < 6U; i++) {
      int64_t w[4] = {0, 0, 0, 0};
      w[i % 4U] = e[i];
      to31(w, 2U);
      ASSERT_EQ(fd_fft31_scan(&b31[0].re, 2U), m_bits(w, 4U));
      ASSERT_EQ(fd_headroom_q31(&b31[0].re, 4U), 31U - m_bits(w, 4U));
    }
  }
  int64_t w[2] = {INT32_MIN, INT32_MAX};
  to31(w, 1U);
  ASSERT_EQ(fd_fft31_scan(&b31[0].re, 1U), 31U);
  ASSERT_EQ(fd_headroom_q31(&b31[0].re, 2U), 0U);
  ASSERT_EQ(fd_headroom_q15(&b15[0].re, 0U), 15U);
}

/* the table and the quadrant logic against long double, every angle */
void test_fd_fft_twiddles(void) {
  double e31 = 0.0, e15 = 0.0;
  for (uint32_t a = 0U; a < 4U * QP; a++) {
    int64_t c, s;
    m_sincos(a, &c, &s);
    long double t = (long double)two_pi * a / (4.0L * QP);
    long double cx = cosl(t) * 2147483648.0L, sx = sinl(t) * 2147483648.0L;
    e31 = fmax(e31, (double)fabsl((long double)c - cx));
    e31 = fmax(e31, (double)fabsl((long double)s - sx));
    e15 = fmax(e15, (double)fabsl((long double)m_scale(c, 16U) - cx / 65536));
    e15 = fmax(e15, (double)fabsl((long double)m_scale(s, 16U) - sx / 65536));
  }
  printf("  twiddles: q31 %.4f LSB, q15 %.6f LSB\n", e31, e15);
  ASSERT_TRUE(e31 <= 0.5);
  ASSERT_TRUE(e15 <= 0.5 + 1.0 / 65536);
}

/* every size, every input kind: the kernels equal the model bit for bit */
void test_fd_fft_model(void) {
  unsigned bad = 0U, runs = 0U;
  model_overflows = 0U;
  for (uint32_t n = 2U; n <= NMAX; n *= 2U) {
    for (int kind = 0; kind < KINDS; kind++) {
      for (int rep = 0; rep < (kind >= 7 ? 4 : 1); rep++) {
        fill(m_x, n, kind, 0);
        memcpy(m_y, m_x, sizeof(int64_t) * 2U * n);
        to15(m_x, n);
        int32_t e = fd_cfft_q15(b15, n);
        int32_t me = m_cfft(m_x, n, 0);
        bad += (e != (me == M_ZERO ? 0 : me)) + diff15(m_x, n);
        to15(m_y, n);
        e = fd_icfft_q15(b15, n);
        me = m_icfft(m_y, n, 0);
        bad += (e != me) + diff15(m_y, n);

        fill(m_x, n, kind, 1);
        memcpy(m_y, m_x, sizeof(int64_t) * 2U * n);
        to31(m_x, n);
        e = fd_cfft_q31(b31, n);
        me = m_cfft(m_x, n, 1);
        bad += (e != (me == M_ZERO ? 0 : me)) + diff31(m_x, n);
        to31(m_y, n);
        e = fd_icfft_q31(b31, n);
        me = m_icfft(m_y, n, 1);
        bad += (e != me) + diff31(m_y, n);
        runs += 4U;
      }
    }
  }
  printf("  %u transforms, %u differences, %u model overflows\n", runs, bad,
         model_overflows);
  ASSERT_EQ(bad, 0U);
  ASSERT_EQ(model_overflows, 0U);
}

void test_fd_fft_real_model(void) {
  unsigned bad = 0U, runs = 0U;
  model_overflows = 0U;
  static fm_q15 r15[NMAX] __attribute__((aligned(4)));
  static fm_q31 r31[NMAX];
  for (uint32_t n = 4U; n <= NMAX; n *= 2U) {
    for (int kind = 0; kind < KINDS; kind++) {
      for (int rep = 0; rep < (kind >= 7 ? 4 : 1); rep++) {
        for (int q31 = 0; q31 <= 1; q31++) {
          fill(m_x, n / 2U, kind, q31); /* n reals */
          memcpy(m_y, m_x, sizeof(int64_t) * n);
          for (uint32_t i = 0U; i < n; i++) {
            if (q31) {
              r31[i] = (fm_q31)m_x[i];
            } else {
              r15[i] = (fm_q15)m_x[i];
            }
          }
          int32_t e = q31 ? fd_rfft_q31(r31, n) : fd_rfft_q15(r15, n);
          int32_t me = m_rfft(m_x, n, q31);
          bad += e != me;
          for (uint32_t i = 0U; i < n; i++) {
            bad += (q31 ? r31[i] : r15[i]) != m_x[i];
          }
          /* the inverse, from the model's spectrum */
          for (uint32_t i = 0U; i < n; i++) {
            if (q31) {
              r31[i] = (fm_q31)m_x[i];
            } else {
              r15[i] = (fm_q15)m_x[i];
            }
          }
          e = q31 ? fd_irfft_q31(r31, n) : fd_irfft_q15(r15, n);
          me = m_irfft(m_x, n, q31);
          bad += e != me;
          for (uint32_t i = 0U; i < n; i++) {
            bad += (q31 ? r31[i] : r15[i]) != m_x[i];
          }
          /* and from arbitrary bins (not a real signal's): still exact */
          fill(m_y, n / 2U, kind, q31);
          for (uint32_t i = 0U; i < n; i++) {
            if (q31) {
              r31[i] = (fm_q31)m_y[i];
            } else {
              r15[i] = (fm_q15)m_y[i];
            }
          }
          e = q31 ? fd_irfft_q31(r31, n) : fd_irfft_q15(r15, n);
          me = m_irfft(m_y, n, q31);
          bad += e != me;
          for (uint32_t i = 0U; i < n; i++) {
            bad += (q31 ? r31[i] : r15[i]) != m_y[i];
          }
          runs += 3U;
        }
      }
    }
  }
  printf("  %u transforms, %u differences, %u model overflows\n", runs, bad,
         model_overflows);
  ASSERT_EQ(bad, 0U);
  ASSERT_EQ(model_overflows, 0U);
}

/* stage inputs that reach each growth bound: every component at a corner,
 * signs aligned with the twiddles of the butterfly they feed */
static void corner_stage(uint32_t n, uint32_t q, uint32_t b, int q31,
                         double *reach) {
  const int64_t hi = (1LL << b) - 1, lo = -(1LL << b);
  /* out component o of butterfly j: maximise it, or minimise when neg */
  for (uint32_t base = 0U; base < n; base += 4U * q) {
    uint32_t j = (base / (4U * q) * 7U + 1U) % q; /* a different j per group */
    uint32_t o = (base / (4U * q)) % 8U;
    int neg = ((base / (4U * q)) / 8U) & 1U;
    for (uint32_t jj = 0U; jj < q; jj++) {
      for (uint32_t i = 0U; i < 4U; i++) {
        int64_t *v = m_x + 2U * (base + jj + i * q);
        v[0] = (int64_t)(test_rng_u32(&rng) % (uint32_t)(1U << b));
        v[1] = -(int64_t)(test_rng_u32(&rng) % (uint32_t)(1U << b));
      }
    }
    /* coefficient of each input component in output o, from the model's
     * formulas with W^0 = 1 when q == 1 */
    static const uint32_t mult[4] = {0U, 2U, 1U, 3U};
    for (uint32_t i = 0U; i < 4U; i++) {
      int64_t c = 2147483647, s = 0;
      if (i > 0U && q > 1U) {
        m_sincos(mult[i] * j * (QP / q), &c, &s);
      }
      /* P = (xr c + xi s, xi c - xr s); output o's sign per input i */
      static const int sg[8][4][2] = {
          /* re out0: +P0r +P1r +P2r +P3r  (k: [re, im] of P) */
          {{1, 0}, {1, 0}, {1, 0}, {1, 0}},
          {{0, 1}, {0, 1}, {0, 1}, {0, 1}},
          {{1, 0}, {-1, 0}, {0, 1}, {0, -1}},
          {{0, 1}, {0, -1}, {-1, 0}, {1, 0}},
          {{1, 0}, {1, 0}, {-1, 0}, {-1, 0}},
          {{0, 1}, {0, 1}, {0, -1}, {0, -1}},
          {{1, 0}, {-1, 0}, {0, -1}, {0, 1}},
          {{0, 1}, {0, -1}, {1, 0}, {-1, 0}},
      };
      double wr = sg[o][i][0], wi = sg[o][i][1];   /* out += wr Pr + wi Pi */
      double cr = wr * (double)c - wi * (double)s; /* coefficient of xr */
      double ci = wr * (double)s + wi * (double)c; /* coefficient of xi */
      if (neg) {
        cr = -cr;
        ci = -ci;
      }
      int64_t *v = m_x + 2U * (base + j + i * q);
      v[0] = cr > 0 ? hi : (cr < 0 ? lo : v[0]);
      v[1] = ci > 0 ? hi : (ci < 0 ? lo : v[1]);
    }
  }
  memcpy(m_y, m_x, sizeof(int64_t) * 2U * n);
  const int32_t hb = q31 ? 31 : 15;
  int32_t sh = (int32_t)b - (q == 1U ? hb - 2 : hb - 3);
  m_stage(m_y, n, q, sh, b, q31);
  double peak = 0.0;
  for (uint32_t i = 0U; i < 2U * n; i++) {
    peak = fmax(peak, fabs((double)m_y[i]));
  }
  *reach = fmax(*reach, peak / (q31 ? 2147483648.0 : 32768.0));
  if (q31) {
    to31(m_x, n);
    int32_t *x = &b31[0].re;
    if (sh > 0) {
      fd_fft31_t4r(x, n, q, sh);
    } else {
      fd_fft31_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (sh > 0) {
        fd_fft31_r4(x, n, q, sh);
      } else {
        fd_fft31_r4x(x, n, q, sh);
      }
    }
    ASSERT_EQ(diff31(m_y, n), 0U);
  } else {
    to15(m_x, n);
    int16_t *x = &b15[0].re;
    if (sh > 0) {
      fd_fft15_t4r(x, n, q, sh);
    } else {
      fd_fft15_t4l(x, n, q, sh);
    }
    if (q > 1U) {
      if (b <= 13U) {
        fd_fft15_r4(x, n, q, sh);
      } else {
        fd_fft15_r4w(x, n, q, sh);
      }
    }
    ASSERT_EQ(diff15(m_y, n), 0U);
  }
}

void test_fd_fft_bounds(void) {
  model_overflows = 0U;
  double reach4 = 0.0, reachr = 0.0;
  for (int q31 = 0; q31 <= 1; q31++) {
    for (uint32_t b = q31 ? 26U : 10U; b <= (q31 ? 31U : 15U); b++) {
      for (int rep = 0; rep < 8; rep++) {
        corner_stage(256U, 1U, b, q31, &reach4);
        corner_stage(256U, 4U, b, q31, &reachr);
        corner_stage(256U, 16U, b, q31, &reachr);
        corner_stage(256U, 64U, b, q31, &reachr);
        corner_stage(NMAX, NMAX / 4U, b, q31, &reachr);
      }
    }
  }
  /* the radix-4 stage without twiddles meets its bound (4 x 2^b at full
   * scale); with twiddles the bound has slack */
  printf("  peak / full scale: radix-4 %.5f, with twiddles %.5f\n", reach4,
         reachr);
  ASSERT_EQ(model_overflows, 0U);
  ASSERT_TRUE(reach4 > 0.9999);
  ASSERT_TRUE(reachr > 0.6);
  /* the radix-2 stage at its corner: 32767 + 32767 and -32768 - 32768 */
  int64_t p[8] = {32767, -32768, 32767, -32768, 32767, 32767, -32768, -32768};
  memcpy(m_y, p, sizeof p);
  m_stage(m_y, 2U, 0U, 1, 15U, 0);
  to15(p, 2U);
  fd_fft15_t2r(&b15[0].re, 2U, 1);
  ASSERT_EQ(diff15(m_y, 2U), 0U);
  ASSERT_EQ(b15[0].re, 32767);
  ASSERT_EQ(b15[0].im, -32768);
  ASSERT_EQ(model_overflows, 0U);
}

/* error against the double DFT, in LSB of the result's exponent */
static void dft_errors(uint32_t n, int kind, int q31, int real, SnrStats *snr,
                       double *maxlsb) {
  static fm_q15 r15[NMAX] __attribute__((aligned(4)));
  static fm_q31 r31[NMAX];
  uint32_t len = real ? n / 2U : n;
  fill(m_x, len, kind, q31);
  for (uint32_t i = 0U; i < n; i++) {
    dre[i] = real ? (double)m_x[i] : (double)m_x[2U * i];
    dim[i] = real ? 0.0 : (double)m_x[2U * i + 1U];
  }
  ref_fft(dre, dim, n, 0);
  int32_t e;
  uint32_t bins = real ? n / 2U : n;
  const int64_t *v;
  static int64_t out[2U * NMAX];
  if (real) {
    for (uint32_t i = 0U; i < n; i++) {
      if (q31) {
        r31[i] = (fm_q31)m_x[i];
      } else {
        r15[i] = (fm_q15)m_x[i];
      }
    }
    e = q31 ? fd_rfft_q31(r31, n) : fd_rfft_q15(r15, n);
    for (uint32_t i = 0U; i < n; i++) {
      out[i] = q31 ? r31[i] : r15[i];
    }
    v = out;
  } else if (q31) {
    to31(m_x, n);
    e = fd_cfft_q31(b31, n);
    for (uint32_t i = 0U; i < n; i++) {
      out[2U * i] = b31[i].re;
      out[2U * i + 1U] = b31[i].im;
    }
    v = out;
  } else {
    to15(m_x, n);
    e = fd_cfft_q15(b15, n);
    for (uint32_t i = 0U; i < n; i++) {
      out[2U * i] = b15[i].re;
      out[2U * i + 1U] = b15[i].im;
    }
    v = out;
  }
  const double u = ldexp(1.0, e);
  for (uint32_t k = 0U; k < bins; k++) {
    double gr = (double)v[2U * k] * u, gi = (double)v[2U * k + 1U] * u;
    double wr = dre[k], wi = dim[k];
    if (real && k == 0U) { /* slot 0: Re X[0], Re X[n/2] */
      wi = dre[n / 2U];
    }
    snr_add(snr, gr, wr);
    snr_add(snr, gi, wi);
    *maxlsb = fmax(*maxlsb, fmax(fabs(gr - wr), fabs(gi - wi)) / u);
  }
}

void test_fd_fft_accuracy(void) {
  static const uint32_t sizes[] = {16U, 64U, 256U, 1024U};
  static const struct {
    int kind;
    const char *name;
  } kinds[] = {{10, "white"}, {7, "tone"}, {9, "two tones"}, {11, "-60 dB"}};
  for (int q31 = 0; q31 <= 1; q31++) {
    for (int real = 0; real <= 1; real++) {
      for (uint32_t si = 0U; si < 4U; si++) {
        uint32_t n = sizes[si];
        if (n > NMAX) {
          continue;
        }
        for (uint32_t ki = 0U; ki < 4U; ki++) {
          SnrStats s;
          snr_init(&s, kinds[ki].name);
          double mx = 0.0;
          for (int rep = 0; rep < 20; rep++) {
            dft_errors(n, kinds[ki].kind, q31, real, &s, &mx);
          }
          printf("  %s %s %4u %-9s: %6.1f dB, max %.2f LSB\n",
                 q31 ? "q31" : "q15", real ? "rfft" : "cfft", n, kinds[ki].name,
                 snr_db(&s), mx);
          /* a stage adds at most 0.5 LSB per component at its own scale */
          ASSERT_TRUE(mx < 2.0 + 0.6 * log2((double)n));
          ASSERT_TRUE(snr_db(&s) > (q31 ? 150.0 : 55.0));
        }
      }
    }
  }
}

/* x exact multiples of 2^k: the same output, e larger by k */
void test_fd_fft_shift_invariance(void) {
  for (uint32_t n = 2U; n <= NMAX; n *= 4U) {
    for (uint32_t k = 1U; k <= 10U; k++) {
      static fd_cq15 a[NMAX];
      static fd_cq31 c[NMAX];
      fill(m_x, n, 10, 0);
      for (uint32_t i = 0U; i < n; i++) {
        b15[i].re = (fm_q15)(m_x[2U * i] >> k);
        b15[i].im = (fm_q15)(m_x[2U * i + 1U] >> k);
        a[i].re = (fm_q15)(b15[i].re * (1 << k));
        a[i].im = (fm_q15)(b15[i].im * (1 << k));
        b31[i].re = (fm_q31)(m_x[2U * i] * 1024);
        b31[i].im = (fm_q31)(m_x[2U * i + 1U] * 1024);
        c[i].re = b31[i].re / (1 << k);
        c[i].im = b31[i].im / (1 << k);
      }
      int32_t e0 = fd_cfft_q15(b15, n), e1 = fd_cfft_q15(a, n);
      ASSERT_EQ(e1, e0 + (int32_t)k);
      ASSERT_TRUE(memcmp(a, b15, n * sizeof a[0]) == 0);
      e0 = fd_cfft_q31(b31, n);
      e1 = fd_cfft_q31(c, n);
      ASSERT_EQ(e0, e1 + (int32_t)k);
      ASSERT_TRUE(memcmp(c, b31, n * sizeof c[0]) == 0);
    }
  }
}

/* round trips, and the real layout against the complex transform */
void test_fd_fft_round_trip(void) {
  static fm_q15 r15[NMAX] __attribute__((aligned(4)));
  static fm_q15 keep[NMAX];
  for (uint32_t n = 4U; n <= NMAX; n *= 2U) {
    SnrStats c, r;
    snr_init(&c, "cfft + icfft");
    snr_init(&r, "rfft + irfft");
    double d = 0.0, lsb = 0.0;
    for (int rep = 0; rep < 8; rep++) {
      fill(m_x, n, 10, 0);
      to15(m_x, n);
      int32_t e = fd_cfft_q15(b15, n);
      int32_t ei = fd_icfft_q15(b15, n);
      for (uint32_t i = 0U; i < n; i++) {
        snr_add(&c, ldexp(b15[i].re, e + ei), (double)m_x[2U * i]);
        snr_add(&c, ldexp(b15[i].im, e + ei), (double)m_x[2U * i + 1U]);
      }

      fill(m_x, n / 2U, 10, 0);
      for (uint32_t i = 0U; i < n; i++) {
        keep[i] = r15[i] = (fm_q15)m_x[i];
        b15[i].re = r15[i];
        b15[i].im = 0;
      }
      int32_t er = fd_rfft_q15(r15, n);
      int32_t ec = fd_cfft_q15(b15, n);
      for (uint32_t k = 1U; k < n / 2U; k++) {
        d = fmax(d, fabs(ldexp(r15[2U * k], er) - ldexp(b15[k].re, ec)));
        d = fmax(d, fabs(ldexp(r15[2U * k + 1U], er) - ldexp(b15[k].im, ec)));
      }
      d = fmax(d, fabs(ldexp(r15[0], er) - ldexp(b15[0].re, ec)));
      d = fmax(d, fabs(ldexp(r15[1], er) - ldexp(b15[n / 2U].re, ec)));
      lsb = fmax(lsb, ldexp(1.0, er > ec ? er : ec));
      int32_t eb = fd_irfft_q15(r15, n);
      for (uint32_t i = 0U; i < n; i++) {
        snr_add(&r, ldexp(r15[i], er + eb), (double)keep[i]);
      }
    }
    /* each direction within its accuracy row: the pair loses up to 6 dB */
    ASSERT_TRUE(snr_db(&c) > 60.0);
    ASSERT_TRUE(snr_db(&r) > 60.0);
    /* two transforms of the same data agree to their error bounds */
    ASSERT_TRUE(d <= 2.0 * (2.0 + 0.6 * log2((double)n)) * lsb);
    if (n == 256U || n == NMAX) {
      printf("  n %4u: cfft + icfft %.1f dB, rfft + irfft %.1f dB, rfft vs "
             "cfft %.2f LSB\n",
             n, snr_db(&c), snr_db(&r), d / lsb);
    }
  }
}

void test_fd_fft_edges(void) {
  /* all zero: zeros, e = 0, every entry point */
  memset(b15, 0, sizeof b15);
  memset(b31, 0, sizeof b31);
  ASSERT_EQ(fd_cfft_q15(b15, 64U), 0);
  ASSERT_EQ(fd_icfft_q15(b15, 64U), 0);
  ASSERT_EQ(fd_cfft_q31(b31, 64U), 0);
  ASSERT_EQ(fd_icfft_q31(b31, 64U), 0);
  ASSERT_EQ(fd_rfft_q15(&b15[0].re, 64U), 0);
  ASSERT_EQ(fd_irfft_q15(&b15[0].re, 64U), 0);
  ASSERT_EQ(fd_rfft_q31(&b31[0].re, 64U), 0);
  ASSERT_EQ(fd_irfft_q31(&b31[0].re, 64U), 0);
  unsigned nz = 0U;
  for (uint32_t i = 0U; i < 64U; i++) {
    nz += b15[i].re != 0 || b15[i].im != 0 || b31[i].re != 0 || b31[i].im != 0;
  }
  ASSERT_EQ(nz, 0U);
  /* a full-scale impulse: a flat spectrum at full scale */
  b15[0].re = -32768;
  int32_t e = fd_cfft_q15(b15, 64U);
  for (uint32_t i = 0U; i < 64U; i++) {
    ASSERT_NEAR(ldexp(b15[i].re, e), -32768.0, ldexp(0.5, e));
    ASSERT_NEAR(ldexp(b15[i].im, e), 0.0, ldexp(0.5, e));
  }
  /* n = 2 and 4 by hand */
  b15[0].re = 1000;
  b15[0].im = -7;
  b15[1].re = 24;
  b15[1].im = 3;
  e = fd_cfft_q15(b15, 2U);
  ASSERT_NEAR(ldexp(b15[0].re, e), 1024.0, ldexp(0.5, e));
  ASSERT_NEAR(ldexp(b15[0].im, e), -4.0, ldexp(0.5, e));
  ASSERT_NEAR(ldexp(b15[1].re, e), 976.0, ldexp(0.5, e));
  ASSERT_NEAR(ldexp(b15[1].im, e), -10.0, ldexp(0.5, e));
  b31[0].re = 1;
  b31[0].im = 0;
  b31[1].re = 0;
  b31[1].im = 1;
  b31[2].re = -1;
  b31[2].im = 0;
  b31[3].re = 0;
  b31[3].im = -1; /* e^(i 2 pi k / 4): all in bin 1 */
  e = fd_cfft_q31(b31, 4U);
  ASSERT_EQ(b31[0].re, 0);
  ASSERT_NEAR(ldexp(b31[1].re, e), 4.0, 0.0);
  ASSERT_EQ(b31[1].im, 0);
  ASSERT_EQ(b31[2].re, 0);
  ASSERT_EQ(b31[3].re, 0);
}

void test_fd_fft_rescale(void) {
  fd_cq15 v[3] = {{3, -3}, {32767, -32768}, {5, 6}};
  fd_fft_rescale_q15(v, 3U, 4, 5); /* / 2, ties up */
  ASSERT_EQ(v[0].re, 2);
  ASSERT_EQ(v[0].im, -1);
  ASSERT_EQ(v[1].re, 16384);
  ASSERT_EQ(v[1].im, -16384);
  ASSERT_EQ(v[2].re, 3);
  fd_fft_rescale_q15(v, 3U, 9, 0); /* x 512, saturated */
  ASSERT_EQ(v[0].re, 1024);
  ASSERT_EQ(v[1].re, 32767);
  ASSERT_EQ(v[1].im, -32768);
  fd_fft_rescale_q15(v, 3U, 100, 0);
  ASSERT_EQ(v[0].re, 32767);
  ASSERT_EQ(v[0].im, -32768);
  fd_fft_rescale_q15(v, 3U, -16, 0);
  ASSERT_EQ(v[0].re, 0);
  ASSERT_EQ(v[0].im, 0);
  fd_cq31 w[2] = {{INT32_MIN, 3}, {INT32_MAX, -5}};
  fd_fft_rescale_q31(w, 2U, 0, 1);
  ASSERT_EQ(w[0].re, -1073741824);
  ASSERT_EQ(w[0].im, 2);
  ASSERT_EQ(w[1].re, 1073741824);
  ASSERT_EQ(w[1].im, -2);
  fd_fft_rescale_q31(w, 2U, 40, 0);
  ASSERT_EQ(w[0].re, INT32_MIN);
  ASSERT_EQ(w[1].im, INT32_MIN);
  fd_fft_rescale_q31(w, 2U, -40, 0);
  ASSERT_EQ(w[0].re, 0);
  ASSERT_EQ(w[1].re, 0);
  /* the clamps' edges: a shift of -15 still rounds, +15 is the largest */
  fd_cq15 u[2] = {{16384, -32768}, {16383, 1}};
  fd_fft_rescale_q15(u, 2U, -15, 0);
  ASSERT_EQ(u[0].re, 1);
  ASSERT_EQ(u[0].im, -1);
  ASSERT_EQ(u[1].re, 0);
  ASSERT_EQ(u[1].im, 0);
  fd_cq15 t[1] = {{1, -1}};
  fd_fft_rescale_q15(t, 1U, 15, 0);
  ASSERT_EQ(t[0].re, 32767);
  ASSERT_EQ(t[0].im, -32768);
  fd_cq31 z[2] = {{1073741824, INT32_MIN}, {1073741823, 1}};
  fd_fft_rescale_q31(z, 2U, -31, 0);
  ASSERT_EQ(z[0].re, 1);
  ASSERT_EQ(z[0].im, -1);
  ASSERT_EQ(z[1].re, 0);
  ASSERT_EQ(z[1].im, 0);
  fd_cq31 y1[1] = {{1, -1}};
  fd_fft_rescale_q31(y1, 1U, 31, 0);
  ASSERT_EQ(y1[0].re, INT32_MAX);
  ASSERT_EQ(y1[0].im, INT32_MIN);
  /* to = log2 n: CMSIS-DSP's X / n, the rounding of the transform */
  fill(m_x, 256U, 10, 0);
  to15(m_x, 256U);
  for (uint32_t i = 0U; i < 256U; i++) {
    dre[i] = (double)m_x[2U * i];
    dim[i] = (double)m_x[2U * i + 1U];
  }
  ref_fft(dre, dim, 256U, 0);
  int32_t e = fd_cfft_q15(b15, 256U);
  static fd_cq15 before[256];
  memcpy(before, b15, sizeof before);
  fd_fft_rescale_q15(b15, 256U, e, 8);
  double mx = 0.0;
  unsigned bad = 0U;
  for (uint32_t k = 0U; k < 256U; k++) {
    double sr = floor(ldexp(before[k].re, e - 8) + 0.5);
    double si = floor(ldexp(before[k].im, e - 8) + 0.5);
    bad += b15[k].re != (fm_q15)fmin(fmax(sr, -32768.0), 32767.0);
    bad += b15[k].im != (fm_q15)fmin(fmax(si, -32768.0), 32767.0);
    mx = fmax(mx, fabs(b15[k].re - dre[k] / 256.0));
    mx = fmax(mx, fabs(b15[k].im - dim[k] / 256.0));
  }
  ASSERT_EQ(bad, 0U);
  ASSERT_TRUE(e < 8);
  ASSERT_TRUE(mx <= 0.5 + ldexp(2.0 + 0.6 * 8.0, e - 8));
}

void test_fd_fft_db(void) {
  ErrStats s15, s31;
  stats_init(&s15, "fd_fft_db_q15", "lsb");
  stats_init(&s31, "fd_fft_db_q31", "lsb");
  for (int rep = 0; rep < 20000; rep++) {
    fd_cq15 x = {(fm_q15)test_rng_u32(&rng), (fm_q15)test_rng_u32(&rng)};
    if (rep % 3 == 0) {
      x.re = (fm_q15)(x.re >> (rep % 15));
      x.im = (fm_q15)(x.im >> (rep % 13));
    }
    int32_t e = (int32_t)(test_rng_u32(&rng) % 41U) - 20;
    fm_q16 y;
    fd_fft_db_q15(&x, e, &y, 1U);
    double p = (double)x.re * x.re + (double)x.im * x.im;
    if (p == 0.0) {
      ASSERT_EQ(y, INT32_MIN);
      continue;
    }
    double want = 10.0 * log10(p / 1073741824.0) + 20.0 * e * log10(2.0);
    stats_add(&s15, p, (double)y - want * 65536.0);

    fd_cq31 w = {(fm_q31)test_rng_u32(&rng), (fm_q31)test_rng_u32(&rng)};
    if (rep % 3 == 0) {
      w.re >>= rep % 31;
      w.im >>= rep % 29;
    }
    fd_fft_db_q31(&w, e, &y, 1U);
    long double pl = (long double)w.re * w.re + (long double)w.im * w.im;
    if (pl == 0.0L) {
      ASSERT_EQ(y, INT32_MIN);
      continue;
    }
    long double wl =
        10.0L * log10l(pl / 4611686018427387904.0L) + 20.0L * e * log10l(2.0L);
    stats_add(&s31, (double)pl, (double)((long double)y - wl * 65536.0L));
  }
  ASSERT_STATS_MAX(&s15, 0.52);
  ASSERT_STATS_MAX(&s31, 0.52);
  fd_cq15 z = {0, 0};
  fm_q16 y;
  fd_fft_db_q15(&z, 0, &y, 1U);
  ASSERT_EQ(y, INT32_MIN);
  fd_cq15 f = {-32768, 0}; /* |X| = 1: 0 dB */
  fd_fft_db_q15(&f, 0, &y, 1U);
  ASSERT_EQ(y, 0);
  fd_fft_db_q15(&f, 1, &y, 1U);
  ASSERT_NEAR(y, 20.0 * log10(2.0) * 65536.0, 0.51);
  fd_fft_db_q15(&f, INT32_MAX, &y, 1U);
  ASSERT_EQ(y, INT32_MAX);
  fd_fft_db_q15(&f, INT32_MIN, &y, 1U);
  ASSERT_EQ(y, INT32_MIN + 1);
}
