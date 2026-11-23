/**
 * @file fnn_dwconv.c
 * @brief Depthwise convolution layers
 *
 * Copyright (c) 2026 Antony Kasera
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fnn_dwconv.h"
#include "fnn_quant.h"
#include "fx_arch.h"
#include <stddef.h>

static int fits(uint32_t in, uint32_t out, uint32_t k, uint32_t s, uint32_t p) {
  if (in == 0U || out == 0U || k == 0U || s == 0U || p >= k) {
    return 0;
  }
  return (uint64_t)(out - 1U) * s < (uint64_t)in + p;
}

FNN_Status fnn_dwconv_check(const FNN_DwConv *l) {
  if (!fits(l->in.h, l->out.h, l->kh, l->sh, l->pt) ||
      !fits(l->in.w, l->out.w, l->kw, l->sw, l->pl) || l->in.c == 0U ||
      l->out.c == 0U || l->out.c % l->in.c != 0U ||
      (l->q.n != 1U && l->q.n != l->out.c)) {
    return FNN_ERR_SHAPE;
  }
  if (l->zx < -128 || l->zx > 127) {
    return FNN_ERR_RANGE;
  }
  return FNN_OK;
}

void fnn_dwconv_fold_bias_s8(const int8_t *w, uint32_t taps, uint32_t c,
                             const int32_t *b, int32_t zx, int32_t *b_out) {
  for (uint32_t j = 0U; j < c; j++) {
    int32_t s = 0;
    for (uint32_t t = 0U; t < taps; t++) {
      s += w[t * c + j];
    }
    uint32_t v = (b != NULL ? (uint32_t)b[j] : 0U) - (uint32_t)zx * (uint32_t)s;
    b_out[j] = (int32_t)v;
  }
}

/* Adds four channels' products over the taps inside the input. */
static void window4(const int8_t *x, const int8_t *w, uint32_t nky,
                    uint32_t nkx, uint32_t c, uint32_t row, uint32_t wrow,
                    uint32_t *sum) {
  uint32_t a0 = sum[0], a1 = sum[1], a2 = sum[2], a3 = sum[3];
  for (uint32_t ky = 0U; ky < nky; ky++, x += row, w += wrow) {
    const int8_t *xp = x, *wp = w;
#pragma GCC unroll 3
    for (uint32_t kx = 0U; kx < nkx; kx++, xp += c, wp += c) {
      a0 += (uint32_t)(xp[0] * wp[0]);
      a1 += (uint32_t)(xp[1] * wp[1]);
      a2 += (uint32_t)(xp[2] * wp[2]);
      a3 += (uint32_t)(xp[3] * wp[3]);
    }
  }
  sum[0] = a0;
  sum[1] = a1;
  sum[2] = a2;
  sum[3] = a3;
}

/* Adds four channels' weights over taps ky0 to ky1 - 1, kx0 to kx1 - 1. */
static void wsum4(const int8_t *w, int32_t ky0, int32_t ky1, int32_t kx0,
                  int32_t kx1, uint32_t c, int32_t kw, int32_t *s) {
  for (int32_t ky = ky0; ky < ky1; ky++) {
    const int8_t *p = w + (uint32_t)(ky * kw + kx0) * c;
    for (int32_t kx = kx0; kx < kx1; kx++, p += c) {
      s[0] += p[0];
      s[1] += p[1];
      s[2] += p[2];
      s[3] += p[3];
    }
  }
}

