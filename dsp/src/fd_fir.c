/**
 * @file fd_fir.c
 * @brief FIR filters, decimators, interpolators and windowed-sinc design
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_fir.h"
#include "fd_fir_core.h"
#include "fm_float.h"

/* ---- decimator kernels and the one-output wide ones (the rest in
 * fd_fir_core.h) ---- */

/* q15 decimator, sum |h| < 2: windows ws apart, four outputs per pass */
static void k15d(const fm_q15 *h, uint32_t t, const fm_q15 *b, uint32_t ws,
                 fm_q15 *y, uint32_t m) {
  for (; m >= 4U; m -= 4U) {
    int32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    const fm_q15 *p0 = b, *p1 = b + ws, *p2 = b + 2U * ws, *p3 = b + 3U * ws;
    for (uint32_t k = t; k != 0U; k--) {
      int32_t c = h[k - 1U];
      a0 += c * *p0++;
      a1 += c * *p1++;
      a2 += c * *p2++;
      a3 += c * *p3++;
    }
    y[0] = out15(a0);
    y[1] = out15(a1);
    y[2] = out15(a2);
    y[3] = out15(a3);
    y += 4;
    b += 4U * ws;
  }
  for (; m != 0U; m--) {
    int32_t a = 0;
    for (uint32_t k = 0U; k < t; k++) {
      a += h[t - 1U - k] * b[k];
    }
    *y++ = out15(a);
    b += ws;
  }
}

/* q15, any gain: 64-bit sums */
static void k15w(const fm_q15 *h, uint32_t hs, uint32_t t, const fm_q15 *b,
                 uint32_t ws, fm_q15 *y, uint32_t ys, uint32_t m) {
  for (; m != 0U; m--) {
    int64_t a = 0;
    uint32_t i = (t - 1U) * hs;
    for (uint32_t k = 0U; k < t; k++) {
      a += h[i] * b[k];
      i -= hs;
    }
    *y = out15w(a);
    y += ys;
    b += ws;
  }
}

/* q31 decimator, sum |h| < 2: two outputs per pass */
static void k31d(const fm_q31 *h, uint32_t t, const fm_q31 *b, uint32_t ws,
                 fm_q31 *y, uint32_t m) {
  for (; m >= 2U; m -= 2U) {
    int64_t a0 = 0, a1 = 0;
    const fm_q31 *p0 = b, *p1 = b + ws;
    for (uint32_t k = t; k != 0U; k--) {
      int32_t c = h[k - 1U];
      a0 = fx_smlal(a0, c, *p0++);
      a1 = fx_smlal(a1, c, *p1++);
    }
    y[0] = out31(a0);
    y[1] = out31(a1);
    y += 2;
    b += 2U * ws;
  }
  if (m != 0U) {
    int64_t a = 0;
    for (uint32_t k = 0U; k < t; k++) {
      a = fx_smlal(a, h[t - 1U - k], b[k]);
    }
    *y = out31(a);
  }
}

/* q31, any gain: 96-bit sums */
static void k31w(const fm_q31 *h, uint32_t hs, uint32_t t, const fm_q31 *b,
                 uint32_t ws, fm_q31 *y, uint32_t ys, uint32_t m) {
  for (; m != 0U; m--) {
    Sum96 s = {0U, 0};
    uint32_t i = (t - 1U) * hs;
    for (uint32_t k = 0U; k < t; k++) {
      sum96_add(&s, fx_smull(h[i], b[k]));
      i -= hs;
    }
    *y = out31w(s);
    y += ys;
    b += ws;
  }
}

/* ---- state: the history, then room for new samples ---- */

static uint64_t abs_sum15(const fm_q15 *h, uint32_t hs, uint32_t n) {
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = h[i * hs];
    s += (uint32_t)(v < 0 ? -v : v);
  }
  return s;
}

static uint64_t abs_sum31(const fm_q31 *h, uint32_t hs, uint32_t n) {
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = h[i * hs];
    s += v < 0 ? 0U - (uint32_t)v : (uint32_t)v;
  }
  return s;
}

