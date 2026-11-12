/**
 * @file fd_resample.c
 * @brief Rational resampling over the polyphase branches of fd_fir_interp
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fd_resample.h"
#include "fd_fir_core.h"

/* by residue once every residue has this many outputs in a chunk */
#define RS_GROUP 4U

void fd_resample_init_q15(FD_ResampleQ15 *r, const fm_q15 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q15 *state,
                          uint32_t state_len) {
  FX_ASSERT(m >= 1U && (uint64_t)l * state_len + m <= UINT32_MAX);
  fd_fir_interp_init_q15(&r->ip, h, ntaps, l, state, state_len);
  r->m = m;
  r->ph = 0U;
}

void fd_resample_init_q31(FD_ResampleQ31 *r, const fm_q31 *h, uint32_t ntaps,
                          uint32_t l, uint32_t m, fm_q31 *state,
                          uint32_t state_len) {
  FX_ASSERT(m >= 1U && (uint64_t)l * state_len + m <= UINT32_MAX);
  fd_fir_interp_init_q31(&r->ip, h, ntaps, l, state, state_len);
  r->m = m;
  r->ph = 0U;
}

void fd_resample_reset_q15(FD_ResampleQ15 *r) {
  fd_fir_reset_q15(&r->ip.fir);
  r->ph = 0U;
}

void fd_resample_reset_q31(FD_ResampleQ31 *r) {
  fd_fir_reset_q31(&r->ip.fir);
  r->ph = 0U;
}

/* output k is at p = ph + k m on the l-times rate: input p / l, branch p mod l
 * (t taps below r0, t - 1 from there); windows end at b[i] */
typedef struct {
  uint32_t i, br, r0, mq, mr;
} Walk;

static Walk walk(uint32_t ph, uint32_t l, uint32_t m, uint32_t ntaps,
                 uint32_t t) {
  Walk w = {ph / l, ph % l, ntaps - (t - 1U) * l, m / l, m % l};
  return w;
}

FX_INLINE void step(Walk *w, uint32_t l) {
  w->i += w->mq;
  w->br += w->mr;
  if (w->br >= l) {
    w->br -= l;
    w->i++;
  }
}

static void chunk15(const FD_ResampleQ15 *r, const fm_q15 *b, uint32_t cnt,
                    fm_q15 *y) {
  const FD_FirQ15 *f = &r->ip.fir;
  const uint32_t l = r->ip.l, m = r->m, t = f->ntaps;
  Walk w = walk(r->ph, l, m, r->ip.ntaps, t);
  if (cnt >= RS_GROUP * l) {
    const uint32_t per = cnt / l, extra = cnt % l;
    for (uint32_t q = 0U; q < l; q++) {
      uint32_t tb = w.br < w.r0 ? t : t - 1U;
      const fm_q15 *wb = b + w.i + 1U - tb;
      uint32_t c = per + (q < extra ? 1U : 0U);
      if (f->wide != 0U) {
        k15w(f->h + w.br, l, tb, wb, m, y + q, l, c);
      } else {
        k15d(f->h + w.br, l, tb, wb, m, y + q, l, c);
      }
      step(&w, l);
    }
    return;
  }
  for (; cnt != 0U; cnt--) {
    uint32_t tb = w.br < w.r0 ? t : t - 1U;
    const fm_q15 *wb = b + w.i + 1U - tb;
    const fm_q15 *h = f->h + w.br + tb * l;
    if (f->wide != 0U) {
      int64_t a = 0;
      for (uint32_t k = 0U; k < tb; k++) {
        h -= l;
        a += *h * wb[k];
      }
      *y++ = out15w(a);
    } else {
      int32_t a = 0;
      FX_UNROLL(4)
      for (uint32_t k = 0U; k < tb; k++) {
        h -= l;
        a += *h * wb[k];
      }
      *y++ = out15(a);
    }
    step(&w, l);
  }
}