/* Computes one output channel, the zero point standing in for the padding. */
static uint32_t window1(const FNN_DwConv *l, const int8_t *x, int32_t iy,
                        int32_t ix, uint32_t oc, uint32_t sum) {
  uint32_t ci = oc / (l->out.c / l->in.c);
  for (uint32_t ky = 0U; ky < l->kh; ky++) {
    int32_t yy = iy + (int32_t)ky;
    for (uint32_t kx = 0U; kx < l->kw; kx++) {
      int32_t xx = ix + (int32_t)kx;
      int32_t v = l->zx;
      if (yy >= 0 && yy < (int32_t)l->in.h && xx >= 0 &&
          xx < (int32_t)l->in.w) {
        v = x[((uint32_t)yy * l->in.w + (uint32_t)xx) * l->in.c + ci];
      }
      sum += (uint32_t)(v * l->w[(ky * l->kw + kx) * l->out.c + oc]);
    }
  }
  return sum;
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void fnn_dwconv_s8(const FNN_DwConv *l, const int8_t *x, int8_t *y) {
  /* Local copies, or GCC reloads them after every int8 store. */
  const int8_t *w = l->w;
  const int32_t *b = l->b, *mult = l->q.mult;
  const int8_t *rsh = l->q.rsh;
  uint32_t c = l->out.c, step = l->q.n == 1U ? 0U : 1U;
  uint32_t oh = l->out.h, ow = l->out.w, sh = l->sh, sw = l->sw;
  uint32_t row = l->in.w * l->in.c, wrow = l->kw * c, fast = c == l->in.c;
  int32_t ih = (int32_t)l->in.h, iw = (int32_t)l->in.w;
  int32_t pt = (int32_t)l->pt, pl = (int32_t)l->pl;
  int32_t kh = (int32_t)l->kh, kw = (int32_t)l->kw, zx = l->zx;
  int32_t zp = l->o.zp, lo = l->o.lo, hi = l->o.hi;
  for (uint32_t oy = 0U; oy < oh; oy++) {
    int32_t iy = (int32_t)(oy * sh) - pt;
    int32_t ky0 = clampi(-iy, 0, kh), ky1 = clampi(ih - iy, 0, kh);
    for (uint32_t ox = 0U; ox < ow; ox++, y += c) {
      int32_t ix = (int32_t)(ox * sw) - pl;
      int32_t kx0 = clampi(-ix, 0, kw), kx1 = clampi(iw - ix, 0, kw);
      int edge = ky0 > 0 || ky1 < kh || kx0 > 0 || kx1 < kw;
      uint32_t j = 0U;
      if (fast) {
        const int8_t *xw =
            x + (uint32_t)(iy + ky0) * row + (uint32_t)(ix + kx0) * c;
        const int8_t *ww = w + (uint32_t)(ky0 * kw + kx0) * c;
        for (; j + 4U <= c; j += 4U) {
          uint32_t sum[4];
          for (uint32_t k = 0U; k < 4U; k++) {
            sum[k] = b != NULL ? (uint32_t)b[j + k] : 0U;
          }
          window4(xw + j, ww + j, (uint32_t)(ky1 - ky0), (uint32_t)(kx1 - kx0),
                  c, row, wrow, sum);
          if (edge) {
            /* Taps in the padding read zx, which the folded bias expects. */
            int32_t s[4] = {0, 0, 0, 0};
            wsum4(w + j, 0, ky0, 0, kw, c, kw, s);
            wsum4(w + j, ky1, kh, 0, kw, c, kw, s);
            wsum4(w + j, ky0, ky1, 0, kx0, c, kw, s);
            wsum4(w + j, ky0, ky1, kx1, kw, c, kw, s);
            for (uint32_t k = 0U; k < 4U; k++) {
              sum[k] += (uint32_t)zx * (uint32_t)s[k];
            }
          }
          for (uint32_t k = 0U; k < 4U; k++) {
            uint32_t q = (j + k) * step;
            int32_t v = fnn_requant((int32_t)sum[k], mult[q], rsh[q]);
            y[j + k] = fnn_clamp_s8(v, zp, lo, hi);
          }
        }
      }
      for (; j < c; j++) {
        uint32_t s = b != NULL ? (uint32_t)b[j] : 0U;
        s = window1(l, x, iy, ix, j, s);
        uint32_t q = j * step;
        y[j] =
            fnn_clamp_s8(fnn_requant((int32_t)s, mult[q], rsh[q]), zp, lo, hi);
      }
    }
  }
}