/* forward copies, so they may move the history down over itself */
static void move15(fm_q15 *d, const fm_q15 *s, uint32_t n) {
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
}

static void move31(fm_q31 *d, const fm_q31 *s, uint32_t n) {
  FX_OPAQUE(d);
  for (uint32_t i = 0U; i < n; i++) {
    d[i] = s[i];
  }
}

/* room for up to n new samples, stored; returns how many */
static uint32_t take15(FD_FirQ15 *f, const fm_q15 *x, uint32_t n) {
  if (f->w == f->len) {
    move15(f->state, f->state + f->w - (f->ntaps - 1U), f->ntaps - 1U);
    f->w = f->ntaps - 1U;
  }
  uint32_t m = f->len - f->w < n ? f->len - f->w : n;
  move15(f->state + f->w, x, m);
  return m;
}

static uint32_t take31(FD_FirQ31 *f, const fm_q31 *x, uint32_t n) {
  if (f->w == f->len) {
    move31(f->state, f->state + f->w - (f->ntaps - 1U), f->ntaps - 1U);
    f->w = f->ntaps - 1U;
  }
  uint32_t m = f->len - f->w < n ? f->len - f->w : n;
  move31(f->state + f->w, x, m);
  return m;
}

/* ---- FIR ---- */

void fd_fir_init_q15(FD_FirQ15 *f, const fm_q15 *h, uint32_t ntaps,
                     fm_q15 *state, uint32_t state_len) {
  FX_ASSERT(ntaps >= 1U && state_len >= ntaps);
  f->h = h;
  f->state = state;
  f->ntaps = ntaps;
  f->len = state_len;
  f->wide = abs_sum15(h, 1U, ntaps) >= 65536U ? 1U : 0U;
  fd_fir_reset_q15(f);
}

void fd_fir_init_q31(FD_FirQ31 *f, const fm_q31 *h, uint32_t ntaps,
                     fm_q31 *state, uint32_t state_len) {
  FX_ASSERT(ntaps >= 1U && state_len >= ntaps);
  f->h = h;
  f->state = state;
  f->ntaps = ntaps;
  f->len = state_len;
  f->wide = abs_sum31(h, 1U, ntaps) >= (1ULL << 32) ? 1U : 0U;
  fd_fir_reset_q31(f);
}

void fd_fir_reset_q15(FD_FirQ15 *f) {
  for (uint32_t i = 0U; i + 1U < f->ntaps; i++) {
    f->state[i] = 0;
  }
  f->w = f->ntaps - 1U;
}

void fd_fir_reset_q31(FD_FirQ31 *f) {
  for (uint32_t i = 0U; i + 1U < f->ntaps; i++) {
    f->state[i] = 0;
  }
  f->w = f->ntaps - 1U;
}

void fd_fir_q15(FD_FirQ15 *f, const fm_q15 *x, fm_q15 *y, uint32_t n) {
  const uint32_t t = f->ntaps;
  while (n != 0U) {
    uint32_t m = take15(f, x, n);
    const fm_q15 *b = f->state + f->w - (t - 1U);
    if (f->wide != 0U) {
      k15w2(f->h, 1U, false, t, b, y, 1U, m, 0U, 0U, 0U, NULL, 0);
    } else {
      k15(f->h, 1U, false, t, b, y, 1U, m, 0U, 0U, 0U, NULL, 0);
    }
    f->w += m;
    x += m;
    y += m;
    n -= m;
  }
}

void fd_fir_q31(FD_FirQ31 *f, const fm_q31 *x, fm_q31 *y, uint32_t n) {
  const uint32_t t = f->ntaps;
  while (n != 0U) {
    uint32_t m = take31(f, x, n);
    const fm_q31 *b = f->state + f->w - (t - 1U);
    if (f->wide != 0U) {
      k31w(f->h, 1U, t, b, 1U, y, 1U, m);
    } else {
      k31(f->h, 1U, false, t, b, y, 1U, m, 0U, 0U, 0U, NULL, 0);
    }
    f->w += m;
    x += m;
    y += m;
    n -= m;
  }
}