static void chunk31(const FD_ResampleQ31 *r, const fm_q31 *b, uint32_t cnt,
                    fm_q31 *y) {
  const FD_FirQ31 *f = &r->ip.fir;
  const uint32_t l = r->ip.l, m = r->m, t = f->ntaps;
  Walk w = walk(r->ph, l, m, r->ip.ntaps, t);
  if (cnt >= RS_GROUP * l) {
    const uint32_t per = cnt / l, extra = cnt % l;
    for (uint32_t q = 0U; q < l; q++) {
      uint32_t tb = w.br < w.r0 ? t : t - 1U;
      const fm_q31 *wb = b + w.i + 1U - tb;
      uint32_t c = per + (q < extra ? 1U : 0U);
      if (f->wide != 0U) {
        k31w(f->h + w.br, l, tb, wb, m, y + q, l, c);
      } else {
        k31d(f->h + w.br, l, tb, wb, m, y + q, l, c);
      }
      step(&w, l);
    }
    return;
  }
  for (; cnt != 0U; cnt--) {
    uint32_t tb = w.br < w.r0 ? t : t - 1U;
    const fm_q31 *wb = b + w.i + 1U - tb;
    if (f->wide != 0U) {
      k31w(f->h + w.br, l, tb, wb, 0U, y, 1U, 1U);
    } else {
      const fm_q31 *h = f->h + w.br + tb * l;
      int64_t a = 0;
      for (uint32_t k = 0U; k < tb; k++) {
        h -= l;
        a = fx_smlal(a, *h, wb[k]);
      }
      *y = out31(a);
    }
    y++;
    step(&w, l);
  }
}

uint32_t fd_resample_q15(FD_ResampleQ15 *r, const fm_q15 *x, fm_q15 *y,
                         uint32_t n) {
  FD_FirQ15 *f = &r->ip.fir;
  const uint32_t l = r->ip.l;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t c = take15(f, x, n), span = c * l;
    if (r->ph < span) {
      uint32_t cnt = (span - 1U - r->ph) / r->m + 1U;
      chunk15(r, f->state + f->w, cnt, y);
      r->ph += cnt * r->m;
      y += cnt;
      out += cnt;
    }
    r->ph -= span;
    f->w += c;
    x += c;
    n -= c;
  }
  return out;
}

uint32_t fd_resample_q31(FD_ResampleQ31 *r, const fm_q31 *x, fm_q31 *y,
                         uint32_t n) {
  FD_FirQ31 *f = &r->ip.fir;
  const uint32_t l = r->ip.l;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t c = take31(f, x, n), span = c * l;
    if (r->ph < span) {
      uint32_t cnt = (span - 1U - r->ph) / r->m + 1U;
      chunk31(r, f->state + f->w, cnt, y);
      r->ph += cnt * r->m;
      y += cnt;
      out += cnt;
    }
    r->ph -= span;
    f->w += c;
    x += c;
    n -= c;
  }
  return out;
}

FD_Status fd_resample_design(float *h, uint32_t ntaps, uint32_t l, uint32_t m,
                             float fc, FD_Window win) {
  FX_ASSERT(l >= 1U && m >= 1U);
  if (!(fc > 0.0f && fc < 1.0f)) {
    return FD_ERR_RANGE;
  }
  const uint32_t big = l > m ? l : m;
  FD_Status st =
      fd_fir_design(h, ntaps, FD_LOWPASS, 0.5f * fc / (float)big, 0.0f, win);
  for (uint32_t k = 0U; st == FD_OK && k < ntaps; k++) {
    h[k] *= (float)l;
  }
  return st;
}

/* ---- half-band ---- */

/* m outputs: output i sums h[t - 1 - k] b[(i + k) bs] over k < t, plus
 * cz[i bs] / 2 when cz is set, rounded over 2^s into y[i ys] */
FX_INLINE fm_q15 rnd15(int32_t a, uint32_t s) {
  return fm_sat_q15((a + (1 << (s - 1U))) >> s);
}

/* q15, sum |h| < 1.5: four outputs per pass, samples rotating in registers */
FX_INLINE void hk15(const fm_q15 *h, uint32_t t, const fm_q15 *b, uint32_t bs,
                    const fm_q15 *cz, uint32_t s, fm_q15 *y, uint32_t ys,
                    uint32_t m) {
  for (; m >= 4U; m -= 4U) {
    int32_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    if (cz != NULL) {
      a0 = cz[0] * 16384;
      a1 = cz[bs] * 16384;
      a2 = cz[2U * bs] * 16384;
      a3 = cz[3U * bs] * 16384;
      cz += 4U * bs;
    }
    int32_t x0 = b[0], x1 = b[bs], x2 = b[2U * bs], x3;
    const fm_q15 *bp = b + 3U * bs, *hp = h + t;
    uint32_t k = t;
    for (; k >= 4U; k -= 4U) {
      x3 = *bp;
      bp += bs;
      MAC4(hp[-1], x0, x1, x2, x3);
      x0 = *bp;
      bp += bs;
      MAC4(hp[-2], x1, x2, x3, x0);
      x1 = *bp;
      bp += bs;
      MAC4(hp[-3], x2, x3, x0, x1);
      x2 = *bp;
      bp += bs;
      MAC4(hp[-4], x3, x0, x1, x2);
      hp -= 4;
    }
    for (; k != 0U; k--) {
      x3 = *bp;
      bp += bs;
      hp--;
      MAC4(*hp, x0, x1, x2, x3);
      x0 = x1;
      x1 = x2;
      x2 = x3;
    }
    y[0] = rnd15(a0, s);
    y[ys] = rnd15(a1, s);
    y[2U * ys] = rnd15(a2, s);
    y[3U * ys] = rnd15(a3, s);
    y += 4U * ys;
    b += 4U * bs;
  }
  for (; m != 0U; m--) {
    int32_t a = 0;
    if (cz != NULL) {
      a = *cz * 16384;
      cz += bs;
    }
    for (uint32_t k = 0U; k < t; k++) {
      a += h[t - 1U - k] * b[k * bs];
    }
    *y = rnd15(a, s);
    y += ys;
    b += bs;
  }
}

/* q15, any gain: 64-bit sums */
static void hk15w(const fm_q15 *h, uint32_t t, const fm_q15 *b, uint32_t bs,
                  const fm_q15 *cz, uint32_t s, fm_q15 *y, uint32_t ys,
                  uint32_t m) {
  for (; m != 0U; m--) {
    int64_t a = 0;
    if (cz != NULL) {
      a = *cz * 16384;
      cz += bs;
    }
    for (uint32_t k = 0U; k < t; k++) {
      a += h[t - 1U - k] * b[k * bs];
    }
    int64_t r = (a + (1 << (s - 1U))) >> s;
    *y = r > 32767 ? 32767 : (r < -32768 ? -32768 : (fm_q15)r);
    y += ys;
    b += bs;
  }
}

FX_INLINE fm_q31 rnd31(int64_t a, uint32_t s) {
  if (s == 31U) {
    return out31(a);
  }
  int64_t r = ((a >> (s - 1U)) + 1) >> 1;
  return r > INT32_MAX ? INT32_MAX : (r < INT32_MIN ? INT32_MIN : (fm_q31)r);
}

/* q31, sum |h| < 1.5: three outputs per pass */
FX_INLINE void hk31(const fm_q31 *h, uint32_t t, const fm_q31 *b, uint32_t bs,
                    const fm_q31 *cz, uint32_t s, fm_q31 *y, uint32_t ys,
                    uint32_t m) {
  for (; m >= 3U; m -= 3U) {
    int64_t a0 = 0, a1 = 0, a2 = 0;
    if (cz != NULL) {
      a0 = (int64_t)cz[0] * 1073741824;
      a1 = (int64_t)cz[bs] * 1073741824;
      a2 = (int64_t)cz[2U * bs] * 1073741824;
      cz += 3U * bs;
    }
    int32_t x0 = b[0], x1 = b[bs], x2;
    const fm_q31 *bp = b + 2U * bs, *hp = h + t;
    uint32_t k = t;
    for (; k >= 3U; k -= 3U) {
      x2 = *bp;
      bp += bs;
      SMLAL3(hp[-1], x0, x1, x2);
      x0 = *bp;
      bp += bs;
      SMLAL3(hp[-2], x1, x2, x0);
      x1 = *bp;
      bp += bs;
      SMLAL3(hp[-3], x2, x0, x1);
      hp -= 3;
    }
    for (; k != 0U; k--) {
      x2 = *bp;
      bp += bs;
      hp--;
      SMLAL3(*hp, x0, x1, x2);
      x0 = x1;
      x1 = x2;
    }
    y[0] = rnd31(a0, s);
    y[ys] = rnd31(a1, s);
    y[2U * ys] = rnd31(a2, s);
    y += 3U * ys;
    b += 3U * bs;
  }
  for (; m != 0U; m--) {
    int64_t a = 0;
    if (cz != NULL) {
      a = (int64_t)*cz * 1073741824;
      cz += bs;
    }
    for (uint32_t k = 0U; k < t; k++) {
      a = fx_smlal(a, h[t - 1U - k], b[k * bs]);
    }
    *y = rnd31(a, s);
    y += ys;
    b += bs;
  }
}