fm_q15 fd_fir_sample_q15(FD_FirQ15 *f, fm_q15 x) {
  fm_q15 y;
  fd_fir_q15(f, &x, &y, 1U);
  return y;
}

fm_q31 fd_fir_sample_q31(FD_FirQ31 *f, fm_q31 x) {
  fm_q31 y;
  fd_fir_q31(f, &x, &y, 1U);
  return y;
}

/* ---- decimators ---- */

void fd_fir_decim_init_q15(FD_FirDecimQ15 *d, const fm_q15 *h, uint32_t ntaps,
                           uint32_t m, fm_q15 *state, uint32_t state_len) {
  FX_ASSERT(m >= 1U);
  fd_fir_init_q15(&d->fir, h, ntaps, state, state_len);
  d->m = m;
  d->phase = 0U;
}

void fd_fir_decim_init_q31(FD_FirDecimQ31 *d, const fm_q31 *h, uint32_t ntaps,
                           uint32_t m, fm_q31 *state, uint32_t state_len) {
  FX_ASSERT(m >= 1U);
  fd_fir_init_q31(&d->fir, h, ntaps, state, state_len);
  d->m = m;
  d->phase = 0U;
}

uint32_t fd_fir_decim_q15(FD_FirDecimQ15 *d, const fm_q15 *x, fm_q15 *y,
                          uint32_t n) {
  FD_FirQ15 *f = &d->fir;
  const uint32_t t = f->ntaps, step = d->m;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t m = take15(f, x, n);
    uint32_t i0 = step - 1U - d->phase; /* the chunk's first output */
    if (i0 < m) {
      uint32_t cnt = (m - 1U - i0) / step + 1U;
      const fm_q15 *b = f->state + f->w + i0 - (t - 1U);
      if (f->wide != 0U) {
        k15w(f->h, 1U, t, b, step, y, 1U, cnt);
      } else {
        k15d(f->h, t, b, step, y, cnt);
      }
      y += cnt;
      out += cnt;
    }
    d->phase = (d->phase + m) % step;
    f->w += m;
    x += m;
    n -= m;
  }
  return out;
}

uint32_t fd_fir_decim_q31(FD_FirDecimQ31 *d, const fm_q31 *x, fm_q31 *y,
                          uint32_t n) {
  FD_FirQ31 *f = &d->fir;
  const uint32_t t = f->ntaps, step = d->m;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t m = take31(f, x, n);
    uint32_t i0 = step - 1U - d->phase;
    if (i0 < m) {
      uint32_t cnt = (m - 1U - i0) / step + 1U;
      const fm_q31 *b = f->state + f->w + i0 - (t - 1U);
      if (f->wide != 0U) {
        k31w(f->h, 1U, t, b, step, y, 1U, cnt);
      } else {
        k31d(f->h, t, b, step, y, cnt);
      }
      y += cnt;
      out += cnt;
    }
    d->phase = (d->phase + m) % step;
    f->w += m;
    x += m;
    n -= m;
  }
  return out;
}

/* ---- interpolators: branch p is h[p], h[p + l], ... ---- */

void fd_fir_interp_init_q15(FD_FirInterpQ15 *p, const fm_q15 *h, uint32_t ntaps,
                            uint32_t l, fm_q15 *state, uint32_t state_len) {
  FX_ASSERT(l >= 1U && ntaps >= 1U);
  uint32_t t = (ntaps + l - 1U) / l;
  fd_fir_init_q15(&p->fir, h, t, state, state_len);
  uint64_t g = 0U;
  for (uint32_t b = 0U; b < l && b < ntaps; b++) {
    uint64_t s = abs_sum15(h + b, l, (ntaps - b + l - 1U) / l);
    g = s > g ? s : g;
  }
  p->fir.wide = g >= 65536U ? 1U : 0U;
  p->l = l;
  p->ntaps = ntaps;
}