/* q31, any gain: 96-bit sums */
static void hk31w(const fm_q31 *h, uint32_t t, const fm_q31 *b, uint32_t bs,
                  const fm_q31 *cz, uint32_t s, fm_q31 *y, uint32_t ys,
                  uint32_t m) {
  for (; m != 0U; m--) {
    Sum96 a = {0U, 0};
    if (cz != NULL) {
      sum96_add(&a, (int64_t)*cz * 1073741824);
      cz += bs;
    }
    for (uint32_t k = 0U; k < t; k++) {
      sum96_add(&a, fx_smull(h[t - 1U - k], b[k * bs]));
    }
    if (s == 30U) { /* round over 2^30: twice the sum over 2^31 */
      a.hi = a.hi * 2 + (int32_t)(a.lo >> 63);
      a.lo <<= 1;
    }
    *y = out31w(a);
    y += ys;
    b += bs;
  }
}

static uint64_t hb_sum15(const fm_q15 *hb, uint32_t n) {
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = hb[i];
    s += (uint32_t)(v < 0 ? -v : v);
  }
  return s;
}

static uint64_t hb_sum31(const fm_q31 *hb, uint32_t n) {
  uint64_t s = 0U;
  for (uint32_t i = 0U; i < n; i++) {
    int32_t v = hb[i];
    s += v < 0 ? 0U - (uint32_t)v : (uint32_t)v;
  }
  return s;
}

/* the window: 4k - 1 samples (interpolator 2k), coefficients hb */
static void hb_init15(FD_HalfbandQ15 *f, const fm_q15 *hb, uint32_t k,
                      uint32_t win, fm_q15 *state, uint32_t state_len) {
  FX_ASSERT(k >= 1U && state_len >= win);
  f->fir.h = hb;
  f->fir.state = state;
  f->fir.ntaps = win;
  f->fir.len = state_len;
  f->fir.wide = hb_sum15(hb, 2U * k) >= 49152U ? 1U : 0U;
  f->k = k;
  fd_halfband_reset_q15(f);
}

static void hb_init31(FD_HalfbandQ31 *f, const fm_q31 *hb, uint32_t k,
                      uint32_t win, fm_q31 *state, uint32_t state_len) {
  FX_ASSERT(k >= 1U && state_len >= win);
  f->fir.h = hb;
  f->fir.state = state;
  f->fir.ntaps = win;
  f->fir.len = state_len;
  f->fir.wide = hb_sum31(hb, 2U * k) >= (3ULL << 30) ? 1U : 0U;
  f->k = k;
  fd_halfband_reset_q31(f);
}

void fd_halfband_decim_init_q15(FD_HalfbandQ15 *d, const fm_q15 *hb, uint32_t k,
                                fm_q15 *state, uint32_t state_len) {
  hb_init15(d, hb, k, 4U * k - 1U, state, state_len);
}

void fd_halfband_decim_init_q31(FD_HalfbandQ31 *d, const fm_q31 *hb, uint32_t k,
                                fm_q31 *state, uint32_t state_len) {
  hb_init31(d, hb, k, 4U * k - 1U, state, state_len);
}

void fd_halfband_interp_init_q15(FD_HalfbandQ15 *p, const fm_q15 *hb,
                                 uint32_t k, fm_q15 *state,
                                 uint32_t state_len) {
  hb_init15(p, hb, k, 2U * k, state, state_len);
}

void fd_halfband_interp_init_q31(FD_HalfbandQ31 *p, const fm_q31 *hb,
                                 uint32_t k, fm_q31 *state,
                                 uint32_t state_len) {
  hb_init31(p, hb, k, 2U * k, state, state_len);
}

void fd_halfband_reset_q15(FD_HalfbandQ15 *f) {
  fd_fir_reset_q15(&f->fir);
  f->phase = 0U;
}

void fd_halfband_reset_q31(FD_HalfbandQ31 *f) {
  fd_fir_reset_q31(&f->fir);
  f->phase = 0U;
}

uint32_t fd_halfband_decim_q15(FD_HalfbandQ15 *d, const fm_q15 *x, fm_q15 *y,
                               uint32_t n) {
  FD_FirQ15 *f = &d->fir;
  const uint32_t k = d->k;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t c = take15(f, x, n), i0 = 1U - d->phase;
    if (i0 < c) {
      uint32_t cnt = (c - 1U - i0) / 2U + 1U;
      const fm_q15 *b = f->state + f->w + i0 - (4U * k - 2U);
      if (f->wide != 0U) {
        hk15w(f->h, 2U * k, b, 2U, b + 2U * k - 1U, 15U, y, 1U, cnt);
      } else {
        hk15(f->h, 2U * k, b, 2U, b + 2U * k - 1U, 15U, y, 1U, cnt);
      }
      y += cnt;
      out += cnt;
    }
    d->phase = (d->phase + c) & 1U;
    f->w += c;
    x += c;
    n -= c;
  }
  return out;
}

uint32_t fd_halfband_decim_q31(FD_HalfbandQ31 *d, const fm_q31 *x, fm_q31 *y,
                               uint32_t n) {
  FD_FirQ31 *f = &d->fir;
  const uint32_t k = d->k;
  uint32_t out = 0U;
  while (n != 0U) {
    uint32_t c = take31(f, x, n), i0 = 1U - d->phase;
    if (i0 < c) {
      uint32_t cnt = (c - 1U - i0) / 2U + 1U;
      const fm_q31 *b = f->state + f->w + i0 - (4U * k - 2U);
      if (f->wide != 0U) {
        hk31w(f->h, 2U * k, b, 2U, b + 2U * k - 1U, 31U, y, 1U, cnt);
      } else {
        hk31(f->h, 2U * k, b, 2U, b + 2U * k - 1U, 31U, y, 1U, cnt);
      }
      y += cnt;
      out += cnt;
    }
    d->phase = (d->phase + c) & 1U;
    f->w += c;
    x += c;
    n -= c;
  }
  return out;
}

void fd_halfband_interp_q15(FD_HalfbandQ15 *p, const fm_q15 *x, fm_q15 *y,
                            uint32_t n) {
  FD_FirQ15 *f = &p->fir;
  const uint32_t k = p->k;
  while (n != 0U) {
    uint32_t c = take15(f, x, n);
    const fm_q15 *b = f->state + f->w - (2U * k - 1U);
    if (f->wide != 0U) {
      hk15w(f->h, 2U * k, b, 1U, NULL, 14U, y, 2U, c);
    } else {
      hk15(f->h, 2U * k, b, 1U, NULL, 14U, y, 2U, c);
    }
    const fm_q15 *cz = f->state + f->w - (k - 1U);
    for (uint32_t i = 0U; i < c; i++) {
      y[2U * i + 1U] = cz[i];
    }
    f->w += c;
    x += c;
    y += 2U * c;
    n -= c;
  }
}

void fd_halfband_interp_q31(FD_HalfbandQ31 *p, const fm_q31 *x, fm_q31 *y,
                            uint32_t n) {
  FD_FirQ31 *f = &p->fir;
  const uint32_t k = p->k;
  while (n != 0U) {
    uint32_t c = take31(f, x, n);
    const fm_q31 *b = f->state + f->w - (2U * k - 1U);
    if (f->wide != 0U) {
      hk31w(f->h, 2U * k, b, 1U, NULL, 30U, y, 2U, c);
    } else {
      hk31(f->h, 2U * k, b, 1U, NULL, 30U, y, 2U, c);
    }
    const fm_q31 *cz = f->state + f->w - (k - 1U);
    for (uint32_t i = 0U; i < c; i++) {
      y[2U * i + 1U] = cz[i];
    }
    f->w += c;
    x += c;
    y += 2U * c;
    n -= c;
  }
}

FD_Status fd_halfband_design(float *hb, uint32_t k, FD_Window win) {
  FX_ASSERT(k >= 1U && (uint32_t)win <= (uint32_t)FD_WIN_BARTLETT);
  const uint32_t n = 4U * k - 1U;
  float s = 0.0f;
  for (uint32_t j = 0U; j < 2U * k; j++) {
    /* odd offset d from the centre: sin(pi d / 2) / (pi d) = (-1)^i / (pi |d|)
     */
    uint32_t d = j < k ? 2U * (k - j) - 1U : 2U * (j - k) + 1U;
    float v = win_value(win, 2U * j, n) / (PI_F * (float)d);
    hb[j] = ((d >> 1) & 1U) != 0U ? -v : v;
    s += hb[j];
  }
  if (!(s > 1e-6f)) {
    return FD_ERR_RANGE;
  }
  const float g = 0.5f / s;
  for (uint32_t j = 0U; j < 2U * k; j++) {
    hb[j] *= g;
  }
  return FD_OK;
}