void fd_fir_interp_init_q31(FD_FirInterpQ31 *p, const fm_q31 *h, uint32_t ntaps,
                            uint32_t l, fm_q31 *state, uint32_t state_len) {
  FX_ASSERT(l >= 1U && ntaps >= 1U);
  uint32_t t = (ntaps + l - 1U) / l;
  fd_fir_init_q31(&p->fir, h, t, state, state_len);
  uint64_t g = 0U;
  for (uint32_t b = 0U; b < l && b < ntaps; b++) {
    uint64_t s = abs_sum31(h + b, l, (ntaps - b + l - 1U) / l);
    g = s > g ? s : g;
  }
  p->fir.wide = g >= (1ULL << 32) ? 1U : 0U;
  p->l = l;
  p->ntaps = ntaps;
}

void fd_fir_interp_q15(FD_FirInterpQ15 *p, const fm_q15 *x, fm_q15 *y,
                       uint32_t n) {
  FD_FirQ15 *f = &p->fir;
  const uint32_t t = f->ntaps, l = p->l;
  while (n != 0U) {
    uint32_t m = take15(f, x, n);
    const fm_q15 *b = f->state + f->w - (t - 1U);
    for (uint32_t br = 0U; br < l; br++) {
      uint32_t tb = br < p->ntaps ? (p->ntaps - br + l - 1U) / l : 0U;
      if (tb == 0U) {
        for (uint32_t i = 0U; i < m; i++) {
          y[br + i * l] = 0;
        }
      } else if (f->wide != 0U) {
        k15w2(f->h + br, l, false, tb, b + (t - tb), y + br, l, m, 0U, 0U, 0U,
              NULL, 0);
      } else {
        k15(f->h + br, l, false, tb, b + (t - tb), y + br, l, m, 0U, 0U, 0U,
            NULL, 0);
      }
    }
    f->w += m;
    x += m;
    y += m * l;
    n -= m;
  }
}

void fd_fir_interp_q31(FD_FirInterpQ31 *p, const fm_q31 *x, fm_q31 *y,
                       uint32_t n) {
  FD_FirQ31 *f = &p->fir;
  const uint32_t t = f->ntaps, l = p->l;
  while (n != 0U) {
    uint32_t m = take31(f, x, n);
    const fm_q31 *b = f->state + f->w - (t - 1U);
    for (uint32_t br = 0U; br < l; br++) {
      uint32_t tb = br < p->ntaps ? (p->ntaps - br + l - 1U) / l : 0U;
      if (tb == 0U) {
        for (uint32_t i = 0U; i < m; i++) {
          y[br + i * l] = 0;
        }
      } else if (f->wide != 0U) {
        k31w(f->h + br, l, tb, b + (t - tb), 1U, y + br, l, m);
      } else {
        k31(f->h + br, l, false, tb, b + (t - tb), y + br, l, m, 0U, 0U, 0U,
            NULL, 0);
      }
    }
    f->w += m;
    x += m;
    y += m * l;
    n -= m;
  }
}

/* ---- design ---- */

#define PI_F 3.14159265358979f

/* sin(2 pi f m) / (pi m), the ideal lowpass at f */
static float sinc_lp(float f, float m) {
  if (m == 0.0f) {
    return 2.0f * f;
  }
  return fm_sinf(2.0f * PI_F * f * m) / (PI_F * m);
}

static float window(FD_Window win, uint32_t k, uint32_t n) {
  static const float a[][5] = {
      {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},                     /* rectangular */
      {0.5f, 0.5f, 0.0f, 0.0f, 0.0f},                     /* Hann */
      {0.54f, 0.46f, 0.0f, 0.0f, 0.0f},                   /* Hamming */
      {0.42f, 0.5f, 0.08f, 0.0f, 0.0f},                   /* Blackman */
      {0.35875f, 0.48829f, 0.14128f, 0.01168f, 0.0f},     /* Blackman-Harris */
      {0.355768f, 0.487396f, 0.144232f, 0.012604f, 0.0f}, /* Nuttall */
      {0.21557895f, 0.41663158f, 0.277263158f, 0.083578947f, 0.006947368f},
  };
  if (n == 1U) {
    return 1.0f;
  }
  if (win == FD_WIN_BARTLETT) {
    float r = (2.0f * (float)k - (float)(n - 1U)) / (float)(n - 1U);
    return 1.0f - (r < 0.0f ? -r : r);
  }
  float t = 2.0f * PI_F * (float)k / (float)(n - 1U);
  const float *c = a[win];
  return c[0] - c[1] * fm_cosf(t) + c[2] * fm_cosf(2.0f * t) -
         c[3] * fm_cosf(3.0f * t) + c[4] * fm_cosf(4.0f * t);
}

FD_Status fd_fir_design(float *h, uint32_t ntaps, FD_Band band, float f1,
                        float f2, FD_Window win) {
  FX_ASSERT(ntaps >= 1U && (uint32_t)win <= (uint32_t)FD_WIN_BARTLETT);
  bool two = band == FD_BANDPASS || band == FD_BANDSTOP;
  if (!(f1 > 0.0f && f1 < 0.5f) || (two && !(f2 > f1 && f2 < 0.5f))) {
    return FD_ERR_RANGE;
  }
  if ((band == FD_HIGHPASS || band == FD_BANDSTOP) && (ntaps & 1U) == 0U) {
    return FD_ERR_RANGE;
  }
  const float c = (float)(ntaps - 1U) * 0.5f;
  float f0 = 0.0f; /* where the gain is set to 1 */
  if (band == FD_HIGHPASS) {
    f0 = 0.5f;
  } else if (band == FD_BANDPASS) {
    f0 = 0.5f * (f1 + f2);
  }
  float g = 0.0f;
  for (uint32_t k = 0U; k < ntaps; k++) {
    float m = (float)k - c;
    float v = sinc_lp(f1, m);
    if (band == FD_HIGHPASS) {
      v = (m == 0.0f ? 1.0f : 0.0f) - v;
    } else if (band == FD_BANDPASS) {
      v = sinc_lp(f2, m) - v;
    } else if (band == FD_BANDSTOP) {
      v = (m == 0.0f ? 1.0f : 0.0f) - (sinc_lp(f2, m) - v);
    }
    h[k] = v * window(win, k, ntaps);
    g += h[k] * fm_cosf(2.0f * PI_F * f0 * m);
  }
  if (!(g > 1e-6f || g < -1e-6f)) {
    return FD_ERR_RANGE;
  }
  float s = 1.0f / g;
  for (uint32_t k = 0U; k < ntaps; k++) {
    h[k] *= s;
  }
  return FD_OK;
}

FD_Status fd_fir_quantize_q15(const float *h, fm_q15 *q, uint32_t ntaps) {
  FD_Status st = FD_OK;
  for (uint32_t k = 0U; k < ntaps; k++) {
    if (!(h[k] >= -1.0f && h[k] < 1.0f - 0.5f / 32768.0f)) {
      st = FD_ERR_RANGE;
    }
    q[k] = fm_f32_to_q15(h[k]);
  }
  return st;
}

FD_Status fd_fir_quantize_q31(const float *h, fm_q31 *q, uint32_t ntaps) {
  FD_Status st = FD_OK;
  for (uint32_t k = 0U; k < ntaps; k++) {
    if (!(h[k] >= -1.0f && h[k] < 1.0f)) {
      st = FD_ERR_RANGE;
    }
    q[k] = fm_f32_to_q31(h[k]);
  }
  return st;
}

fm_q16 fd_fir_gain_q15(const fm_q15 *h, uint32_t ntaps) {
  uint64_t s = abs_sum15(h, 1U, ntaps) * 2U;
  return s > (uint64_t)INT32_MAX ? INT32_MAX : (fm_q16)s;
}

fm_q16 fd_fir_gain_q31(const fm_q31 *h, uint32_t ntaps) {
  uint64_t s = (abs_sum31(h, 1U, ntaps) + 16384U) >> 15;
  return s > (uint64_t)INT32_MAX ? INT32_MAX : (fm_q16)s;
}
